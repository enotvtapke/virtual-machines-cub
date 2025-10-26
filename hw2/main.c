/* Lama SM Bytecode interpreter */

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "gc.h"
#include "interpreter.h"
#include "./runtime/runtime.h"

#include <dirent.h>
#include <unistd.h>

void interpret_file(char * filename) {
  bytefile *f = read_file(filename);
  dump_file(stdout, f);
  __gc_init();
  __gc_stack_bottom = (size_t) f->global_ptr + f->global_area_size * sizeof(size_t) + sizeof(size_t);
  __gc_stack_top = __gc_stack_bottom;
  // __gc_stack_top = (size_t) f->global_ptr - STACK_SIZE * sizeof(aint);
  interpret(stdout, f);
  free(f);
}

int main(int argc, char *argv[]) {
  if (sizeof(void*) != 4 || sizeof(aint) != sizeof(size_t)) {
    perror("ERROR: only 32-bit mode is supported\n");
    exit(1);
  }

  printf("Interpreting %s\n", argv[1]);

  // Redirect stdin to the input file
  if (freopen(argv[2], "r", stdin) == NULL) {
    perror("Failed to redirect stdin");
    exit(1);
  }

  setbuf(stdin, NULL);

  interpret_file(argv[1]);

  return 0;
}
