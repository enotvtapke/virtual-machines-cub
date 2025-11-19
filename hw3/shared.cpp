#include <stdexcept>
#include <string>

#include <vector>

#include "analizer.h"

inline static int read(char **ip, const unsigned int bytes, const bytefile *bf) {
  char *ipValue = *ip;
  if (ipValue + bytes > bf->code_ptr + bf->code_size)
    failure(
      "When reading %d bytes IP counter %d can move outside of the code section of size\n",
      bytes,
      ipValue,
      bf->code_size
    );
  *ip = *ip + bytes;
  return *reinterpret_cast<int *>(ipValue);
}

enum BinOps {
  ADD = 1,
  SUB = 2,
  MUL = 3,
  DIV = 4,
  MOD = 5,
  LT = 6,
  LTE = 7,
  GT = 8,
  GTE = 9,
  EQ = 10,
  NEQ = 11,
  AND = 12,
  NOT = 13
};

Instruction decodeInstruction(const bytefile *bf, const unsigned int entrypoint_offset) {
  char *ip = bf->code_ptr + entrypoint_offset;
#define INT (read(&ip, 4, bf))
#define BYTE (read(&ip, 1, bf))
#define STRING get_string(bf, INT)
#define FAIL failure("ERROR: invalid opcode %d-%d\n", h, l)

  const unsigned char x = BYTE, h = (x & 0xF0) >> 4, l = x & 0x0F;

  switch (h) {
    case STOP: return Instruction{STOP, EMPTY_TAG, {}};

    case BINOP: return Instruction{BINOP, static_cast<InstructionTag>(l), {}};

    case CONST:
      switch (l) {
        case CONST_INT: return Instruction{CONST, CONST_INT, {INT}};
        case CONST_STRING: return Instruction{CONST, CONST_STRING, {INT}};
        case MAKE_SEXP: return Instruction{CONST, MAKE_SEXP, {INT, INT}};
        case STI: failure("Should not happen. Indirect assignments are temporarily prohibited.\n");
        case STA: return Instruction{CONST, STA, {}};
        case JMP: return Instruction{CONST, JMP, {INT}};
        case END: return Instruction{CONST, END, {}};
        case RET: return Instruction{CONST, RET, {}};
        case DROP: return Instruction{CONST, DROP, {}};
        case DUP: return Instruction{CONST, DUP, {}};
        case SWAP: return Instruction{CONST, SWAP, {}};
        case ELEM: return Instruction{CONST, ELEM, {}};
        default: FAIL;
      }
      break;

    case LD: return Instruction{LD, static_cast<InstructionTag>(l), {INT}};
    case LDA: failure("Should not happen. Indirect assignments are temporarily prohibited.\n");
    case ST: return Instruction{ST, static_cast<InstructionTag>(l), {INT}};

    case CONTROL:
      switch (l) {
        case CJMPz: return Instruction{CONTROL, CJMPz, {INT}};
        case CJMPnz: return Instruction{CONTROL, CJMPnz, {INT}};
        case BEGIN: return Instruction{CONTROL, BEGIN, {INT, INT}};
        case CBEGIN: return Instruction{CONTROL, CBEGIN, {INT, INT}};
        case MAKE_CLOSURE: return Instruction{CONTROL, MAKE_CLOSURE, {INT, INT}};
        case CALLC: return Instruction{CONTROL, CALLC, {INT}};
        case CALL: return Instruction{CONTROL, CALL, {INT, INT}};
        case TAG: return Instruction{CONTROL, TAG, {INT, INT}};
        case MAKE_ARRAY: return Instruction{CONTROL, MAKE_ARRAY, {INT}};
        case FAIL_I: return Instruction{CONTROL, FAIL_I, {INT, INT}};
        case LINE: return Instruction{CONTROL, LINE, {INT}};
        default: FAIL;
      }
      break;

    case PATT: {
      return Instruction{PATT, static_cast<InstructionTag>(l), {}};
    }

    case BUILTIN: {
      switch (l) {
        case BUILTIN_Lread: return Instruction{BUILTIN, BUILTIN_Lread, {}};
        case BUILTIN_Lwrite: return Instruction{BUILTIN, BUILTIN_Lwrite, {}};
        case BUILTIN_Llength: return Instruction{BUILTIN, BUILTIN_Llength, {}};
        case BUILTIN_Lstring: return Instruction{BUILTIN, BUILTIN_Lstring, {}};
        case BUILTIN_Barray: return Instruction{BUILTIN, BUILTIN_Barray, {INT}};
        default: FAIL;
      }
    }

    default:
      FAIL;
  }
  throw std::logic_error("Unreachable");
}
