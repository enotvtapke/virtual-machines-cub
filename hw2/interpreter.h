//
// Created by enotvtapke on 10/25/25.
//

#ifndef HW2_INTERPRETER_H
#define HW2_INTERPRETER_H

#define STACK_SIZE 1048576

typedef struct
{
  char *string_ptr;          /* A pointer to the beginning of the string table */
  int *public_ptr;           /* A pointer to the beginning of publics table    */
  char *code_ptr;            /* A pointer to the bytecode itself               */
  int *global_ptr;           /* A pointer to the global area                   */
  int *stack_ptr;            /* A pointer to the global area                   */
  int stringtab_size;        /* The size (in bytes) of the string table        */
  int global_area_size;      /* The size (in words) of global area             */
  int public_symbols_number; /* The number of public symbols                   */
  char buffer[0];
} bytefile;

bytefile *read_file(char *fname);

void dump_file(FILE *f, bytefile *bf);

int get_public_offset(bytefile *f, int i);

char *get_public_name(bytefile *f, int i);

char *get_string(bytefile *f, int pos);

void interpret(FILE *f, bytefile *bf);

#endif //HW2_INTERPRETER_H