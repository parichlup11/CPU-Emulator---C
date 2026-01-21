#include "cpu.h"

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
/*
 * Main CPU structure holding the state of the machine.
 * It contains the memory, stack pointers, registers, and flags.
 * The stack is located at the very end of the allocated memory.
 */
struct cpu
{
    int32_t *memory;       // Main memory (instructions + data)
    int32_t *stack_bottom; // Pointer to the end of memory where stack begins
    size_t stack_capacity; // Max items allowed on stack
    enum cpu_status status;// Current status (running, halted, error...)
    int32_t inst_index;    // Instruction pointer (index of next instruction)
    int32_t stack_size;    // Current number of items on the stack

    // Registers: A, B, C, D and the Result register (index 4)
    int32_t registers[5];

    // Helper variables to keep track of memory boundaries
    int32_t end_of_stack;
    size_t memory_size;
};

/*
 * Lookup table for instructions.
 * Maps the opcode number (e.g., 0x02) to the C function that executes it.
 */
struct instruction
{
    uint32_t opcode;
    int (*execute)(struct cpu *);
};

/*
 * Reads the binary program from a file and loads it into memory.
 * Since the program is binary, we read it byte-by-byte and reconstruct
 * the 32-bit integers (instructions) using bitwise operations.
 */
int32_t *cpu_create_memory(FILE *program, size_t stack_capacity, int32_t **stack_bottom)
{
    assert(program != NULL);
    assert(stack_bottom != NULL);

    int32_t *memory;
    // We allocate memory in blocks to avoid calling realloc too often.
    size_t BLOCK_SIZE = 4096 / sizeof(int32_t);
    size_t memory_size = BLOCK_SIZE;
    size_t memory_index = 0;
    int32_t ch;
    size_t cycle_index = 0;
    uint32_t result = 0;

    memory = (int32_t *) malloc(memory_size * sizeof(int32_t));
    if (memory == NULL) {
        return NULL;
    }

    // Read the file byte by byte until EOF
    while ((ch = fgetc(program)) != EOF) {

        // If the current memory block is full, allocate more space
        if (memory_index == memory_size) {
            int32_t *new_memory = realloc(memory, (memory_size + BLOCK_SIZE) * sizeof(int32_t));
            if (new_memory == NULL) {
                free(memory);
                memory = NULL;
                return NULL;
            }
            memory = new_memory;
            memory_size += BLOCK_SIZE;
        }
        /* Pack 4 bytes into one 32-bit integer */
        result |= (ch << (cycle_index * 8));
        cycle_index++;

        /* Once we have a full 32-bit word, save it to memory */
        if (cycle_index == 4) {
            memory[memory_index++] = result;
            result = 0;
            cycle_index = 0;
        }
        if (ch == EOF) {
            free(memory);
            memory = NULL;
            return NULL;
        }
    }
    assert(cycle_index == 0);
    while ((memory_index + stack_capacity) > memory_size) {
        int32_t *new_memory = realloc(memory, (memory_size + BLOCK_SIZE) * sizeof(int32_t));

        if (new_memory == NULL) {
            free(memory);
            memory = NULL;
            return NULL;
        }
        memory = new_memory;
        memory_size += BLOCK_SIZE;
    }
    memset(memory + memory_index, 0, (memory_size - memory_index * sizeof(int32_t)));
    *stack_bottom = &memory[memory_size - 1];
    return memory;
}

/*
 * Initializes the CPU structure.
 * Sets up the pointers to memory and resets registers.
 */
struct cpu *cpu_create(int32_t *memory, int32_t *stack_bottom, size_t stack_capacity)
{
    assert(stack_bottom != NULL);
    assert(memory != NULL);

    struct cpu *cpu_instance = (struct cpu *) malloc(sizeof(struct cpu));
    if (cpu_instance == NULL) {
        return NULL;
    }
    // Link memory and stack
    cpu_instance->memory = memory;
    cpu_instance->stack_bottom = stack_bottom;
    cpu_instance->stack_capacity = stack_capacity;

    // Initialize state
    cpu_instance->inst_index = 0;
    cpu_instance->status = CPU_OK;
    memset(cpu_instance->registers, 0, sizeof(cpu_instance->registers));
    cpu_instance->end_of_stack = 0;
    cpu_instance->stack_size = 0;

    // Calculate memory boundaries for safety checks later
    int32_t *stack_end = stack_bottom - stack_capacity;

