#include "cpu.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *status_name(enum cpu_status status)
{
    switch (status) {
    case CPU_OK:
        return "CPU_OK";
    case CPU_HALTED:
        return "CPU_HALTED";
    case CPU_ILLEGAL_INSTRUCTION:
        return "CPU_ILLEGAL_INSTRUCTION";
    case CPU_ILLEGAL_OPERAND:
        return "CPU_ILLEGAL_OPERAND";
    case CPU_INVALID_ADDRESS:
        return "CPU_INVALID_ADDRESS";
    case CPU_INVALID_STACK_OPERATION:
        return "CPU_INVALID_STACK_OPERATION";
    case CPU_DIV_BY_ZERO:
        return "CPU_DIV_BY_ZERO";
    case CPU_IO_ERROR:
        return "CPU_IO_ERROR";
    default:
        fprintf(stderr, "BUG: Unknown status (%d)\n", status);
        abort();
    }
    printf("\n");
}

static void state(struct cpu *cpu)
{
    printf("A: %d, B: %d, C: %d, D: %d\n", cpu_get_register(cpu, REGISTER_A),
        cpu_get_register(cpu, REGISTER_B), cpu_get_register(cpu, REGISTER_C),
        cpu_get_register(cpu, REGISTER_D));

    printf("Stack size: %d\n", cpu_get_stack_size(cpu));
#ifdef BONUS_JMP
    printf("Result: %d\n", cpu_get_register(cpu, REGISTER_RESULT));
#endif
    printf("Status: %s\n", status_name(cpu_get_status(cpu)));
}

static void usage(void)
{
    printf("Invalid arguments, run ./cpu (run|trace) [stack_capacity] FILE\n");
}

int main(int argc, char *argv[])
{
    if (argc > 4 || argc < 3) {
        usage();
        return EXIT_FAILURE;
    }

    size_t stack_capacity = 256;
    if (argc == 4) {
        char *end;
        stack_capacity = (size_t) strtol(argv[2], &end, 10);
        if (*end != '\0') {
            printf("Invalid stack capacity\n");
            return EXIT_FAILURE;
        }
        if (errno == ERANGE) {
            printf("Stack capacity out of range\n");
            return EXIT_FAILURE;
        }
    }

    FILE *fptr;
    if ((fptr = fopen(argv[argc - 1], "rb")) == NULL) {
        perror(argv[argc - 1]);
        return EXIT_FAILURE;
    }
    int32_t *stack_ptr;
    int32_t *memory = cpu_create_memory(fptr, stack_capacity, &stack_ptr);
    if (memory == NULL) {
        fprintf(stderr, "Memory failure");
        fclose(fptr);
        return EXIT_FAILURE;
    }

    struct cpu *cp = cpu_create(memory, stack_ptr, stack_capacity);
    if (cp == NULL) {
        fprintf(stderr, "Memory failure");
        free(memory);
        fclose(fptr);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "run") == 0) {
        int run_result = cpu_run(cp, INT_MAX);
        state(cp);
        printf("\'cpu_run\' result: %d\n", run_result);
    } else if (strcmp(argv[1], "trace") == 0) {
        printf("Press Enter to execute the next instruction or type 'q' to quit.\n");
        while (true) {
            int c;
            if ((c = getchar()) == '\n') {
                if (cpu_step(cp) == 0) {
                    state(cp);
                    printf("finished\n");
                    break;
                }
                state(cp);
            } else if (c == 'q') {
                break;
            }
        }
    } else {
        usage();
    }

    fclose(fptr);
    cpu_destroy(cp);
    free(cp);
    return EXIT_SUCCESS;
}
