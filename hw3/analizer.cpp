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

#define EMPTY BOX(0)

typedef struct {
  char *ip;
  const bytefile *bf;
} State;

static State state;

std::unordered_set<int64_t> basic_blocks_offsets;
std::map<int64_t, std::vector<int64_t>> cf_graph;

static auto SEP = " | ";
static std::unordered_map<std::string, long long> opcode_freq;

void add_to_cf_graph(const int64_t key, const int64_t value) {
  auto it = cf_graph.find(key);
  if (it == cf_graph.end()) {
    cf_graph[key] = std::vector<int64_t>{value};
  } else {
    it->second.push_back(value);
  }
}

void print_set(const std::unordered_set<int64_t>& s, const int64_t bf_code_ptr) {
  printf("{");
  bool first = true;
  for (int64_t value : s) {
    if (!first) {
      printf(", ");
    }
    printf("0x%.8x", value - bf_code_ptr);
    first = false;
  }
  printf("}\n");
}

void print_cf_graph(const std::map<int64_t, std::vector<int64_t>>& graph, const int64_t bf_code_ptr) {
  for (const auto&[from, to] : graph) {
    printf("0x%.8x -> {", from - bf_code_ptr);
    bool first = true;
    for (const int64_t target : to) {
      if (!first) {
        printf(", ");
      }
      printf("0x%.8x", target - bf_code_ptr);
      first = false;
    }
    printf("}\n");
  }
}

inline static int read(const unsigned int bytes) {
  if (state.ip + bytes > state.bf->code_ptr + state.bf->code_size) {
    failure("When reading %d bytes IP counter %d can move outside of the code section of size\n", bytes, state.ip,
            state.bf->code_size);
  }
  state.ip += bytes;
  return *(int *)(state.ip - bytes);
}

#define INT (read(4))
#define BYTE (read(1))
#define STRING get_string(state.bf, INT)
#define FAIL failure("ERROR: invalid opcode %d-%d\n", h, l)


void dfs(const int64_t node, std::unordered_set<int64_t> &used) {
  if (used.find(node) != used.end()) {
    return;
  }

  used.insert(node);
  interpret(state.bf, ANALYZE_FREQUENCIES, node - (int64_t) state.bf->code_ptr);

  auto it = cf_graph.find(node);
  if (it != cf_graph.end()) {
    for (int64_t successor : it->second) {
      dfs(successor, used);
    }
  }
}

