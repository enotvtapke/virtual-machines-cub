//
// Created by enotvtapke on 10/25/25.
//

#include <cstring>
#include <string>
#include <cstdio>

#include <unordered_set>
#include <unordered_map>
#include <map>
#include <vector>
#include <algorithm>

#include "analizer.h"

#include <iostream>
#include <set>

#define EMPTY BOX(0)

static auto SEP = " | ";
static std::unordered_map<std::string, long long> opcode_freq;

void add_to_cf_graph(std::map<int64_t, std::vector<int64_t> > &cf_graph, const int64_t key, const int64_t value) {
  auto it = cf_graph.find(key);
  if (it == cf_graph.end()) {
    cf_graph[key] = std::vector<int64_t>{value};
  } else {
    it->second.push_back(value);
  }
}

#define INT (read(4))
#define BYTE (read(1))
#define STRING get_string(state.bf, INT)
#define FAIL failure("ERROR: invalid opcode %d-%d\n", h, l)

void analyze_frequencies(const bytefile * const bytefile, const unsigned int entrypoint_offset, std::set<int64_t> basic_blocks_offsets) {
  int64_t current_offset = entrypoint_offset;
  std::string prev_token = "";
  do {
    auto instruction = decodeInstruction(bytefile, current_offset);
    current_offset += instruction.length();

    std::string token = instruction.to_string(bytefile);

    opcode_freq[token]++;
    if (!prev_token.empty()) opcode_freq[prev_token + SEP + token]++;
    prev_token = token;
  } while (!basic_blocks_offsets.contains((int64_t) bytefile->code_ptr + current_offset));
}

void dfs(const bytefile * bf, const int64_t node, std::unordered_set<int64_t> &used, const std::map<int64_t, std::vector<int64_t> > &cf_graph,  std::set<int64_t> basic_blocks_offsets) {
  if (used.find(node) != used.end()) {
    return;
  }

  used.insert(node);
  // interpret(state.bf, ANALYZE_FREQUENCIES, node - (int64_t) state.bf->code_ptr);
  analyze_frequencies(bf, node - (int64_t) bf->code_ptr, basic_blocks_offsets);

  auto it = cf_graph.find(node);
  if (it != cf_graph.end()) {
    for (int64_t successor: it->second) {
      dfs(bf, successor, used, cf_graph, basic_blocks_offsets);
    }
  }
}

std::set<int64_t> find_basic_blocks(const bytefile * const bytefile) {
  std::set<int64_t> basic_blocks_offsets;
  basic_blocks_offsets.insert((int64_t) bytefile->code_ptr);

  size_t current_offset = bytefile->entrypoint_offset;
  while (current_offset < bytefile->code_size) {
    auto instruction = decodeInstruction(bytefile, current_offset);
    current_offset += instruction.length();

    if (instruction.highTag == CONST && instruction.lowTag == JMP) {
      size_t offset = instruction.args[0];
      basic_blocks_offsets.insert((int64_t) bytefile->code_ptr + current_offset);
      basic_blocks_offsets.insert((int64_t) bytefile->code_ptr + offset);
    }

    if (instruction.highTag == CONST && (instruction.lowTag == END || instruction.lowTag == RET)) {
      basic_blocks_offsets.insert((int64_t) bytefile->code_ptr + current_offset);
    }

    if (instruction.highTag == CONTROL &&
        (instruction.lowTag == CALL || instruction.lowTag == CJMPz || instruction.lowTag == CJMPnz)
    ) {
      size_t offset = instruction.args[0];
      basic_blocks_offsets.insert((int64_t) bytefile->code_ptr + current_offset);
      basic_blocks_offsets.insert((int64_t) bytefile->code_ptr + offset);
    }
  }

  return basic_blocks_offsets;
}

std::map<int64_t, std::vector<int64_t> > calculate_cfg(const bytefile *bytefile, const std::set<int64_t> &basic_blocks_offsets) {
  std::map<int64_t, std::vector<int64_t> > cf_graph;
  int64_t current_block_offset = -1;
  size_t current_offset = bytefile->entrypoint_offset;
  while (current_offset < bytefile->code_size) {
    if (basic_blocks_offsets.find((int64_t) bytefile->code_ptr + current_offset) != basic_blocks_offsets.end()) {
      DEBUG_LOG("---\n");
      current_block_offset = (int64_t) bytefile->code_ptr + current_offset;
    }
    auto instruction = decodeInstruction(bytefile, current_offset);
    current_offset += instruction.length();

    if (instruction.highTag == CONST && instruction.lowTag == JMP) {
      size_t offset = instruction.args[0];
      add_to_cf_graph(cf_graph, current_block_offset, (int64_t) bytefile->code_ptr + offset);
    }

    if (instruction.highTag == CONTROL && (instruction.lowTag == CALL || instruction.lowTag == CJMPz || instruction.lowTag == CJMPnz)) {
      size_t offset = instruction.args[0];
      add_to_cf_graph(cf_graph, current_block_offset, (int64_t) bytefile->code_ptr + current_offset);
      add_to_cf_graph(cf_graph, current_block_offset, (int64_t) bytefile->code_ptr + offset);
    }
  }
  return cf_graph;
}

void print_statistics() {
  std::vector<std::pair<std::string, long long> > op_list(opcode_freq.begin(), opcode_freq.end());
  std::ranges::sort(op_list, [](const auto &a, const auto &b) {
    if (a.second != b.second) return a.second > b.second;
    return a.first < b.first;
  });
  printf("Instruction frequencies :\n");
  for (const auto &[opcodes, freq]: op_list) {
    printf("%lld :\t%s\n", freq, opcodes.c_str());
  }
}