    cpu_instance->end_of_stack = stack_end - cpu_instance->memory;
    cpu_instance->memory_size = ((cpu_instance->stack_bottom - cpu_instance->memory) * sizeof(int32_t));
    cpu_instance->memory_size += sizeof(int32_t);
    return cpu_instance;
}

void cpu_set_register(struct cpu *cpu, enum cpu_register reg, int32_t value)
{
    assert(reg >= REGISTER_A && reg <= REGISTER_RESULT);
    assert(cpu != NULL);

    cpu->registers[reg] = value;
}

int32_t cpu_get_register(struct cpu *cpu, enum cpu_register reg)
{

    assert(reg >= REGISTER_A && reg <= REGISTER_RESULT);
    assert(cpu != NULL);

    return cpu->registers[reg];
}

int32_t cpu_get_stack_size(struct cpu *cpu)
{
    assert(cpu != NULL);
    return cpu->stack_size;
}

void cpu_reset(struct cpu *cpu)
{
    assert(cpu != NULL);
    memset(cpu->registers, 0, sizeof(cpu->registers));
    cpu->stack_size = 0;
    cpu->status = CPU_OK;
    //int32_t *end_of_stack = cpu->stack_bottom - cpu->stack_capacity;
    size_t stack_size_bytes = cpu->stack_capacity * sizeof(int32_t);
    memset(cpu->stack_bottom - cpu->stack_capacity, 0, stack_size_bytes);
}

void cpu_destroy(struct cpu *cpu)
{
    assert(cpu != NULL);
    cpu->status = CPU_OK;

    cpu->inst_index = 0;
    cpu->end_of_stack = 0;
    cpu->inst_index = 0;
    for (int i = 0; i < 4; ++i) {
        cpu->registers[i] = 0;
    }
    cpu->registers[4] = 0;

    memset(cpu->memory, 0, cpu->memory_size);
    cpu->memory_size = 0;
    cpu->stack_size = 0;

    free(cpu->memory);
    cpu->memory = NULL;
    cpu->stack_bottom = NULL;
}

enum cpu_status cpu_get_status(struct cpu *cpu)
{
    assert(cpu != NULL);
    return cpu->status;
}

bool is_valid_reg(struct cpu *cpu, int reg)
{
    assert(cpu != NULL);
    if (reg < 0 || reg > 4) {
        cpu->status = CPU_ILLEGAL_OPERAND;
        return false;
    }
    return true;
}

int execute_nop(struct cpu *cpu)
{
    cpu->status = CPU_OK;
    return 1;
}

int execute_halt(struct cpu *cpu)
{
    cpu->status = CPU_HALTED;
    return 0;
}

int execute_add(struct cpu *cpu)
{
    int32_t reg_to_add = cpu->memory[cpu->inst_index + 1]; //value of which registr add to A
    cpu->inst_index += 1;
    if (!is_valid_reg(cpu, reg_to_add)) {
        return 0;
    }
    cpu->registers[REGISTER_A] += cpu->registers[reg_to_add];
    cpu->registers[4] = cpu->registers[0];
    cpu->status = CPU_OK;
    return 1;
}

int execute_sub(struct cpu *cpu)
{
    int32_t reg_to_sub = cpu->memory[cpu->inst_index + 1];
    cpu->inst_index += 1;
    if (!is_valid_reg(cpu, reg_to_sub)) {
        return 0;
    }
    cpu->registers[REGISTER_A] -= cpu->registers[reg_to_sub];
    cpu->registers[4] = cpu->registers[0];
    cpu->status = CPU_OK;
    return 1;
}

int execute_mul(struct cpu *cpu)
{
    int32_t reg_to_mul = cpu->memory[cpu->inst_index + 1];
    cpu->inst_index += 1;
    if (!is_valid_reg(cpu, reg_to_mul)) {
        return 0;
    }
    cpu->registers[REGISTER_A] *= cpu->registers[reg_to_mul];
    cpu->registers[4] = cpu->registers[0];
    cpu->status = CPU_OK;
    return 1;
}

int execute_div(struct cpu *cpu)
{
    int32_t reg_to_div = cpu->memory[cpu->inst_index + 1];
    cpu->inst_index += 1;
    if (!is_valid_reg(cpu, reg_to_div)) {
        return 0;
    }
    if (cpu->registers[reg_to_div] == 0) {
        cpu->status = CPU_DIV_BY_ZERO;
        return 0;
    }
    cpu->registers[REGISTER_A] /= cpu->registers[reg_to_div];
    cpu->registers[4] = cpu->registers[0];
    cpu->status = CPU_OK;
    return 1;
}

