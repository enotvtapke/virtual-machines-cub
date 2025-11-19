/* Lama SM Bytecode interpreter */

#include <algorithm>
#include <cstdint>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <iostream>

#include "analizer.h"
#include "shared.h"

#include <unordered_set>

void print_basic_block_starts(const std::vector<bool> &s) {
  printf("{");
  for (int i = 0; i < s.size(); i++) {
    if (s[i]) {
      printf("0x%.8x, ", i);
    }
  }
  printf("}\n");
}

static void analyze_file(const char *filename) {
  const bytefile *bf = read_file(filename);
  dump_file(stdout, bf);
  fprintf(stdout, "\n");

  std::vector<int32_t> public_symbols_offsets(bf->public_symbols_number);
  for (int i = 0; i < bf->public_symbols_number; i++) {
    public_symbols_offsets[i] = get_public_offset(bf, i);
  }
  traverse(bf, public_symbols_offsets);
  print_statistics(bf);
  free((bytefile *) bf);
}

int main(const int argc, char *argv[]) {
  if (sizeof(int64_t) != sizeof(size_t)) {
    perror("ERROR: adaptive int has wrong size\n");
    exit(1);
  }
  analyze_file(argv[1]);
  return 0;
}
