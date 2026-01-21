#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
enum cpu_status
{
    CPU_OK,
    CPU_HALTED,
    CPU_ILLEGAL_INSTRUCTION,
    CPU_ILLEGAL_OPERAND,
    CPU_INVALID_ADDRESS,
    CPU_INVALID_STACK_OPERATION,
    CPU_DIV_BY_ZERO,
    CPU_IO_ERROR
};

enum cpu_register
{
    REGISTER_A,
    REGISTER_B,
    REGISTER_C,
    REGISTER_D,
    REGISTER_RESULT,
};
struct cpu;

struct cpu *cpu_create(int32_t *memory, int32_t *stack_bottom, size_t stack_capacity);

int32_t *cpu_create_memory(FILE *program, size_t stack_capacity, int32_t **stack_bottom);

int32_t cpu_get_register(struct cpu *cpu, enum cpu_register reg);

void cpu_set_register(struct cpu *cpu, enum cpu_register reg, int32_t value);

enum cpu_status cpu_get_status(struct cpu *cpu);

int32_t cpu_get_stack_size(struct cpu *cpu);

void cpu_destroy(struct cpu *cpu);

void cpu_reset(struct cpu *cpu);

long long cpu_run(struct cpu *cpu, size_t steps);

int cpu_step(struct cpu *cpu);

#endif // CPU_H