int execute_inc(struct cpu *cpu)
{
    int32_t reg_input = cpu->memory[cpu->inst_index + 1];
    cpu->inst_index += 1;
    if (!is_valid_reg(cpu, reg_input)) {
        return 0;
    }
    cpu->registers[reg_input] += 1;
    cpu->registers[4] = cpu->registers[reg_input];
    cpu->status = CPU_OK;
    return 1;
}

int execute_dec(struct cpu *cpu)
{
    int32_t reg_input = cpu->memory[cpu->inst_index + 1];
    cpu->inst_index += 1;
    if (!is_valid_reg(cpu, reg_input)) {
        return 0;
    }
    cpu->registers[reg_input] -= 1;
    cpu->registers[4] = cpu->registers[reg_input];
    cpu->status = CPU_OK;
    return 1;
}

int execute_loop(struct cpu *cpu)
{
    int32_t index_to_jump = cpu->memory[cpu->inst_index + 1];
    cpu->inst_index += 1;

    if (cpu->registers[2] != 0) {
        cpu->inst_index = index_to_jump - 1;
    }
    cpu->status = CPU_OK;
    return 1;
}

int execute_movr(struct cpu *cpu)
{
    int32_t to_reg = cpu->memory[cpu->inst_index + 1];
    uint32_t num = cpu->memory[cpu->inst_index + 2];
    cpu->inst_index += 2;
    if (!is_valid_reg(cpu, to_reg)) {
        return 0;
    }
    cpu_set_register(cpu, to_reg, num);
    cpu->status = CPU_OK;
    return 1;
}

int execute_in(struct cpu *cpu)
{
    int32_t reg_to_add = cpu->memory[cpu->inst_index + 1];
    if (!is_valid_reg(cpu, reg_to_add)) {
        return 0;
    }
    cpu->inst_index += 1;

    int32_t input;
    if (scanf("%d", &input) != 1) {
        cpu->status = CPU_IO_ERROR;
        return 0;
    }

    cpu_set_register(cpu, reg_to_add, input);
    return 1;
}

int execute_get(struct cpu *cpu)
{
    int32_t reg_to_input = cpu->memory[cpu->inst_index + 1];
    cpu->inst_index += 1;

    if (!is_valid_reg(cpu, reg_to_input)) {
        return 0;
    }

    int32_t input_value = getchar();

    if (input_value == EOF) {
        cpu_set_register(cpu, REGISTER_C, 0);
        cpu_set_register(cpu, reg_to_input, -1);
        cpu->status = CPU_OK;
        return 1;
    }
    cpu_set_register(cpu, reg_to_input, input_value);
    cpu->status = CPU_OK;
    return 1;
}

int execute_out(struct cpu *cpu)
{
    int32_t reg_to_print = cpu->memory[cpu->inst_index + 1];
    cpu->inst_index += 1;
    if (!is_valid_reg(cpu, reg_to_print)) {
        return 0;
    }

    printf("%d", cpu->registers[reg_to_print]);
    cpu->status = CPU_OK;
    return 1;
}

int execute_put(struct cpu *cpu)
{
    int32_t reg_index = cpu->memory[cpu->inst_index + 1];
    cpu->inst_index += 1;

    if (!is_valid_reg(cpu, reg_index)) {
        return 0;
    }

    int32_t reg_value = cpu->registers[reg_index];

    if (reg_value < 0 || reg_value > 255) {
        cpu->status = CPU_ILLEGAL_OPERAND;
        return 0;
    }

    printf("%c", reg_value);
    cpu->status = CPU_OK;
    return 1;
}

int execute_swap(struct cpu *cpu)
{
    int32_t reg_index_1 = cpu->memory[cpu->inst_index + 1];
    int32_t reg_index_2 = cpu->memory[cpu->inst_index + 2];

    cpu->inst_index += 2;

    if (!is_valid_reg(cpu, reg_index_1) || !is_valid_reg(cpu, reg_index_2)) {
        return 0;
    }

    int32_t temp = cpu->registers[reg_index_1];
    cpu->registers[reg_index_1] = cpu->registers[reg_index_2];
    cpu->registers[reg_index_2] = temp;

    cpu->status = CPU_OK;
    return 1;
}

