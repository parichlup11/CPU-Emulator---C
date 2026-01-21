#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define LABEL_UNDEFINED ((size_t)(-1))

typedef enum
{
    SUCCESS = 0,
    ERR_TOKENIZE = 1,
    ERR_PARSE = 2,
    ERR_ARGC = 3,
    ERR_LABEL_DUPLICITY = 4,
    ERR_EXTRA_TOKEN = 5,
    ERR_INVREG = 6,
    ERR_INVNUM = 7,
    ERR_LABELFORMAT = 8,
    ERR_LABEL_UNDEF = 10,
    ERR_LABEL_EMPTY = 11
} error_code;

typedef enum
{
    ARGTYPE_NONE,
    ARGTYPE_NUMBER,
    ARGTYPE_REGISTER,
    ARGTYPE_LABEL
} argtype;

typedef struct
{
    char name[10];
    argtype args[3];
    uint32_t code;
} instruction_info;

struct machinecode
{
    size_t capacity;
    size_t occupied;
    uint32_t *stream;
};

static struct machinecode machinecode;

instruction_info instruction_set[] = {
    { .name = "nop", .args = { ARGTYPE_NONE }, .code = 0x0 },
    { .name = "halt", .args = { ARGTYPE_NONE }, .code = 0x1 },
    { .name = "add", .args = { ARGTYPE_REGISTER, ARGTYPE_NONE }, .code = 0x2 },
    { .name = "sub", .args = { ARGTYPE_REGISTER, ARGTYPE_NONE }, .code = 0x3 },
    { .name = "mul", .args = { ARGTYPE_REGISTER, ARGTYPE_NONE }, .code = 0x4 },
    { .name = "div", .args = { ARGTYPE_REGISTER, ARGTYPE_NONE }, .code = 0x5 },
    { .name = "inc", .args = { ARGTYPE_REGISTER, ARGTYPE_NONE }, .code = 0x6 },
    { .name = "dec", .args = { ARGTYPE_REGISTER, ARGTYPE_NONE }, .code = 0x7 },
    { .name = "loop", .args = { ARGTYPE_LABEL, ARGTYPE_NONE }, .code = 0x8 },
    { .name = "movr",
            .args = { ARGTYPE_REGISTER, ARGTYPE_NUMBER, ARGTYPE_NONE },
            .code = 0x9 },
    { .name = "load",
            .args = { ARGTYPE_REGISTER, ARGTYPE_NUMBER, ARGTYPE_NONE },
            .code = 0xa },
    { .name = "store",
            .args = { ARGTYPE_REGISTER, ARGTYPE_NUMBER, ARGTYPE_NONE },
            .code = 0xb },
    { .name = "in", .args = { ARGTYPE_REGISTER, ARGTYPE_NONE }, .code = 0xc },
    { .name = "get", .args = { ARGTYPE_REGISTER, ARGTYPE_NONE }, .code = 0xd },
    { .name = "out", .args = { ARGTYPE_REGISTER, ARGTYPE_NONE }, .code = 0xe },
    { .name = "put", .args = { ARGTYPE_REGISTER, ARGTYPE_NONE }, .code = 0xf },
    { .name = "swap",
            .args = { ARGTYPE_REGISTER, ARGTYPE_REGISTER, ARGTYPE_NONE },
            .code = 0x10 },
    { .name = "push", .args = { ARGTYPE_REGISTER, ARGTYPE_NONE }, .code = 0x11 },
    { .name = "pop", .args = { ARGTYPE_REGISTER, ARGTYPE_NONE }, .code = 0x12 },
    { .name = "cmp",
            .args = { ARGTYPE_REGISTER, ARGTYPE_REGISTER, ARGTYPE_NONE },
            .code = 0x13 },
    { .name = "jmp", .args = { ARGTYPE_LABEL, ARGTYPE_NONE }, .code = 0x14 },
    { .name = "jz", .args = { ARGTYPE_LABEL, ARGTYPE_NONE }, .code = 0x15 },
    { .name = "jnz", .args = { ARGTYPE_LABEL, ARGTYPE_NONE }, .code = 0x16 },
    { .name = "jgt", .args = { ARGTYPE_LABEL, ARGTYPE_NONE }, .code = 0x17 },
    { .name = "call", .args = { ARGTYPE_LABEL, ARGTYPE_NONE }, .code = 0x18 },
    { .name = "ret", .args = { ARGTYPE_NONE }, .code = 0x19 },
};

