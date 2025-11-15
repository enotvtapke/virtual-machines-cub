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

#define INT (read(4))
#define BYTE (read(1))
#define STRING get_string(state.bf, INT)
#define FAIL failure("ERROR: invalid opcode %d-%d\n", h, l)

void analyze_frequencies(
  const bytefile *const bytefile,
  const unsigned int entrypoint_offset,
  const int64_t basic_block_end
) {
  int64_t current_offset = entrypoint_offset;
  std::string prev_token = "";
  while (basic_block_end > (int64_t) bytefile->code_ptr + current_offset) {
    auto instruction = decodeInstruction(bytefile, current_offset);
    current_offset += instruction.length();

    std::string token = instruction.to_string(bytefile);

    opcode_freq[token]++;
    if (!prev_token.empty()) opcode_freq[prev_token + SEP + token]++;
    prev_token = token;
  }
}

void dfs(
  const bytefile *bf, const int64_t node,
  std::vector<bool> &used,
  const std::vector<std::vector<int64_t> > &cf_graph,
  std::vector<int64_t> basic_blocks_offsets
) {
  if (used[node]) return;

  used[node] = true;
  analyze_frequencies(bf, basic_blocks_offsets[node] - (int64_t) bf->code_ptr, basic_blocks_offsets[node + 1]);
  for (int64_t successor: cf_graph[node]) {
    dfs(bf, successor, used, cf_graph, basic_blocks_offsets);
  }
}

std::vector<int64_t> find_basic_blocks(const bytefile *const bytefile) {
  std::vector<int64_t> basic_blocks_offsets;
  basic_blocks_offsets.push_back((int64_t) bytefile->code_ptr);

  size_t current_offset = bytefile->entrypoint_offset;
  while (current_offset < bytefile->code_size) {
    auto instruction = decodeInstruction(bytefile, current_offset);
    current_offset += instruction.length();

    if (instruction.highTag == CONST && instruction.lowTag == JMP) {
      size_t offset = instruction.args[0];
      basic_blocks_offsets.push_back((int64_t) bytefile->code_ptr + current_offset);
      basic_blocks_offsets.push_back((int64_t) bytefile->code_ptr + offset);
    }

    if (instruction.highTag == CONST && (instruction.lowTag == END || instruction.lowTag == RET)) {
      basic_blocks_offsets.push_back((int64_t) bytefile->code_ptr + current_offset);
    }

    if (instruction.highTag == CONTROL &&
        (instruction.lowTag == CALL || instruction.lowTag == CJMPz || instruction.lowTag == CJMPnz)
    ) {
      size_t offset = instruction.args[0];
      basic_blocks_offsets.push_back((int64_t) bytefile->code_ptr + current_offset);
      basic_blocks_offsets.push_back((int64_t) bytefile->code_ptr + offset);
    }
  }

  std::sort(basic_blocks_offsets.begin(), basic_blocks_offsets.end());

  auto last = std::unique(basic_blocks_offsets.begin(), basic_blocks_offsets.end());
  basic_blocks_offsets.erase(last, basic_blocks_offsets.end());

  return basic_blocks_offsets;
}

int64_t index_of(const std::vector<int64_t> &basic_blocks_offsets, int64_t value) {
  auto it = std::lower_bound(basic_blocks_offsets.begin(), basic_blocks_offsets.end(), value);

  if (it == basic_blocks_offsets.end() || *it != value) {
    throw std::runtime_error("Element not found in basic_blocks_offsets");
  }

  return it - basic_blocks_offsets.begin();
}

std::vector<std::vector<int64_t> > calculate_cfg(
  const bytefile *bytefile,
  const std::vector<int64_t> &basic_blocks_offsets
) {
  int index = 0;

  std::vector<std::vector<int64_t> > cf_graph(basic_blocks_offsets.size());
  size_t current_offset = bytefile->entrypoint_offset;
  while (current_offset < bytefile->code_size) {
    auto instruction = decodeInstruction(bytefile, current_offset);
    current_offset += instruction.length();

    if (instruction.highTag == CONST && instruction.lowTag == JMP) {
      size_t offset = instruction.args[0];
      cf_graph[index].push_back(index_of(basic_blocks_offsets, (int64_t) bytefile->code_ptr + offset));
    }

    if (instruction.highTag == CONTROL && (instruction.lowTag == CALL || instruction.lowTag == CJMPz || instruction.
                                           lowTag == CJMPnz)) {
      size_t offset = instruction.args[0];
      cf_graph[index].push_back(index_of(basic_blocks_offsets, (int64_t) bytefile->code_ptr + current_offset));
      cf_graph[index].push_back(index_of(basic_blocks_offsets, (int64_t) bytefile->code_ptr + offset));
    }
    if (basic_blocks_offsets[index + 1] == (int64_t) bytefile->code_ptr + current_offset) {
      DEBUG_LOG("---\n");
      index++;
    } else if (index + 1 < basic_blocks_offsets.size() &&
               basic_blocks_offsets[index + 1] < (int64_t) bytefile->code_ptr + current_offset) {
      std::cout << "ERROR: basic block offset is not sorted\n";
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
