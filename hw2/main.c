/* Lama SM Bytecode interpreter */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "gc.h"
#include "interpreter.h"
#include "./runtime/runtime.h"

size_t __gc_stack_top = 0, __gc_stack_bottom = 0;

int main(int argc, char *argv[]) {
  if (sizeof(void*) != 4 || sizeof(aint) != sizeof(size_t)) {
    perror("ERROR: only 32-bit mode is supported\n");
    exit(1);
  }
  bytefile *f = read_file(argv[1]);
  dump_file(stdout, f);
  __gc_init();
  __gc_stack_bottom = (size_t) f->global_ptr + f->global_area_size * sizeof(size_t) + sizeof(size_t);
  __gc_stack_top = (size_t) f->global_ptr - STACK_SIZE * sizeof(aint);
  interpret(stdout, f);
  free(f);
  return 0;
}
