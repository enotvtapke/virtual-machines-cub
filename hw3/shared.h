#ifndef HW3_SHARED_H
#define HW3_SHARED_H

#include <string>
#include <vector>
#include <stdexcept>

static void failure(const std::string &s, ...) {
  throw std::runtime_error(s);
}

struct bytefile {
  char *string_ptr; // A pointer to the beginning of the string table
  int32_t *public_ptr; // A pointer to the beginning of publics table
  char *code_ptr; // A pointer to the bytecode itself
  int64_t *global_ptr; // A pointer to the global area
  unsigned long code_size; // Code section size in bytes
  unsigned int entrypoint_offset; // Public symbol "main" offset
  unsigned int stringtab_size; // The size (in bytes) of the string table
  unsigned int global_area_size; // The size (in words) of global area
  unsigned int public_symbols_number; // The number of public symbols
  char buffer[0];

  std::string get_string(const unsigned int pos) const {
    if (pos >= this->stringtab_size) failure("*** FAILURE: invalid string index");
    return &this->string_ptr[pos];
  };
};

static std::string hex8(const int v) {
  char buf[16];
  snprintf(buf, sizeof(buf), "0x%.8x", v);
  return {buf};
}

/* Gets an offset for a public symbol */
int get_public_offset(const bytefile *f, unsigned int i);

enum InstructionTag {
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
  BUILTIN_Barray = 4,

  // Empty Tag
  EMPTY_TAG = 0
};

struct Instruction {
  InstructionTag highTag;
  InstructionTag lowTag;
  std::vector<int32_t> args;

  size_t length() const {
    return args.size() * 4 + 1;
  }

  bool operator<(const Instruction &other) const {
    if (highTag != other.highTag) {
      return highTag < other.highTag;
    }
    if (lowTag != other.lowTag) {
      return lowTag < other.lowTag;
    }
    return args < other.args;
  }

  bool operator==(const Instruction &other) const {
    return highTag == other.highTag && lowTag == other.lowTag && args == other.args;
  }

  std::string to_string(const bytefile *f) const {
    static const char *const ops[] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
    static const char *const pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
    static const char *const lds[] = {"LD", "LDA", "ST"};

    switch (highTag) {
      case STOP:
        return "<end>";

      case BINOP:
        return std::string("BINOP\t") + ops[lowTag - 1];

      case CONST:
        switch (lowTag) {
          case CONST_INT:
            return std::string("CONST\t") + std::to_string(args[0]);
          case CONST_STRING:
            return std::string("STRING\t") + f->get_string(args[0]);
          case MAKE_SEXP:
            return std::string("SEXP\t") + f->get_string(args[0]) + " " + std::to_string(args[1]);
          case STI:
            return "STI";
          case STA:
            return "STA";
          case JMP:
            return std::string("JMP\t") + hex8(args[0]);
          case END:
            return "END";
          case RET:
            return "RET";
          case DROP:
            return "DROP";
          case DUP:
            return "DUP";
          case SWAP:
            return "SWAP";
          case ELEM:
            return "ELEM";
          default:
            return "UNKNOWN";
        }

      case LD:
      case LDA:
      case ST: {
        std::string result = std::string(lds[highTag - 2]) + "\t";
        switch (lowTag) {
          case GLOBAL:
            result += "G(" + std::to_string(args[0]) + ")";
            break;
          case LOCAL:
            result += "L(" + std::to_string(args[0]) + ")";
            break;
          case ARG:
            result += "A(" + std::to_string(args[0]) + ")";
            break;
          case CLOSURE_VAR:
            result += "C(" + std::to_string(args[0]) + ")";
            break;
          default:
            result += "UNKNOWN";
        }
        return result;
      }

      case CONTROL:
        switch (lowTag) {
          case CJMPz:
            return std::string("CJMPz\t") + hex8(args[0]);
          case CJMPnz:
            return std::string("CJMPnz\t") + hex8(args[0]);
          case BEGIN:
            return std::string("BEGIN\t") + std::to_string(args[0]) + " " + std::to_string(args[1]);
          case CBEGIN:
            return std::string("CBEGIN\t") + std::to_string(args[0]) + " " + std::to_string(args[1]);
          case MAKE_CLOSURE:
            return std::string("CLOSURE\t") + hex8(args[0]) + " " + std::to_string(args[1]);
          case CALLC:
            return std::string("CALLC\t") + std::to_string(args[0]);
          case CALL:
            return std::string("CALL\t") + hex8(args[0]) + " " + std::to_string(args[1]);
          case TAG:
            return std::string("TAG\t") + f->get_string(args[0]) + " " + std::to_string(args[1]);
          case MAKE_ARRAY:
            return std::string("ARRAY\t") + std::to_string(args[0]);
          case FAIL_I:
            return std::string("FAIL\t") + std::to_string(args[0]) + " " + std::to_string(args[1]);
          case LINE:
            return std::string("LINE\t") + std::to_string(args[0]);
          default:
            return "UNKNOWN";
        }

      case PATT:
        return std::string("PATT\t") + pats[lowTag];

      case BUILTIN:
        switch (lowTag) {
          case BUILTIN_Lread:
            return "CALL\tLread";
          case BUILTIN_Lwrite:
            return "CALL\tLwrite";
          case BUILTIN_Llength:
            return "CALL\tLlength";
          case BUILTIN_Lstring:
            return "CALL\tLstring";
          case BUILTIN_Barray:
            return std::string("CALL\tBarray\t") + std::to_string(args[0]);
          default:
            return "UNKNOWN";
        }

      default:
        return "UNKNOWN";
    }
  }
};

Instruction decodeInstruction(const bytefile *bf, unsigned int entrypoint_offset);

std::vector<Instruction> decodeInstructions(const bytefile *bf, unsigned int entrypoint_offset);

#endif //HW3_SHARED_H