static size_t machinecode_push(uint32_t word)
{
    if (machinecode.occupied + 1 >= machinecode.capacity) {
        size_t new_capacity =
                (machinecode.capacity > 0) ? machinecode.capacity * 2 : 1024;
        uint32_t *nmem = realloc(machinecode.stream, new_capacity * sizeof(*nmem));
        assert(nmem != NULL); // ! THIS IS NOT THE CORRECT WAY HOW TO CHECK !
                              // ! ALLOCATION ERRORS                        !
        machinecode.stream = nmem;
        machinecode.capacity = new_capacity;
    }

    machinecode.stream[machinecode.occupied] = word;
    return machinecode.occupied++;
}

/* Will be optimized out anyway, as pointer to first attribute
 is pointer to the struct itself, and vice versa.
 Ergo, strcmp could be used directly.
*/
inline static int instruction_cmp(const void *a, const void *b)
{
    instruction_info *ia = (instruction_info *) a;
    instruction_info *ib = (instruction_info *) b;

    return strcmp(ia->name, ib->name);
}

static instruction_info *seek_instruction(const char *name)
{
    instruction_info dummy;
    strcpy(dummy.name, name);

    return bsearch(&dummy, instruction_set,
        sizeof(instruction_set) / sizeof(instruction_set[0]),
        sizeof(instruction_set[0]), instruction_cmp);
}

static int32_t registerno(const char *regname)
{
    assert(regname != NULL);

    if (strcasecmp(regname, "result") == 0)
        return 4;

    if (strlen(regname) != 1)
        return -1;

    char reglabel = tolower(regname[0]);
    if (reglabel >= 'a' && reglabel <= 'd')
        return reglabel - 'a';

    if (reglabel == 'r')
        return 4;

    if (reglabel >= '0' && reglabel < '5')
        return reglabel - '0';

    return -1;
}

typedef struct
{
    const char *label;
    size_t *refs;
    size_t refcount;
    size_t definition;
} label_record;

typedef struct
{
    label_record *labels;
    size_t num_labels;
} label_table;

static label_table labels;

void init_label_table(void)
{
    labels.labels = calloc(1, sizeof(*labels.labels));
    assert(labels.labels != NULL);

    labels.num_labels = 0;
}

inline static int label_cmp(const void *a, const void *b)
{
    label_record *ia = (label_record *) a;
    label_record *ib = (label_record *) b;

    return strcmp(ia->label, ib->label);
}

static label_record *label_handle(const char *label_name)
{
    label_record *label = NULL;

    if (labels.labels != NULL) {
        label_record dummy = { .label = label_name };
        label = bsearch(&dummy, labels.labels, labels.num_labels, sizeof(*labels.labels), label_cmp);
    }

    if (label != NULL)
        return label;

    label_record *nlabels =
            realloc(labels.labels, sizeof(*labels.labels) * (labels.num_labels + 1));
    assert(nlabels != NULL); // ! THIS IS NOT THE CORRECT WAY HOW TO CHECK !
                             // ! ALLOCATION ERRORS                        !

    labels.labels = nlabels;
    label = labels.labels + labels.num_labels;
    labels.num_labels++;
    memset(label, 0, sizeof(*label));
    label->label = strdup(label_name);
    label->definition = LABEL_UNDEFINED;

    qsort(labels.labels, labels.num_labels, sizeof(*labels.labels), label_cmp);
    return label_handle(label_name);
}

static void label_reference(const char *label_name, size_t placeholder_pos)
{
    label_record *handle = label_handle(label_name);
    size_t *refs = realloc(handle->refs, (handle->refcount + 1) * sizeof(*refs));
    assert(refs != NULL); // ! THIS IS NOT THE CORRECT WAY HOW TO CHECK !
                          // ! ALLOCATION ERRORS                        !
    refs[handle->refcount] = placeholder_pos;
    handle->refs = refs;
    handle->refcount++;
}

static error_code define_label(const char *label_name)
{
    label_record *handle = label_handle(label_name);
    if (handle->definition != LABEL_UNDEFINED) {
        fprintf(stderr, "Duplicit label definition for %s\n", label_name);
        return ERR_LABEL_DUPLICITY;
    }

    handle->definition = machinecode.occupied;
    return SUCCESS;
}

static error_code parse_argument_register(char *token)
{
    int32_t regno = registerno(token);
    if (regno < 0) {
        fprintf(stderr, "Invalid register %s\n", token);
        return ERR_INVREG;
    }

    machinecode_push(regno);
    return SUCCESS;
}

