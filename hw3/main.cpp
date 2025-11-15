/* Lama SM Bytecode interpreter */

#include <algorithm>
#include <cstdint>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <iostream>

#include "analizer.h"
#include "shared.h"

#include <unistd.h>
#include <unordered_set>

void print_set(const std::vector<int64_t> &s, const int64_t bf_code_ptr) {
  printf("{");
  bool first = true;
  for (int64_t value: s) {
    if (!first) {
      printf(", ");
    }
    printf("0x%.8x", value - bf_code_ptr);
    first = false;
  }
  printf("}\n");
}

void print_cf_graph(const  std::vector<std::vector<int64_t>> &graph) {
  for (int i = 0; i < graph.size(); i++) {
    std::cout << i << ": ";
    for (int64_t succ: graph[i]) {
      std::cout << succ << " ";
    }
    std::cout << "\n";
  }
}

static void interpret_file(const char *filename) {
  const bytefile *bf = read_file(filename);
  dump_file(stdout, bf);
  fprintf(stdout, "\n");
  const std::vector<int64_t> basic_blocks_offsets = find_basic_blocks(bf);
  printf("Basic blocks offsets:\n");
  print_set(basic_blocks_offsets, (int64_t) bf->code_ptr);
  printf("\n");

  auto cf_graph = calculate_cfg(bf, basic_blocks_offsets);
  printf("Control flow graph:\n");
  print_cf_graph(cf_graph);
  printf("\n");

  std::vector used(basic_blocks_offsets.size(), false);
  dfs(bf, 0, used, cf_graph, basic_blocks_offsets);
  print_statistics();
  free((bytefile *) bf);
}

int main(const int argc, char *argv[]) {
  if (sizeof(int64_t) != sizeof(size_t)) {
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
  const bytefile *f = read_file(argv[1]);
  dump_file(stdout, f);
  interpret_file(argv[1]);
  return 0;
}
