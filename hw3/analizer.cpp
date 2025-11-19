//
// Created by enotvtapke on 10/25/25.
//

#include <cstring>
#include <string>
#include <cstdio>

#include <vector>
#include <algorithm>

#include "analizer.h"

#include <iostream>

std::vector<Instruction> instructions;
std::vector<std::pair<Instruction, Instruction> > instructionPairs;

void traverse(
  const bytefile *bf,
  std::vector<int32_t> & stack
) {
  std::vector used(bf->code_size, false);
  while (!stack.empty()) {
    const int64_t node = stack.back();
    stack.pop_back();

    std::optional<Instruction> prev_token = std::nullopt;
    size_t current_offset = node;
    do {
      auto instruction = decodeInstruction(bf, current_offset);
      used[current_offset] = true;
      current_offset += instruction.length();

      instructions.push_back(instruction);
      if (prev_token.has_value()) {
        instructionPairs.emplace_back(*prev_token, instruction);
      }
      prev_token = instruction;

      if (instruction.highTag == CONST && instruction.lowTag == JMP) {
        size_t offset = instruction.args[0];
        if (!used[offset]) stack.push_back(offset);
        break;
      }

      if (instruction.highTag == CONTROL && (instruction.lowTag == CALL || instruction.lowTag == CJMPz || instruction.
                                             lowTag == CJMPnz)) {
        size_t offset = instruction.args[0];
        if (!used[offset]) stack.push_back(offset);
        if (!used[offset]) stack.push_back(current_offset);
        break;
      }

      if (instruction.highTag == CONST && (instruction.lowTag == END || instruction.lowTag == RET)) {
        break;
      }
    } while (true);
  }
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
  auto instr_occurrences = count_occurrences(instructions);
  instructions.clear();
  auto instr_pair_occurrences = count_occurrences(instructionPairs);
  instructionPairs.clear();

  std::ranges::sort(instr_occurrences, [](const auto& x, const auto& y) {
    return x.second > y.second;
  });
  std::ranges::sort(instr_pair_occurrences, [](const auto& x, const auto& y) {
    return x.second > y.second;
  });

  size_t i = 0, j = 0;
  while (i < instr_occurrences.size() && j < instr_pair_occurrences.size()) {
    if (instr_occurrences[i].second >= instr_pair_occurrences[j].second) {
      printf("%d :\t%s\n", instr_occurrences[i].second, instr_occurrences[i].first.to_string(bf).c_str());
      i++;
    } else {
      printf("%d :\t%s\n", instr_pair_occurrences[j].second, (instr_pair_occurrences[j].first.first.to_string(bf) + " | " + instr_pair_occurrences[j].first.second.to_string(bf)).c_str());
      j++;
    }
  }

  while (i < instr_occurrences.size()) {
    printf("%d :\t%s\n", instr_occurrences[i].second, instr_occurrences[i].first.to_string(bf).c_str());
    i++;
  }

  while (j < instr_pair_occurrences.size()) {
    printf("%d :\t%s\n", instr_pair_occurrences[j].second, (instr_pair_occurrences[j].first.first.to_string(bf) + " | " + instr_pair_occurrences[j].first.second.to_string(bf)).c_str());
    j++;
  }
}
