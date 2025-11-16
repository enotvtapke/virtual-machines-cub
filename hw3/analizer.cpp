//
// Created by enotvtapke on 10/25/25.
//

#include <cstring>
#include <string>
#include <cstdio>

#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <vector>
#include <algorithm>

#include "analizer.h"

#include <iostream>
#include <set>

#define EMPTY BOX(0)


#define INT (read(4))
#define BYTE (read(1))
#define STRING get_string(state.bf, INT)
#define FAIL failure("ERROR: invalid opcode %d-%d\n", h, l)

std::vector<Instruction> instructions;
std::vector<std::pair<Instruction, Instruction> > instructionPairs;

void analyze_frequencies(
  const bytefile *const bytefile,
  const unsigned int entrypoint_offset,
  const int64_t basic_block_end
) {
  size_t current_offset = entrypoint_offset;
  std::optional<Instruction> prev_token = std::nullopt;
  while (basic_block_end > reinterpret_cast<int64_t>(bytefile->code_ptr) + current_offset) {
    Instruction instruction = decodeInstruction(bytefile, current_offset);
    current_offset += instruction.length();

    instructions.push_back(instruction);
    if (prev_token.has_value()) {
      instructionPairs.emplace_back(*prev_token, instruction);
    }
    prev_token = instruction;
  }
}

void traverse(
  const bytefile *bf,
  std::vector<bool> &used,
  const std::vector<std::vector<int64_t> > &cf_graph,
  const std::vector<int64_t> &basic_blocks_offsets,
  std::vector<int64_t> & stack
) {
  while (!stack.empty()) {
    int64_t node = stack.back();
    stack.pop_back();

    analyze_frequencies(bf, basic_blocks_offsets[node] - (int64_t) bf->code_ptr, basic_blocks_offsets[node + 1]);

    for (int64_t successor: cf_graph[node]) {
      if (!used[successor]) {
        used[successor] = true;
        stack.push_back(successor);
      }
    }
  }
}

std::vector<int64_t> find_basic_blocks(const bytefile *const bytefile) {
  std::vector<int64_t> basic_blocks_offsets;
  for (int i = 0; i < bytefile->public_symbols_number; i++) {
    int64_t public_offset = (int64_t) bytefile->code_ptr + get_public_offset(bytefile, i);
    basic_blocks_offsets.push_back((int64_t) public_offset);
  }

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

template<typename T>
std::vector<std::pair<T, int>> count_occurrences(std::vector<T> vec) {
  std::sort(vec.begin(), vec.end());

  std::vector<std::pair<T, int>> result;

  if (vec.empty()) {
    return result;
  }

  T current = vec[0];
  int count = 1;

  for (size_t i = 1; i < vec.size(); i++) {
    if (vec[i] == current) {
      count++;
    } else {
      result.push_back({current, count});
      current = vec[i];
      count = 1;
    }
  }
  result.push_back({current, count});

  return result;
}

void print_statistics(const bytefile * const bf) {
  auto a = count_occurrences(instructions);
  instructions.clear();
  auto b = count_occurrences(instructionPairs);
  instructionPairs.clear();

  std::ranges::sort(a, [](const auto& x, const auto& y) {
    return x.second > y.second;
  });
  std::ranges::sort(b, [](const auto& x, const auto& y) {
    return x.second > y.second;
  });

  size_t i = 0, j = 0;
  while (i < a.size() && j < b.size()) {
    if (a[i].second >= b[j].second) {
      printf("%d :\t%s\n", a[i].second, a[i].first.to_string(bf).c_str());
      i++;
    } else {
      printf("%d :\t%s\n", b[j].second, (b[j].first.first.to_string(bf) + " | " + b[j].first.second.to_string(bf)).c_str());
      j++;
    }
  }

  while (i < a.size()) {
    printf("%d :\t%s\n", a[i].second, a[i].first.to_string(bf).c_str());
    i++;
  }

  while (j < b.size()) {
    printf("%d :\t%s\n", b[j].second, (b[j].first.first.to_string(bf) + " | " + b[j].first.second.to_string(bf)).c_str());
    j++;
  }
}
