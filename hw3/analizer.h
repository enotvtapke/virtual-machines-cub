//
// Created by enotvtapke on 10/25/25.
//

#ifndef HW2_INTERPRETER_H
#define HW2_INTERPRETER_H

#include <stdio.h>
#include <string>
#include <unordered_set>

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

void find_basic_blocks(const bytefile *bf);

void calculate_cfg(const bytefile *bytefile);

void dfs(int64_t node, std::unordered_set<int64_t> &used);

void interpret(const bytefile *bf, Phase phase, unsigned int entrypoint_offset);

void print_statistics();

const char * get_string(const bytefile * f, const unsigned int pos);

#endif //HW2_INTERPRETER_H