static error_code parse_number(char *token, int32_t *value)
{
    char *endptr = NULL;
    *value = strtol(token, &endptr, 10);
    if (*endptr == 'x' && *value == 0) {
        *value = strtol(endptr + 1, &endptr, 16);
    }

    if (*endptr != '\0') {
        return ERR_INVNUM;
    }

    return SUCCESS;
}

static error_code parse_argument_number(char *token)
{
    int32_t value = 0;
    error_code rv = parse_number(token, &value);
    if (rv != SUCCESS) {
        fprintf(stderr, "Invalid number %s\n", token);
        return rv;
    }

    machinecode_push(value);
    return SUCCESS;
}

static error_code parse_argument_label(char *token)
{
    // attempt to parse as number, if not parse as label
    int32_t address = 0;
    if (parse_number(token, &address) != SUCCESS) {
        size_t placeholder_pos = machinecode_push(0xffffffff);
        label_reference(token, placeholder_pos);
    } else {
        machinecode_push(address);
    }

    return SUCCESS;
}

static error_code process_instruction(instruction_info *info)
{
    machinecode_push(info->code);
    for (argtype *arg = info->args; *arg != ARGTYPE_NONE; ++arg) {
        char *token = strtok(NULL, " \r\n\t");
        if (token == NULL) {
            fprintf(stderr, "Missing instruction argument\n");
            return ERR_ARGC;
        }

        error_code rv = SUCCESS;
        switch (*arg) {
        case ARGTYPE_REGISTER: {
            rv = parse_argument_register(token);
            break;
        }
        case ARGTYPE_NUMBER: {
            rv = parse_argument_number(token);
            break;
        }
        case ARGTYPE_LABEL: {
            rv = parse_argument_label(token);
            break;
        }
        default: {
            break;
        }
        }

        if (rv != SUCCESS)
            return rv;
    }

    char *next_token = strtok(NULL, " \r\t\n");
    if (next_token != NULL && next_token[0] != ';') {
        fprintf(stderr, "Extra token %s\n", next_token);
        return ERR_EXTRA_TOKEN;
    }

    return SUCCESS;
}

static error_code decode_label(char *label)
{
    char *colon = strchr(label, ':');
    if (colon == NULL || colon[1] != '\0' || label == colon) {
        fprintf(stderr, "Incorrectly formated label %s\n", label);
        return ERR_LABELFORMAT;
    }

    *colon = '\0';
    error_code rv = define_label(label);
    if (rv != SUCCESS)
        return rv;

    char *next_token = strtok(NULL, " \t\r\n");
    if (next_token != NULL && next_token[0] != ';') {
        fprintf(stderr, "Extra token %s\n", next_token);
        return ERR_EXTRA_TOKEN;
    }

    return SUCCESS;
}

static char *ltrim(char *line)
{
    while (isspace(*line) && *line != '\0')
        ++line;
    return line;
}

static error_code process_line(char *line)
{
    assert(line != NULL);

    line = ltrim(line);
    if (line[0] == '\0' || line[0] == ';')
        return SUCCESS;

    char *instruction_name = strtok(line, " \r\t\n");
    if (instruction_name == NULL)
        return ERR_TOKENIZE;

    instruction_info *instruction = seek_instruction(instruction_name);
    if (instruction != NULL)
        return process_instruction(instruction);

    return decode_label(instruction_name);
}

inline static void sort_instructions(void)
{
    /* qsort deployed to keep the instruction table compliant to assignment */
    qsort(instruction_set, sizeof(instruction_set) / sizeof(instruction_set[0]),
        sizeof(instruction_set[0]), instruction_cmp);
}

static error_code patch(void)
{
    // multipass implementation, quite ineffective
    for (size_t i = 0; i < labels.num_labels; ++i) {
        if (labels.labels[i].definition == LABEL_UNDEFINED) {
            fprintf(stderr, "Undefined reference to %s\n", labels.labels[i].label);
            return ERR_LABEL_UNDEF;
        }

        for (size_t j = 0; j < labels.labels[i].refcount; ++j) {
            size_t placeholder = labels.labels[i].refs[j];
            if (placeholder == machinecode.occupied) {
                fprintf(stderr, "No instruction follows label %s\n", labels.labels[i].label);
                return ERR_LABEL_EMPTY;
            }
            machinecode.stream[placeholder] = labels.labels[i].definition;
        }
    }

    return SUCCESS;
}

