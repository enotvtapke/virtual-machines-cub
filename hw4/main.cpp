/* Lama SM Bytecode interpreter */

#include <algorithm>
#include <cstdint>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <iostream>

#include "analizer.h"
#include "interpreter.h"
#include "shared.h"

#include <unordered_set>

extern "C" {
  #include "gc.h"
}

void print_basic_block_starts(const std::vector<bool> &s) {
  printf("{");
  for (int i = 0; i < s.size(); i++) {
    if (s[i]) {
      printf("0x%.8x, ", i);
    }
  }
  printf("}\n");
}

static void analyze_file(const bytefile *bf) {
  std::vector<StackNode> public_symbols_offsets(bf->public_symbols_number);
  for (int i = 0; i < bf->public_symbols_number; i++) {
    const int32_t public_offset = get_public_offset(bf, i);
    public_symbols_offsets[i] = StackNode(public_offset, public_offset, 0, 0);
  }
  traverse(bf, public_symbols_offsets);
  print_statistics(bf);
}

static void interpret_file(const bytefile * const f) {
  __gc_init();
  __gc_stack_bottom = (size_t) (f->global_ptr + f->global_area_size + 1);
  __gc_stack_top = (size_t) (f->stack_ptr - 1);
  interpret(f);
}

int main(const int argc, char *argv[]) {
  if (sizeof(int64_t) != sizeof(size_t)) {
    perror("ERROR: adaptive int has wrong size\n");
    exit(1);
  }
  argv[1] = "test003.bc";
  argv[2] = "test003.input";
  if (argc > 2) {
    // Redirect stdin to the input file
    if (freopen(argv[2], "r", stdin) == NULL) {
      perror("Failed to redirect stdin");
      exit(1);
    }

    setbuf(stdin, NULL);
  }
  const bytefile *bf = read_file(argv[1]);
  dump_file(stdout, bf);
  fprintf(stdout, "\n");
  analyze_file(bf);
  interpret_file(bf);
  free((bytefile *) bf);
  return 0;
}
