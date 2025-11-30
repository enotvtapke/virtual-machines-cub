/* Lama SM Bytecode interpreter */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <iostream>

#include "verifier.h"
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
  std::vector<int16_t> used(0);
  std::vector<int32_t> public_symbols_offsets(bf->public_symbols_number);
  used.resize(bf->code_size, -1);
  for (int i = 0; i < bf->public_symbols_number; i++) {
    const int32_t public_offset = get_public_offset(bf, i);
    used[public_offset] = 0;
    public_symbols_offsets[i] = public_offset;
  }
  traverse(bf, public_symbols_offsets, used);
  // print_statistics(bf, used);
  verify_and_calc_max_stack_size(bf, used);
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
  fprintf(stderr, "Interpreting %s\n", argv[1]);
  // argv[1] = "test026.bc";
  // argv[2] = "test026.input";
  if (argc > 2) {
    // Redirect stdin to the input file
    if (freopen(argv[2], "r", stdin) == NULL) {
      printf("Failed to redirect stdin to %s\n", argv[2]);
      perror("Failed to redirect stdin");
      exit(1);
    }

    setbuf(stdin, NULL);
  }
  const bytefile *bf = read_file(argv[1]);
  dump_file(stderr, bf);
  fprintf(stderr, "\n");
  auto analyze_start = std::chrono::high_resolution_clock::now();
  analyze_file(bf);
  auto analyze_end = std::chrono::high_resolution_clock::now();
  auto analyze_duration = std::chrono::duration_cast<std::chrono::microseconds>(analyze_end - analyze_start);

  auto interpret_start = std::chrono::high_resolution_clock::now();
  interpret_file(bf);
  auto interpret_end = std::chrono::high_resolution_clock::now();
  auto interpret_duration = std::chrono::duration_cast<std::chrono::microseconds>(interpret_end - interpret_start);
  fprintf(stderr, "analyze_file took %ld microseconds\n", analyze_duration.count());
  fprintf(stderr, "interpret_file took %ld microseconds\n", interpret_duration.count());
  free((bytefile *) bf);
  return 0;
}