static void dump_stdout(void)
{
    fwrite(machinecode.stream, machinecode.occupied, sizeof(*machinecode.stream), stdout);
}

static void dump_code(void)
{
    printf("uint32_t code[] = {\n");
    for (size_t i = 0; i < machinecode.occupied;) {
        size_t block_start = i;
        const char *delimiter = "\t";
        for (; i < block_start + 8 && i < machinecode.occupied; ++i) {
            printf("%s0x%08x,", delimiter, machinecode.stream[i]);
            delimiter = " ";
        }
        printf("\n");
    }
    printf("};\n");
}

static void free_labels(void)
{
    for (size_t i = 0; i < labels.num_labels; ++i) {
        free((char *) (labels.labels[i].label)); // stripping const here is correct
        if (labels.labels[i].refcount > 0)
            free(labels.labels[i].refs);
    }

    free(labels.labels);
}

/**
 * JUST-IN-TIME compiler for the assembly, for direct use in tests.
 * @param binary - where to put the binary stream of instructions (caller is
 * responsible for freing the memory)
 * @param binary_length - where to put information about instructions length
 * @return 0 on SUCCESS, positive error code otherwise (+ nonempty stderr)
 */
int jit(FILE *sourcecode, uint32_t **binary, size_t *binary_length)
{
    error_code retval = SUCCESS;

    sort_instructions();
    init_label_table();

    char *line = NULL;
    size_t bufflen = 0;
    for (size_t lineno = 0;
            retval == SUCCESS && getline(&line, &bufflen, sourcecode) >= 0;
            ++lineno) {
        retval = process_line(line);
        if (retval != SUCCESS)
            fprintf(stderr, "Error processing line %zu\n", lineno);
    }

    if (line != NULL)
        free(line);

    if (retval == SUCCESS)
        retval = patch();

    if (labels.labels != NULL)
        free_labels();

    *binary = machinecode.stream;
    *binary_length = machinecode.occupied;

    return retval;
}

/**
 * JUST-IN-TIME compiler wrapper, exporting the binary to auxiliary file.
 *
 * For use - as:
 * ...
 * const char *source = "my_test_program.asm";
 * FILE *binary = jit_file(source);
 *
 * if (binary == NULL) {
 *     // handle error
 *     return;
 * }
 *
 * int *memory = cpu_create_memory(binary, stackCapacity, &stackBottom);
 * fclose(binary);
 * ...
 *
 * Also, creates uxiliary file with extra sufix .bin in the same directory as the source code resides.
 * As foreseen in the example above, caller is supposed to invoke fclose() upon the return value after it has been processed.
 *
 * @param filename - path to source code, an auxiliary file with same path and extra sufix .bin is created
 * @returns NULL on any error, valid FILE pointer otherwise.
 */
FILE *jit_file(const char *filename)
{
    FILE *sourcefile = fopen(filename, "r");
    if (sourcefile == NULL) {
        fprintf(stderr, "Unable to read source code from file %s\n", filename);
        return NULL;
    }

    error_code retval = jit(sourcefile, &machinecode.stream, &machinecode.occupied);
    fclose(sourcefile);
    if (retval != SUCCESS)
        return NULL;

    char *outfilename = malloc(strlen(filename) + 5);
    assert(outfilename != NULL);
    sprintf(outfilename, "%s.bin", filename);

    FILE *outfile = fopen(outfilename, "w+");
    if (outfile == NULL) {
        fprintf(stderr, "Unable to create file %s\n", outfilename);
        free(outfilename);
        return NULL;
    }
    free(outfilename);

    fwrite(machinecode.stream, machinecode.occupied, sizeof(*machinecode.stream), outfile);

    return outfile;
}

#ifndef NO_COMPILER_MAIN
int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "USAGE:\n");
        fprintf(stderr, "%s -c\n", argv[0]);
        fprintf(stderr, "\tprints binary code as C array\n");
        fprintf(stderr, "%s -o > binary.bin\n", argv[0]);
        fprintf(stderr, "\tdumps binary code to stdout, better redirect to file, "
                        "as it can harm your eyes\n");
        return EXIT_FAILURE;
    }

    void (*dumper)(void) = (strcmp("-c", argv[1]) == 0) ? dump_code : dump_stdout;

    error_code retval = jit(stdin, &machinecode.stream, &machinecode.occupied);

    if (retval == SUCCESS)
        dumper();

    if (machinecode.stream != NULL)
        free(machinecode.stream);

    return retval;
}
#endif