int execute_load(struct cpu *cpu)
{
    int32_t reg_index = cpu->memory[cpu->inst_index + 1];
    int32_t num = cpu->memory[cpu->inst_index + 2];
    cpu->inst_index += 2;

    int32_t end = cpu_get_stack_size(cpu);
    int32_t stack_index = end - (cpu->registers[REGISTER_D] + num) - 1; //minus in D
    if (!is_valid_reg(cpu, reg_index)) {
        return 0;
    }
    if ((cpu->registers[REGISTER_D] + num) < 0) {
        cpu->status = CPU_INVALID_STACK_OPERATION;
        return 0;
    }

    if (stack_index > end || stack_index < 0 || end == 0) { //out of stack
        cpu->status = CPU_INVALID_STACK_OPERATION;
        return 0;
    }

    cpu_set_register(cpu, reg_index, cpu->stack_bottom[-stack_index]);
    cpu->status = CPU_OK;
    return 1;
}

int execute_store(struct cpu *cpu)
{
    int32_t reg_index = cpu->memory[cpu->inst_index + 1];
    int32_t num = cpu->memory[cpu->inst_index + 2];
    cpu->inst_index += 2;

    int end = cpu_get_stack_size(cpu);
    int32_t stack_index = end - cpu->registers[REGISTER_D] - num - 1;

    if (!is_valid_reg(cpu, reg_index)) {
        return 0;
    }
    if ((cpu->registers[REGISTER_D] + num) < 0) {
        cpu->status = CPU_INVALID_STACK_OPERATION;
        return 0;
    }

    if (stack_index > end || stack_index < 0 || end == 0) {
        cpu->status = CPU_INVALID_STACK_OPERATION;
        return 0;
    }
    cpu->stack_bottom[-stack_index] = cpu_get_register(cpu, reg_index);
    cpu->status = CPU_OK;
    return 1;
}

int execute_push(struct cpu *cpu)
{
    int32_t reg_index = cpu->memory[cpu->inst_index + 1];
    cpu->inst_index += 1;
    int32_t capacity = cpu->stack_capacity;
    if (!is_valid_reg(cpu, reg_index)) {
        return 0;
    }

    if (cpu_get_stack_size(cpu) >= capacity) {
        cpu->status = CPU_INVALID_STACK_OPERATION;
        return 0;
    }

    int32_t reg_value = cpu->registers[reg_index];
    cpu->stack_bottom[-(cpu_get_stack_size(cpu))] = reg_value;
    cpu->stack_size += 1;

    cpu->status = CPU_OK;
    return 1;
}

int execute_pop(struct cpu *cpu)
{
    int32_t reg_index = cpu->memory[cpu->inst_index + 1];
    cpu->inst_index += 1;
    if (!is_valid_reg(cpu, reg_index)) {
        return 0;
    }

    if (cpu_get_stack_size(cpu) <= 0) {
        cpu->status = CPU_INVALID_STACK_OPERATION;
        return 0;
    }
    int end_stack_index = -(cpu_get_stack_size(cpu) - 1);
    cpu_set_register(cpu, reg_index, cpu->stack_bottom[end_stack_index]);
    cpu->stack_bottom[end_stack_index] = 0;
    cpu->stack_size -= 1;
    cpu->status = CPU_OK;
    return 1;
}

int execute_call(struct cpu *cpu)
{
    int32_t index_to_jump = cpu->memory[cpu->inst_index + 1];
    int32_t index_to_save = cpu->memory[cpu->inst_index + 2];
    cpu->inst_index += 2;
    int32_t stack_capac = cpu->stack_capacity;
    if (cpu_get_stack_size(cpu) >= stack_capac) {
        cpu->status = CPU_INVALID_STACK_OPERATION;
        return 0;
    }
    cpu->stack_bottom[-(cpu_get_stack_size(cpu))] = index_to_save;
    cpu->stack_size += 1;
    cpu->status = CPU_OK;
    cpu->inst_index = index_to_jump - 1;
    return 1;
}

int execute_ret(struct cpu *cpu)
{
    if (cpu_get_stack_size(cpu) == 0) {
        cpu->status = CPU_INVALID_STACK_OPERATION;
        return 0;
    }
    int32_t ind_to_jump = cpu->stack_bottom[-(cpu_get_stack_size(cpu) - 1)];
    cpu->stack_bottom[-(cpu_get_stack_size(cpu) - 1)] = 0;
    cpu->inst_index = ind_to_jump - 1;
    cpu->stack_size -= 1;
    cpu->status = CPU_OK;
    return 1;
}

int execute_cmp(struct cpu *cpu)
{
    int32_t reg_index_1 = cpu->memory[cpu->inst_index + 1];
    int32_t reg_index_2 = cpu->memory[cpu->inst_index + 2];
    cpu->inst_index += 2;
    if (!is_valid_reg(cpu, reg_index_1) || !is_valid_reg(cpu, reg_index_2)) {
        return 0;
    }
    int32_t result = cpu->registers[reg_index_1] - cpu->registers[reg_index_2];
    cpu->registers[4] = result;
    cpu->status = CPU_OK;
    return 1;
}

