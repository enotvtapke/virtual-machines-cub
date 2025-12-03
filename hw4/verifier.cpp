#include <cstring>
#include <string>
#include <cstdio>

#include <vector>
#include <algorithm>

#include "verifier.h"

#include <iostream>

struct __attribute__((packed)) InstructionView {
  int32_t offset;
  int8_t length;
};

void traverse(
  const bytefile *bf,
  std::vector<int32_t> &stack,
  std::vector<int16_t> &used
) {
  while (!stack.empty()) {
    // fprintf(stderr, "=====\n");

    int32_t current_offset = stack.back();
    stack.pop_back();
    int16_t current_stack = used[current_offset];
    do {
      auto instruction = decodeInstruction(bf, current_offset);
      // fprintf(stderr, "%s %d:\t%s\n", hex8(current_offset).c_str(), current_stack,
      //         instruction.to_string(bf).c_str());

      if (instruction.highTag == CONST && instruction.lowTag == MAKE_SEXP) {
        const int n = instruction.args[1];
        if (n > current_stack) {
          failure("Invalid sexpr length %d at ip %d", n, current_offset);
        }
      }

      if (instruction.highTag == BUILTIN && instruction.lowTag == BUILTIN_Barray) {
        const int n = instruction.args[0];
        if (n > current_stack) {
          failure("Invalid array length %d at ip %d\n", n, current_offset);
        }
      }

      if (instruction.highTag == CONTROL && instruction.lowTag == CALLC) {
        const int args_num = instruction.args[0];
        if (args_num > current_stack) {
          failure("CALLC have invalid number of arguments %d at ip %d", args_num, current_offset);
        }
      }

      current_stack += instruction.stack_diff();
      if (current_stack < 0) {
        throw std::runtime_error("Stack is exhausted at instruction " + instruction.to_string(bf) + " at offset " + hex8(current_offset));
      }

      const int8_t length = instruction.length();
      current_offset += length;

      auto next_step = [&](const int32_t offset, const int16_t expected_stack) {
        if (used[offset] == -1) {
          used[offset] = expected_stack;
          stack.push_back(offset);
        } else if (used[offset] != expected_stack) {
          throw std::runtime_error("Stack size mismatch at offset " + hex8(offset));
        }
      };

      if (instruction.highTag == CONST && instruction.lowTag == JMP) {
        next_step(instruction.args[0], current_stack);
        break;
      }

      if (instruction.highTag == CONTROL && (instruction.lowTag == CJMPz || instruction.
                                             lowTag == CJMPnz)) {
        next_step(instruction.args[0], current_stack);
        next_step(current_offset, current_stack);
        break;
      }

      if (instruction.highTag == CONTROL && instruction.lowTag == CALL) {
        next_step(instruction.args[0], 0);
        next_step(current_offset, current_stack);
        break;
      }

      if (instruction.highTag == CONST && (instruction.lowTag == END || instruction.lowTag == RET)
          || instruction.highTag == CONTROL && instruction.lowTag == FAIL_I) {
        break;
      }

      if (used[current_offset] == -1) {
        used[current_offset] = current_stack;
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

void validate_variable_index(const bytefile *const bf, const int args_num, const int locals_num,
                             unsigned char designation, int index) {
  switch (designation) {
    case GLOBAL:
      if (index >= bf->global_area_size) {
        failure("Global variable %d out of bounds. Number of globals %d\n", index, bf->global_area_size);
      }
      break;
    case LOCAL:
      if (index >= locals_num) {
        failure("Local variable %d out of bounds. Number of locals %d\n", index, locals_num);
      }
      break;
    case ARG:
      if (index >= args_num) {
        failure("Argument variable %d out of bounds. Number of arguments %d\n", index, args_num);
      }
      break;
    case CLOSURE_VAR:
      break;
    default:
      failure("Unknown designation %d\n", designation);
  }
}

void verify_and_calc_max_stack_size(const bytefile *const bf, const std::vector<int16_t> &used) {
  int32_t current_offset = 0;
  int16_t max_stack = 0;
  std::vector<int32_t> begin_offsets(0);
  while (current_offset < bf->code_size) {
    auto instruction = decodeInstruction(bf, current_offset);
    // fprintf(stderr, "%s: %s\n",
    //           hex8(offset).c_str(),
    //           instruction.to_string(bf).c_str()
    //   );
    max_stack = std::max(used[current_offset], max_stack);
    if (instruction.highTag == CONST && instruction.lowTag == END) {
      int32_t begin_first_arg_offset = begin_offsets.back() + 1;
      begin_offsets.pop_back();
      if ((int32_t) bf->code_ptr[begin_first_arg_offset] >= 1 << 16) {
        throw std::runtime_error(
          "Function has to many arguments at offset" + std::to_string(begin_first_arg_offset - 1));
      }
      bf->code_ptr[begin_first_arg_offset] = bf->code_ptr[begin_first_arg_offset] + (max_stack << 16);

      // fprintf(stderr, "%s: %d at %s\n",
      //         hex8(current_offset).c_str(),
      //         max_stack,
      //         hex8(begin_first_arg_offset - 1).c_str()
      // );
      max_stack = -1;
    }
    if (instruction.highTag == CONTROL && (instruction.lowTag == BEGIN || instruction.lowTag == CBEGIN)) {
      begin_offsets.push_back(current_offset);
    }
    if (used[current_offset] != -1) {
      const int32_t begin_offset = begin_offsets.back();
      auto instruction_begin = decodeInstruction(bf, begin_offset);
      const int args_num = instruction_begin.args[0] & 0xFFFF;
      const int locals_num = instruction_begin.args[1];
      if (instruction.highTag == ST || instruction.highTag == LD) {
        const unsigned char designation = instruction.lowTag;
        const int index = instruction.args[0];
        validate_variable_index(bf, args_num, locals_num, designation, index);
      }
      if (instruction.highTag == CONTROL && instruction.lowTag == MAKE_CLOSURE) {
        for (int i = 0; i < instruction.args[1]; i++) {
          const int32_t tag = ((uint32_t) instruction.args[i + 2]) >> 30;
          const int index = instruction.args[i + 2] & 0x3FFFFFFF;
          validate_variable_index(bf, args_num, locals_num, tag, index);
        }
      }
      if (instruction.highTag == CONST && instruction.lowTag == JMP
          || instruction.highTag == CONTROL && (instruction.lowTag == CJMPz || instruction.lowTag == CJMPnz ||
                                                instruction.lowTag == CALL)) {
        if (instruction.args[0] >= bf->code_size) {
          failure("Jump with offset %d is outside of code section of size %d\n", instruction.args[0], bf->code_size);
        }
      }
    }
    current_offset += instruction.length();
  }
}

void print_statistics(const bytefile *const bf, const std::vector<int16_t> &used) {
  int32_t offset = 0;
  while (offset < bf->code_size) {
    auto instruction = decodeInstruction(bf, offset);

    fprintf(stderr, "%s: %d (used[%d] = %d)\n",
            hex8(offset).c_str(),
            offset,
            offset,
            used[offset]);
    fprintf(stderr, "  %s\n", instruction.to_string(bf).c_str());

    offset += instruction.length();
  }
}