/* Disassembles the bytecode pool */
void interpret(const bytefile *bf, const Phase phase, const unsigned int entrypoint_offset) {
  state.ip = bf->code_ptr + (phase == ANALYZE_FREQUENCIES ? entrypoint_offset : bf->entrypoint_offset);
  state.bf = bf;

  if (phase == FIND_BASIC_BLOCKS) basic_blocks_offsets.insert((int64_t) state.ip);
  int64_t current_block_offset = -1;
  std::string prev_token;

  static const char* const ops[] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
  static const char* const pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
  do {
    if (phase == CALCULATE_CFG && basic_blocks_offsets.find((int64_t) state.ip) != basic_blocks_offsets.end()) {
      DEBUG_LOG("---\n");
      current_block_offset = (int64_t) state.ip;
    }
    if (phase == ANALYZE_FREQUENCIES &&
        state.ip != bf->code_ptr + entrypoint_offset &&
        basic_blocks_offsets.contains((int64_t) state.ip)
    ) {
      goto stop;
    }
    const unsigned char x = BYTE, h = (x & 0xF0) >> 4, l = x & 0x0F;
    std::string token;

    DEBUG_LOG("0x%.8x:\t", state.ip - state.bf->code_ptr - 1);
    switch (h) {
      case STOP:
        goto stop;

      case BINOP:
        token = std::string("BINOP ") + ops[l - 1];
        break;

      case CONST:
        switch (l) {
          case CONST_INT: {
            const int64_t value = INT;
            token = std::string("CONST ") + std::to_string(value);
            break;
          }

          case CONST_STRING: {
            const char * s = STRING;
            token = std::string("STRING ") + std::string(s);
            break;
          }

          case MAKE_SEXP: {
            const char * tag = STRING;
            const unsigned int n = INT;
            token = std::string("SEXP ") + tag + " " + std::to_string(n);
            break;
          }

          case STI: {
            failure("Should not happen. Indirect assignments are temporarily prohibited.\n");
          }

          case STA: {
              token = "STA";
              break;
          }

          case JMP: {
            const int offset = INT;
            if (phase == FIND_BASIC_BLOCKS) {
              basic_blocks_offsets.insert((int64_t) state.bf->code_ptr + offset);
              basic_blocks_offsets.insert((int64_t) state.ip);
            }
            if (phase == CALCULATE_CFG) {
              add_to_cf_graph(current_block_offset, (int64_t) state.bf->code_ptr + offset);
            }
            token = std::string("JMP ") + hex8(offset);
            break;
          }

          case END: {
            if (phase == FIND_BASIC_BLOCKS) {
              basic_blocks_offsets.insert((int64_t) state.ip);
            }
            token = "END";
            break;
          }
          case RET: {
            if (phase == FIND_BASIC_BLOCKS) {
              basic_blocks_offsets.insert((int64_t) state.ip);
            }
            token = "RET";
            break;
          }

          case DROP:
            token = "DROP";
            break;

          case DUP: {
            token = "DUP";
            break;
          }

          case SWAP: {
            token = "SWAP";
            break;
          }

          case ELEM: {
            token = "ELEM";
            break;
          }

          default:
            FAIL;
        }
        break;

      case LD: {
        const int index = INT;
        const char *scope = l == GLOBAL ? "G" : l == LOCAL ? "L" : l == ARG ? "A" : "C";
        token = std::string("LD ") + scope + " " + std::to_string(index);
        break;
      }
      case LDA: {
        failure("Should not happen. Indirect assignments are temporarily prohibited.\n");
      }
      case ST: {
        const int index = INT;
        const char *scope = l == GLOBAL ? "G" : l == LOCAL ? "L" : l == ARG ? "A" : "C";
        token = std::string("ST ") + scope + " " + std::to_string(index);
        break;
      }

      case CONTROL:
        switch (l) {
          case CJMPz: {
            const int64_t offset = INT;
            if (phase == FIND_BASIC_BLOCKS) {
              basic_blocks_offsets.insert((int64_t) state.ip);
              basic_blocks_offsets.insert((int64_t) state.bf->code_ptr + offset);
            }
            if (phase == CALCULATE_CFG) {
              add_to_cf_graph(current_block_offset, (int64_t) state.ip);
              add_to_cf_graph(current_block_offset, (int64_t) state.bf->code_ptr + offset);
            }
            token = std::string("CJMPz ") + hex8((int) offset);
            break;
          }

          case CJMPnz: {
            const int64_t offset = INT;
            if (phase == FIND_BASIC_BLOCKS) {
              basic_blocks_offsets.insert((int64_t) state.ip);
              basic_blocks_offsets.insert((int64_t) state.bf->code_ptr + offset);
            }
            if (phase == CALCULATE_CFG) {
              add_to_cf_graph(current_block_offset, (int64_t) state.ip);
              add_to_cf_graph(current_block_offset, (int64_t) state.bf->code_ptr + offset);
            }
            token = std::string("CJMPnz ") + hex8((int) offset);
            break;
          }

          case BEGIN:
          case CBEGIN: {
            const int args_num = INT;
            const int locals_num = INT;
            if (basic_blocks_offsets.find((int64_t) state.ip - 8) != basic_blocks_offsets.end()) {
              failure("ERROR: Begin block is not marked as basic block start\n");
            }
            token = std::string(l == BEGIN ? "BEGIN " : "CBEGIN ") + std::to_string(args_num) + " " + std::to_string(locals_num);
            break;
          }

          case MAKE_CLOSURE: {
            const int offset = INT;
            const unsigned int vars_num = INT;
            token = std::string("CLOSURE ") + hex8(offset) + " " + std::to_string(vars_num);
            break;
          }

          case CALLC: {
            const int args_num = INT;
            token = std::string("CALLC ") + std::to_string(args_num);
            break;
          }

          case CALL: {
            const int offset = INT;
            const int locals_num = INT;
            if (phase == FIND_BASIC_BLOCKS) {
              basic_blocks_offsets.insert((int64_t) state.ip);
              basic_blocks_offsets.insert((int64_t) state.bf->code_ptr + offset);
            }
            if (phase == CALCULATE_CFG) {
              add_to_cf_graph(current_block_offset, (int64_t) state.ip);
              add_to_cf_graph(current_block_offset, (int64_t) state.bf->code_ptr + offset);
            }
            token = std::string("CALL ") + hex8(offset) + " " + std::to_string(locals_num);
            break;
          }

          case TAG: {
            const char * tag = STRING;
            const int len = INT;
            token = std::string("TAG ") + tag + " " + std::to_string(len);
            break;
          }

          case MAKE_ARRAY: {
            const int n = INT;
            token = std::string("ARRAY ") + std::to_string(n);
            break;
          }

          case FAIL_I: {
            const int line = INT;
            const int col = INT;
            token = std::string("FAIL ") + std::to_string(line) + " " + std::to_string(col);
            break;
          }

          case LINE: {
            int line = INT;
            token = std::string("LINE ") + std::to_string(line);
            break;
          }

          default:
            FAIL;
        }
        break;

      case PATT:
        token = std::string("PATT ") + pats[l];
        switch (l) {
          case PATT_STR_EQ:
            break;
          case PATT_STRING:
            break;
          case PATT_ARRAY:
            break;
          case PATT_SEXP:
            break;
          case PATT_BOXED:
            break;
          case PATT_UNBOXED:
            break;
          case PATT_CLOSURE:
            break;
          default:
            FAIL;
        }
        break;

      case BUILTIN: {
        switch (l) {
          case BUILTIN_Lread:
            token = "CALL Lread";
            break;

          case BUILTIN_Lwrite:
            token = "CALL Lwrite";
            break;

          case BUILTIN_Llength: {
            token = "CALL Llength";
            break;
          }

          case BUILTIN_Lstring:
            token = "CALL Lstring";
            break;

          case BUILTIN_Barray: {
            const unsigned int len = INT;
            token = std::string("CALL Barray ") + std::to_string(len);
            break;
          }

          default:
            FAIL;
        }
      }
      break;

      default:
        FAIL;
    }
    DEBUG_LOG("%s\n", token.c_str());
    if (phase == ANALYZE_FREQUENCIES && !token.empty()) {
      opcode_freq[token]++;
      if (!prev_token.empty()) {
        opcode_freq[prev_token + SEP + token]++;
      }
      prev_token = token;
    }
  } while (true);
stop:
  DEBUG_LOG("<done>\n");
  switch (phase) {
    case FIND_BASIC_BLOCKS: {
      printf("Basic blocks offsets:\n");
      print_set(basic_blocks_offsets, (int64_t) state.bf->code_ptr);
      printf("\n");
      break;
    }
    case CALCULATE_CFG: {
      printf("Control flow graph:\n");
      print_cf_graph(cf_graph, (int64_t) state.bf->code_ptr);
      printf("\n");
      break;
    }
    default: ;
  }
}

void print_statistics() {
  std::vector<std::pair<std::string,long long>> op_list(opcode_freq.begin(), opcode_freq.end());
  std::ranges::sort(op_list, [](const auto &a, const auto &b){
    if (a.second != b.second) return a.second > b.second;
    return a.first < b.first;
  });
  printf("Instruction frequencies :\n");
  for (const auto &[opcodes, freq] : op_list) {
    printf("%lld :\t%s\n", freq, opcodes.c_str());
  }
}
