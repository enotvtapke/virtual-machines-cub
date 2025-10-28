//
// Created by enotvtapke on 10/25/25.
//

#ifndef HW2_INTERPRETER_H
#define HW2_INTERPRETER_H

#include "runtime_common.h"
#include <stdio.h>

#define STACK_SIZE 1048576
// #define DEBUG_PRINT
#ifdef DEBUG_PRINT
  #define DEBUG_LOG(...) fprintf(stdout, __VA_ARGS__)
#else
  #define DEBUG_LOG(...) (0)
#endif

typedef struct {
  char *string_ptr;          // A pointer to the beginning of the string table
  int *public_ptr;           // A pointer to the beginning of publics table
  char *code_ptr;            // A pointer to the bytecode itself
  aint *global_ptr;          // A pointer to the global area
  aint *stack_ptr;           // A pointer to the stack bottom (stack grows downwards)
  long code_size;            // Code section size in bytes
  int entrypoint_offset;     // Public symbol "main" offset
  int stringtab_size;        // The size (in bytes) of the string table
  int global_area_size;      // The size (in words) of global area
  int public_symbols_number; // The number of public symbols
  char buffer[0];
} bytefile;

bytefile *read_file(char *fname);

void dump_file(FILE *f, bytefile *bf);

char *get_string(bytefile *f, int pos);

void interpret(FILE *f, bytefile *bf);

#endif //HW2_INTERPRETER_H