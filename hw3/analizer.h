//
// Created by enotvtapke on 10/25/25.
//

#ifndef HW2_INTERPRETER_H
#define HW2_INTERPRETER_H

#include <string>
#include <vector>

#include "shared.h"

#define STACK_SIZE 1048576
// #define DEBUG_PRINT
#ifdef DEBUG_PRINT
  #define DEBUG_LOG(...) fprintf(stdout, __VA_ARGS__)
#else
  #define DEBUG_LOG(...) (0)
#endif

const bytefile *read_file(const char *fname);

void dump_file(FILE *f, const bytefile *bf);

enum Phase {
  FIND_BASIC_BLOCKS,
  CALCULATE_CFG,
  ANALYZE_FREQUENCIES
};

std::vector<int64_t> find_basic_blocks(const bytefile * bytefile);

std::vector<std::vector<int64_t>> calculate_cfg(const bytefile *bytefile, const std::vector<int64_t> &basic_blocks_offsets);

void traverse(
  const bytefile *bf,
  std::vector<bool> &used,
  const std::vector<std::vector<int64_t>> &cf_graph,
  const std::vector<int64_t>& basic_blocks_offsets,
  std::vector<int64_t> & stack
);

void print_statistics(const bytefile * bf);

const char * get_string(const bytefile * f, unsigned int pos);

#endif //HW2_INTERPRETER_H