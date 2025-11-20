#include <cstring>
#include <string>
#include <cstdio>

#include <vector>
#include <algorithm>

#include "analizer.h"

#include <iostream>

struct __attribute__((packed)) InstructionView {
  int32_t offset;
  int8_t length;
};

std::vector<InstructionView> instructionViews;

void traverse(
  const bytefile *bf,
  std::vector<int32_t> & stack
) {
  std::vector used(bf->code_size, false);
  while (!stack.empty()) {
    const int64_t node = stack.back();
    stack.pop_back();

    int32_t prev_offset = -1;
    size_t current_offset = node;
    do {
      auto instruction = decodeInstruction(bf, current_offset);
      used[current_offset] = true;
      size_t length = instruction.length();
      instructionViews.push_back({(int32_t) current_offset, (int8_t) length});
      current_offset += length;

      if (prev_offset != -1) {
        instructionViews.push_back({prev_offset , (int8_t) (current_offset - prev_offset)});
      }
      prev_offset = current_offset - length;

      if (instruction.highTag == CONST && instruction.lowTag == JMP) {
        size_t offset = instruction.args[0];
        if (!used[offset]) stack.push_back(offset);
        prev_offset = -1;
        break;
      }

      if (instruction.highTag == CONTROL && (instruction.lowTag == CALL || instruction.lowTag == CJMPz || instruction.
                                             lowTag == CJMPnz)) {
        size_t offset = instruction.args[0];
        if (!used[offset]) stack.push_back(offset);
        if (!used[offset]) stack.push_back(current_offset);
        prev_offset = -1;
        break;
      }

      if (instruction.highTag == CONST && (instruction.lowTag == END || instruction.lowTag == RET)) {
        prev_offset = -1;
        break;
      }
    } while (true);
  }
}

template<typename T>
std::vector<std::pair<T, int>> count_occurrences(std::vector<T> vec, auto comparator) {
  std::sort(vec.begin(), vec.end(), comparator);

  std::vector<std::pair<T, int>> result;

  if (vec.empty()) {
    return result;
  }

  T current = vec[0];
  int count = 1;

  for (size_t i = 1; i < vec.size(); i++) {
    if (!comparator(vec[i], current) && !comparator(current, vec[i])) {
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
  auto compare = [bf](const InstructionView &i1, const InstructionView &i2) {
    if (i1.length != i2.length) return i1.length < i2.length;
    for (int i = 0; i < i1.length; i++) {
      if ((bf->code_ptr + i1.offset)[i] != (bf->code_ptr + i2.offset)[i]) {
        return (bf->code_ptr + i1.offset)[i] < (bf->code_ptr + i2.offset)[i];
      }
    }
    return false;
  };
  auto instr_views_occurrences = count_occurrences(instructionViews, compare);
  instructionViews.clear();

  std::ranges::sort(instr_views_occurrences, [](const auto& x, const auto& y) {
    return x.second > y.second;
  });
  for (const auto& [instr, count] : instr_views_occurrences) {
    printf("%d :\t", count);
    auto decoded_instr = decodeInstruction(bf, instr.offset);
    printf("%s", decoded_instr.to_string(bf).c_str());
    const size_t length = decoded_instr.length();
    if (instr.length != length) {
      printf(" | %s", decodeInstruction(bf, instr.offset + length).to_string(bf).c_str());
    }
    printf("\n");
  }
}
