==================== 32-BIT CPU EMULATOR ====================

A lightweight virtual machine implementation written in C (C99). This project 
simulates a simplified stack-based processor architecture featuring a custom 
instruction set, dynamic memory management, and a complete fetch-decode-execute 
cycle.

CORE FEATURES:
- Architecture: 32-bit system with 4 general-purpose registers (A-D) + Stack.
- Instruction Set: Arithmetic, Logic, I/O, Stack manipulation, and Control Flow.
- Assembler: Includes a downloaded compiler to convert Assembly (.asm) to binary.
- Debugging: 'Trace' mode allows step-by-step execution to visualize the
  state of registers and stack memory in real-time.

BUILDING:
$ make

USAGE:
1. Compile assembly code to binary:
   $ ./compiler -o < source.asm > program.bin

2. Run the emulator (Run mode):
   $ ./cpu run program.bin

3. Debug the execution (Trace mode):
   $ ./cpu trace program.bin
