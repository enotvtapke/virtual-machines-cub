//
// Created by enotvtapke on 10/25/25.
//

#ifndef HW2_INTERPRETER_H
#define HW2_INTERPRETER_H

#include <stdio.h>
#include <string>
#include <unordered_set>

#define STACK_SIZE 1048576
// #define DEBUG_PRINT
#ifdef DEBUG_PRINT
  #define DEBUG_LOG(...) fprintf(stdout, __VA_ARGS__)
#else
  #define DEBUG_LOG(...) (0)
#endif

typedef struct {
  char *string_ptr;          // A pointer to the beginning of the string table
  int32_t *public_ptr;       // A pointer to the beginning of publics table
  char *code_ptr;            // A pointer to the bytecode itself
  int64_t *global_ptr;          // A pointer to the global area
  unsigned long code_size;            // Code section size in bytes
  unsigned int entrypoint_offset;     // Public symbol "main" offset
  unsigned int stringtab_size;        // The size (in bytes) of the string table
  unsigned int global_area_size;      // The size (in words) of global area
  unsigned int public_symbols_number; // The number of public symbols
  char buffer[0];
} bytefile;

const bytefile *read_file(const char *fname);

void dump_file(FILE *f, const bytefile *bf);

const char *get_string(const bytefile *f, unsigned int pos);

enum Phase {
  FIND_BASIC_BLOCKS,
  CALCULATE_CFG,
  ANALYZE_FREQUENCIES
};

void dfs(int64_t node, std::unordered_set<int64_t> &used);

void interpret(const bytefile *bf, Phase phase, unsigned int entrypoint_offset);

void print_statistics();

#define va_start(v,l)	__builtin_va_start(v,l)

static void vfailure (const std::string &s, va_list args) {
  fprintf(stderr, "*** FAILURE: ");
  vfprintf(stderr, s.data(), args);
  exit(255);
}

static void failure (const std::string &s, ...) {
  va_list args;

  va_start(args, s);
  vfailure(s, args);
}

#endif //HW2_INTERPRETER_H