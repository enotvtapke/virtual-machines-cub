//
// Created by enotvtapke on 10/25/25.
//

#ifndef  HW2_ANALIZER_H
#define  HW2_ANALIZER_H

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

std::vector<bool> find_basic_blocks(const bytefile * bytefile);

void traverse(
  const bytefile *bf,
  std::vector<int32_t> & stack,
  std::vector<int16_t> &used
);

void verify_and_calc_max_stack_size(const bytefile * bf, const std::vector<int16_t> &used);

void print_statistics(const bytefile * bf, const std::vector<int16_t> &used);

const char * get_string(const bytefile * f, unsigned int pos);

#endif // HW2_ANALIZER_H