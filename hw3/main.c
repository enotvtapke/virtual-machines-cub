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

static void interpret_file(const char * filename) {
  const bytefile *f = read_file(filename);
  dump_file(stdout, f);
  fprintf(stdout, "\n");
  interpret(f);
  free((bytefile *) f);
}

int main(const int argc, char *argv[]) {
  if (sizeof(aint) != sizeof(size_t)) {
    perror("ERROR: adaptive int has wrong size\n");
    exit(1);
  }
  printf("Interpreting %s\n", argv[1]);
  if (argc > 2) {
    // Redirect stdin to the input file
    if (freopen(argv[2], "r", stdin) == NULL) {
      perror("Failed to redirect stdin");
      exit(1);
    }

    setbuf(stdin, NULL);
  }
  interpret_file(argv[1]);
  return 0;
}
