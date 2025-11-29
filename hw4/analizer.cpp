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

void traverse(
  const bytefile *bf,
  std::vector<StackNode> &stack,
  std::vector<int16_t> &used
) {
  while (!stack.empty()) {
    const StackNode node = stack.back();
    stack.pop_back();
    printf("=====\n");

    int32_t current_offset = node.offset;
    int16_t max_stack = node.max_stack;
    int16_t current_stack = used[node.offset];
    do {
      used[current_offset] = current_stack;
      auto instruction = decodeInstruction(bf, current_offset);
      printf("%s %d %d:\t%s\n", hex8(current_offset).c_str(), current_stack, max_stack,
             instruction.to_string(bf).c_str());

      current_stack += instruction.stack_diff();
      if (current_stack < 0) {
        throw std::runtime_error("Invalid stack size");
      }
      max_stack = std::max(max_stack, current_stack);

      const int8_t length = instruction.length();
      current_offset += length;

      if (instruction.highTag == CONST && instruction.lowTag == JMP) {
        int32_t offset = instruction.args[0];
        if (used[offset] == -1) {
          used[offset] = current_stack;
          stack.push_back(StackNode{offset, node.begin_offset, max_stack});
        } else if (used[offset] != current_stack) {
          throw std::runtime_error("Stack size mismatch");
        }
        break;
      }

      if (instruction.highTag == CONTROL && (instruction.lowTag == CJMPz || instruction.
                                             lowTag == CJMPnz)) {
        int32_t offset = instruction.args[0];
        if (used[offset] == -1) {
          used[offset] = current_stack;
          stack.push_back(StackNode{offset, node.begin_offset, max_stack});
        } else if (used[offset] != current_stack) {
          throw std::runtime_error("Stack size mismatch");
        }

        if (used[current_offset] == -1) {
          used[current_offset] = current_stack;
          stack.push_back(StackNode{current_offset, node.begin_offset, max_stack});
        } else if (used[current_offset] != current_stack) {
          throw std::runtime_error("Stack size mismatch");
        }
        break;
      }

      if (instruction.highTag == CONTROL && instruction.lowTag == CALL) {
        int32_t offset = instruction.args[0];
        if (used[offset] == -1) {
          used[offset] = 0;
          stack.push_back(StackNode{offset, node.begin_offset, 0});
        } else if (used[offset] != 0) {
          throw std::runtime_error("Stack size mismatch");
        }

        if (used[current_offset] == -1) {
          used[current_offset] = current_stack;
          stack.push_back(StackNode{current_offset, node.begin_offset, max_stack});
        } else if (used[current_offset] != current_stack) {
          throw std::runtime_error("Stack size mismatch");
        }
        break;
      }

      if (instruction.highTag == CONST && (instruction.lowTag == END || instruction.lowTag == RET)) {
        break;
      }
    } while (true);
  }
}

template<typename T>
std::vector<std::pair<T, int> > count_occurrences(std::vector<T> vec, auto comparator) {
  std::sort(vec.begin(), vec.end(), comparator);

  std::vector<std::pair<T, int> > result;

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

void calc_max(const bytefile *const bf, const std::vector<int16_t> &used) {
  int32_t offset = 0;
  int16_t max_stack = 0;
  std::vector<int32_t> begins(0);
  while (offset < bf->code_size) {
    auto instruction = decodeInstruction(bf, offset);
    max_stack = std::max(used[offset], max_stack);
    if (instruction.highTag == CONST && instruction.lowTag == END) {
      int32_t begin_first_arg_offset = begins.back() + 1;
      begins.pop_back();
      if ((int32_t) bf->code_ptr[begin_first_arg_offset] > 31) {
        throw std::runtime_error(
          "Function has to many arguments at offset" + std::to_string(begin_first_arg_offset - 1));
      }
      bf->code_ptr[begin_first_arg_offset] = bf->code_ptr[begin_first_arg_offset] + (max_stack << 4);

      DEBUG_LOG("%s: %d at %s\n",
             hex8(offset).c_str(),
             max_stack,
             hex8(begin_first_arg_offset - 1).c_str()
      );
      max_stack = -1;
    }
    if (instruction.highTag == CONTROL && instruction.lowTag == BEGIN) {
      begins.push_back(offset);
    }
    offset += instruction.length();
  }
}

void print_statistics(const bytefile *const bf, const std::vector<int16_t> &used) {
  int32_t offset = 0;
  while (offset < bf->code_size) {
    auto instruction = decodeInstruction(bf, offset);

    printf("%s: %d (used[%d] = %d)\n",
           hex8(offset).c_str(),
           offset,
           offset,
           used[offset]);
    printf("  %s\n", instruction.to_string(bf).c_str());

    offset += instruction.length();
  }
}
