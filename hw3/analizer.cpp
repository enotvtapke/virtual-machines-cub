//
// Created by enotvtapke on 10/25/25.
//

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "analizer.h"

#define EMPTY BOX(0)

typedef struct {
  char *ip;
  const bytefile *bf;
} State;

static State state;

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

/* Disassembles the bytecode pool */
void interpret(const bytefile *bf) {
  state.ip = bf->code_ptr + bf->entrypoint_offset;
  state.bf = bf;

  #ifdef DEBUG_PRINT
  static const char* const ops[] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
  static const char* const pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
  #endif
  do {
    const unsigned char x = BYTE, h = (x & 0xF0) >> 4, l = x & 0x0F;
    DEBUG_LOG("0x%.8x:\t", state.ip - state.bf->code_ptr - 1);
    switch (h) {
      case STOP:
        goto stop;

      case BINOP:
        DEBUG_LOG("BINOP\t%s", ops[l - 1]);
        break;

      case CONST:
        switch (l) {
          case CONST_INT: {
            const int64_t value = INT;
            DEBUG_LOG("CONST\t%d", value);
            break;
          }

          case CONST_STRING: {
            const char * s = STRING;
            DEBUG_LOG("STRING\t%s", s);
            break;
          }

          case MAKE_SEXP: {
            const char * tag = STRING;
            const unsigned int n = INT;
            DEBUG_LOG("SEXP\t%s ", tag);
            DEBUG_LOG("%d", n);
            break;
          }

          case STI: {
            DEBUG_LOG("STI");
            failure("Should not happen. Indirect assignments are temporarily prohibited.\n");
          }

          case STA: {
              DEBUG_LOG("STA");
              break;
          }

          case JMP: {
            const int offset = INT;
            DEBUG_LOG("JMP\t0x%.8x", offset);
            break;
          }

          case END:
          case RET: {
            DEBUG_LOG("END/RET");
            break;
          }

          case DROP:
            DEBUG_LOG("DROP");
            break;

          case DUP: {
            DEBUG_LOG("DUP");
            break;
          }

          case SWAP: {
            DEBUG_LOG("SWAP");
            break;
          }

          case ELEM: {
            DEBUG_LOG("ELEM");
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
        break;
      }

      case CONTROL:
        switch (l) {
          case CJMPz: {
            const int64_t offset = INT;
            DEBUG_LOG("CJMPz\t0x%.8x", offset);
            break;
          }

          case CJMPnz: {
            const int64_t offset = INT;
            DEBUG_LOG("CJMPnz\t0x%.8x", offset);
            break;
          }

          case BEGIN:
          case CBEGIN: {
            const int args_num = INT;
            const int locals_num = INT;
            DEBUG_LOG("BEGIN\t%d ", args_num);
            DEBUG_LOG("%d", locals_num);
            break;
          }

          case MAKE_CLOSURE: {
            const int offset = INT;
            const unsigned int vars_num = INT;
            DEBUG_LOG("CLOSURE\t0x%.8x\t%d", offset, vars_num);
            break;
          }

          case CALLC: {
            const int args_num = INT;
            DEBUG_LOG("CALLC\t%d", args_num);
            break;
          }

          case CALL: {
            const int offset = INT;
            const int locals_num = INT;
            DEBUG_LOG("CALL\t0x%.8x %d", offset, locals_num);
            break;
          }

          case TAG: {
            const char * tag = STRING;
            const int len = INT;
            DEBUG_LOG("TAG\t%s %d", tag, len);
            break;
          }

          case MAKE_ARRAY: {
            const int n = INT;
            DEBUG_LOG("ARRAY\t%d", n);
            break;
          }

          case FAIL_I: {
            const int line = INT;
            const int col = INT;
            DEBUG_LOG("FAIL\t%d", line);
            DEBUG_LOG("%d", col);
            break;
          }

          case LINE: {
            int line = INT;
            DEBUG_LOG("LINE\t%d", line);
            break;
          }

          default:
            FAIL;
        }
        break;

      case PATT:
        DEBUG_LOG("PATT\t%s", pats[l]);
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
            break;

          case BUILTIN_Lwrite:
            DEBUG_LOG("CALL\tLwrite");
            break;

          case BUILTIN_Llength: {
            DEBUG_LOG("CALL\tLlength");
            break;
          }

          case BUILTIN_Lstring:
            DEBUG_LOG("CALL\tLstring");
            break;

          case BUILTIN_Barray: {
            const unsigned int len = INT;
            DEBUG_LOG("CALL\tBarray %d", len);
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

    DEBUG_LOG("\n");
  } while (1);
stop:
  printf("<done>\n");
}