int execute_jmp(struct cpu *cpu)
{
    int32_t index_to_jump = cpu->memory[cpu->inst_index + 1];
    cpu->inst_index += 1;
    cpu->inst_index = index_to_jump - 1;
    cpu->status = CPU_OK;
    return 1;
}

int execute_jz(struct cpu *cpu)
{
    int32_t index_to_jump = cpu->memory[cpu->inst_index + 1];
    cpu->inst_index += 1;
    if (cpu->registers[4] == 0) {
        cpu->inst_index = index_to_jump - 1;
    }
    return 1;
}

int execute_jnz(struct cpu *cpu)
{
    int32_t index_to_jump = cpu->memory[cpu->inst_index + 1];
    cpu->inst_index += 1;
    if (cpu->registers[4] != 0) {
        cpu->inst_index = index_to_jump - 1;
    }
    return 1;
}

int execute_jgt(struct cpu *cpu)
{
    int32_t index_to_jump = cpu->memory[cpu->inst_index + 1];
    cpu->inst_index += 1;
    if (cpu->registers[4] > 0) {
        cpu->inst_index = index_to_jump - 1;
    }
    return 1;
}


const struct instruction instructions[] = {
    { 0x00, execute_nop },
    { 0x01, execute_halt },
    { 0x02, execute_add },
    { 0x03, execute_sub },
    { 0x04, execute_mul },
    { 0x05, execute_div },
    { 0x06, execute_inc },
    { 0x07, execute_dec },
    { 0x08, execute_loop },
    { 0x09, execute_movr },
    { 0x0C, execute_in },
    { 0x0E, execute_out },
    { 0x0F, execute_put },
    { 0x0D, execute_get },
    { 0x0A, execute_load },
    { 0x0B, execute_store },
    { 0x11, execute_push },
    { 0x10, execute_swap },
    { 0x12, execute_pop },
    { 0x18, execute_call }, // call
    { 0x19, execute_ret },  // ret
    { 19, execute_cmp },
    { 20, execute_jmp },
    { 21, execute_jz },
    { 22, execute_jnz },
    { 23, execute_jgt },
};
/*
 * Executes a single instruction.
 * Returns 1 if successful, 0 if an error occurs.
 */
int cpu_step(struct cpu *cpu)
{
    // We can only execute if the CPU is in a valid state
    assert(cpu != NULL);
    if (cpu->status != CPU_OK) {
        return 0;
    }

    //Ensure the instruction pointer is within valid memory bounds
    if (cpu->inst_index < 0 || cpu->inst_index > cpu->end_of_stack) {
        cpu->status = CPU_INVALID_ADDRESS;
        return 0;
    }

    //Get the opcode from memory at the current position
    uint32_t instruction = cpu->memory[cpu->inst_index];
    int instruction_found = 0;

    //Find the matching opcode in our lookup table
    for (size_t i = 0; i < sizeof(instructions) / sizeof(instructions[0]); ++i) {
        if (instructions[i].opcode == instruction) {
            int executed = instructions[i].execute(cpu);
            instruction_found = 1;
            if (executed == 0) {
                return 0; // Runtime error occurred
            }
            break;
        }
    }
    if (!instruction_found) {
        cpu->status = CPU_ILLEGAL_INSTRUCTION;
        return 0;
    }
    cpu->inst_index++; // Move to next instruction
    return 1;
}

/*
 * Runs the CPU for a specified number of steps.
 * Returns the number of executed steps.
 * If an error occurs, it returns a negative number representing steps taken.
 */
long long cpu_run(struct cpu *cpu, size_t steps)
{
    assert(cpu != NULL);
    if (cpu->status != CPU_OK) {
        return 0;
    }
    long long executed_steps = 0;
    long long error_steps = 0;
    // Main execution loop
    for (size_t i = 0; i < steps; ++i) {
        int result = cpu_step(cpu);

        // If the program halted normally, we stop counting and exit    
        if (cpu->status == CPU_HALTED) {
            executed_steps += 1;
            break;
        }

        // If an error occurred (step returned 0), we record it and stop
        if (result == 0) {
            ++error_steps;
            break;
        }
        ++executed_steps;
    }

    // If we stopped due to an error, return negative count
    if (error_steps > 0) {
        return -(error_steps + executed_steps);
    }
    return executed_steps;
}
