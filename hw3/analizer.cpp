//
// Created by enotvtapke on 10/25/25.
//

#include <string.h>
#include <string>
#include <stdio.h>

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
  printf("}");
}

void print_cf_graph(const std::map<int64_t, std::vector<int64_t>>& graph, const int64_t bf_code_ptr) {
  for (const auto& entry : graph) {
    printf("  0x%.8x -> {", entry.first - bf_code_ptr);
    bool first = true;
    for (const int64_t target : entry.second) {
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

enum Instruction {
  // High nibble values (h)
  BINOP = 0,
  CONST = 1,
  LD = 2,
  LDA = 3,
  ST = 4,
  CONTROL = 5,
  PATT = 6,
  BUILTIN = 7,
  STOP = 15,

  // Low nibble values for CONST group (h=1)
  CONST_INT = 0,
  CONST_STRING = 1,
  MAKE_SEXP = 2,
  STI = 3,
  STA = 4,
  JMP = 5,
  END = 6,
  RET = 7,
  DROP = 8,
  DUP = 9,
  SWAP = 10,
  ELEM = 11,

  // Low nibble values for LD/LDA/ST variable locations
  GLOBAL = 0,
  LOCAL = 1,
  ARG = 2,
  CLOSURE_VAR = 3,

  // Low nibble values for CONTROL group (h=5)
  CJMPz = 0,
  CJMPnz = 1,
  BEGIN = 2,
  CBEGIN = 3,
  MAKE_CLOSURE = 4,
  CALLC = 5,
  CALL = 6,
  TAG = 7,
  MAKE_ARRAY = 8,
  FAIL_I = 9,
  LINE = 10,

  // Low nibble values for PATT group (h=6)
  PATT_STR_EQ = 0,
  PATT_STRING = 1,
  PATT_ARRAY = 2,
  PATT_SEXP = 3,
  PATT_BOXED = 4,
  PATT_UNBOXED = 5,
  PATT_CLOSURE = 6,

  // Low nibble values for BUILTIN group (h=7)
  BUILTIN_Lread = 0,
  BUILTIN_Lwrite = 1,
  BUILTIN_Llength = 2,
  BUILTIN_Lstring = 3,
  BUILTIN_Barray = 4
};

void dfs(const int64_t node, std::unordered_set<int64_t> &used) {
  if (used.find(node) != used.end()) {
    return;
  }

  used.insert(node);
  printf("0x%.8x\n", node - (int64_t) state.bf->code_ptr);
  interpret(state.bf, CALCULATE_STAT, node - (int64_t) state.bf->code_ptr);

  auto it = cf_graph.find(node);
  if (it != cf_graph.end()) {
    for (int64_t successor : it->second) {
      dfs(successor, used);
    }
  }
}

static inline std::string hex8(const int v) {
  char buf[16];
  snprintf(buf, sizeof(buf), "0x%.8x", v);
  return {buf};
}

/* Disassembles the bytecode pool */
void interpret(const bytefile *bf, const Phase phase, const unsigned int entrypoint_offset) {
  state.ip = bf->code_ptr + ((phase == CALCULATE_STAT) ? entrypoint_offset : bf->entrypoint_offset);
  state.bf = bf;

  if (phase == GENERATE_BLOCKS) basic_blocks_offsets.insert((int64_t) state.ip);
  int64_t current_block_offset = -1;
  std::string prev_token;

  #ifdef DEBUG_PRINT
  static const char* const ops[] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
  static const char* const pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
  #endif
  do {
    if (phase == GENERATE_GRAPH && basic_blocks_offsets.find((int64_t) state.ip) != basic_blocks_offsets.end()) {
      DEBUG_LOG("---\n");
      current_block_offset = (int64_t) state.ip;
    }
    if (phase == CALCULATE_STAT &&
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
        DEBUG_LOG("BINOP\t%s", ops[l - 1]);
        token = std::string("BINOP ") + ops[l - 1];
        break;

      case CONST:
        switch (l) {
          case CONST_INT: {
            const int64_t value = INT;
            DEBUG_LOG("CONST\t%ld", value);
            token = std::string("CONST ") + std::to_string(value);
            break;
          }

          case CONST_STRING: {
            const char * s = STRING;
            DEBUG_LOG("STRING\t%s", s);
            token = std::string("STRING ") + std::string(s);
            break;
          }

          case MAKE_SEXP: {
            const char * tag = STRING;
            const unsigned int n = INT;
            DEBUG_LOG("SEXP\t%s ", tag);
            DEBUG_LOG("%d", n);
            token = std::string("SEXP ") + tag + " " + std::to_string(n);
            break;
          }

          case STI: {
            DEBUG_LOG("STI");
            failure("Should not happen. Indirect assignments are temporarily prohibited.\n");
          }

          case STA: {
              DEBUG_LOG("STA");
              token = "STA";
              break;
          }

          case JMP: {
            const int offset = INT;
            if (phase == GENERATE_BLOCKS) {
              basic_blocks_offsets.insert((int64_t) state.bf->code_ptr + offset);
              basic_blocks_offsets.insert((int64_t) state.ip);
            }
            if (phase == GENERATE_GRAPH) {
              add_to_cf_graph(current_block_offset, (int64_t) state.bf->code_ptr + offset);
            }
            DEBUG_LOG("JMP\t0x%.8x", offset);
            token = std::string("JMP ") + hex8(offset);
            break;
          }

          case END: {
            if (phase == GENERATE_BLOCKS) {
              basic_blocks_offsets.insert((int64_t) state.ip);
            }
            DEBUG_LOG("END");
            token = "END";
            break;
          }
          case RET: {
            if (phase == GENERATE_BLOCKS) {
              basic_blocks_offsets.insert((int64_t) state.ip);
            }
            DEBUG_LOG("RET");
            token = "RET";
            break;
          }

          case DROP:
            DEBUG_LOG("DROP");
            token = "DROP";
            break;

          case DUP: {
            DEBUG_LOG("DUP");
            token = "DUP";
            break;
          }

          case SWAP: {
            DEBUG_LOG("SWAP");
            token = "SWAP";
            break;
          }

          case ELEM: {
            DEBUG_LOG("ELEM");
            token = "ELEM";
            break;
          }

          default:
            FAIL;
        }
        break;

      case LD: {
        DEBUG_LOG("LD\t");
        const int index = INT;
        DEBUG_LOG("=%d", index);
        const char *scope = l == GLOBAL ? "G" : l == LOCAL ? "L" : l == ARG ? "A" : "C";
        token = std::string("LD ") + scope + " " + std::to_string(index);
        break;
      }
      case LDA: {
        DEBUG_LOG("LDA\t");
        failure("Should not happen. Indirect assignments are temporarily prohibited.\n");
      }
      case ST: {
        DEBUG_LOG("ST\t");
        const int index = INT;
        DEBUG_LOG("=%d", index);
        const char *scope = l == GLOBAL ? "G" : l == LOCAL ? "L" : l == ARG ? "A" : "C";
        token = std::string("ST ") + scope + " " + std::to_string(index);
        break;
      }

      case CONTROL:
        switch (l) {
          case CJMPz: {
            const int64_t offset = INT;
            if (phase == GENERATE_BLOCKS) {
              basic_blocks_offsets.insert((int64_t) state.ip);
              basic_blocks_offsets.insert((int64_t) state.bf->code_ptr + offset);
            }
            if (phase == GENERATE_GRAPH) {
              add_to_cf_graph(current_block_offset, (int64_t) state.ip);
              add_to_cf_graph(current_block_offset, (int64_t) state.bf->code_ptr + offset);
            }
            DEBUG_LOG("CJMPz\t0x%.8x", offset);
            token = std::string("CJMPz ") + hex8((int) offset);
            break;
          }

          case CJMPnz: {
            const int64_t offset = INT;
            if (phase == GENERATE_BLOCKS) {
              basic_blocks_offsets.insert((int64_t) state.ip);
              basic_blocks_offsets.insert((int64_t) state.bf->code_ptr + offset);
            }
            if (phase == GENERATE_GRAPH) {
              add_to_cf_graph(current_block_offset, (int64_t) state.ip);
              add_to_cf_graph(current_block_offset, (int64_t) state.bf->code_ptr + offset);
            }
            DEBUG_LOG("CJMPnz\t0x%.8x", offset);
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
            DEBUG_LOG(l == BEGIN ? "BEGIN\t%d" : "CBEGIN\t%d", args_num);
            DEBUG_LOG("%d", locals_num);
            token = std::string(l == BEGIN ? "BEGIN " : "CBEGIN ") + std::to_string(args_num) + " " + std::to_string(locals_num);
            break;
          }

          case MAKE_CLOSURE: {
            const int offset = INT;
            const unsigned int vars_num = INT;
            DEBUG_LOG("CLOSURE\t0x%.8x\t%d", offset, vars_num);
            token = std::string("CLOSURE ") + hex8(offset) + " " + std::to_string(vars_num);
            break;
          }

          case CALLC: {
            const int args_num = INT;
            DEBUG_LOG("CALLC\t%d", args_num);
            token = std::string("CALLC ") + std::to_string(args_num);
            break;
          }

          case CALL: {
            const int offset = INT;
            const int locals_num = INT;
            if (phase == GENERATE_BLOCKS) {
              basic_blocks_offsets.insert((int64_t) state.ip);
              basic_blocks_offsets.insert((int64_t) state.bf->code_ptr + offset);
            }
            if (phase == GENERATE_GRAPH) {
              add_to_cf_graph(current_block_offset, (int64_t) state.ip);
              add_to_cf_graph(current_block_offset, (int64_t) state.bf->code_ptr + offset);
            }
            DEBUG_LOG("CALL\t0x%.8x %d", offset, locals_num);
            token = std::string("CALL ") + hex8(offset) + " " + std::to_string(locals_num);
            break;
          }

          case TAG: {
            const char * tag = STRING;
            const int len = INT;
            DEBUG_LOG("TAG\t%s %d", tag, len);
            token = std::string("TAG ") + tag + " " + std::to_string(len);
            break;
          }

          case MAKE_ARRAY: {
            const int n = INT;
            DEBUG_LOG("ARRAY\t%d", n);
            token = std::string("ARRAY ") + std::to_string(n);
            break;
          }

          case FAIL_I: {
            const int line = INT;
            const int col = INT;
            DEBUG_LOG("FAIL\t%d", line);
            DEBUG_LOG("%d", col);
            token = std::string("FAIL ") + std::to_string(line) + " " + std::to_string(col);
            break;
          }

          case LINE: {
            int line = INT;
            DEBUG_LOG("LINE\t%d", line);
            token = std::string("LINE ") + std::to_string(line);
            break;
          }

          default:
            FAIL;
        }
        break;

      case PATT:
        DEBUG_LOG("PATT\t%s", pats[l]);
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
            DEBUG_LOG("CALL\tLread");
            token = "CALL Lread";
            break;

          case BUILTIN_Lwrite:
            DEBUG_LOG("CALL\tLwrite");
            token = "CALL Lwrite";
            break;

          case BUILTIN_Llength: {
            DEBUG_LOG("CALL\tLlength");
            token = "CALL Llength";
            break;
          }

          case BUILTIN_Lstring:
            DEBUG_LOG("CALL\tLstring");
            token = "CALL Lstring";
            break;

          case BUILTIN_Barray: {
            const unsigned int len = INT;
            DEBUG_LOG("CALL\tBarray %d", len);
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

    if (phase == CALCULATE_STAT && !token.empty()) {
      opcode_freq[token]++;
      if (!prev_token.empty()) {
        opcode_freq[prev_token + SEP + token]++;
      }
      prev_token = token;
    }

    DEBUG_LOG("\n");
  } while (1);
stop:
  printf("<done>\n");
  switch (phase) {
    case GENERATE_BLOCKS: {
      print_set(basic_blocks_offsets, (int64_t) state.bf->code_ptr);
      printf("\n");
      break;
    }
    case GENERATE_GRAPH: {
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
