//
// Created by enotvtapke on 10/25/25.
//

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "interpreter.h"
#include "./runtime/runtime.c"

#define EMPTY BOX(0)

typedef struct {
  char *ip;
  aint *ebp;
  const bytefile *bf;
} State;

static State state;

#define ESP (((aint *) __gc_stack_top) + 1)

static void print_stack_value(const aint v) {
  if (UNBOXED(v)) {
    DEBUG_LOG("%d", UNBOX(v));
  } else {
    const data * d = TO_DATA(v);
    size_t l = LEN(d->data_header);
    // STRING_TAG 0x00000001
    // ARRAY_TAG 0x00000003
    // SEXP_TAG 0x00000005
    // CLOSURE_TAG 0x00000007
    DEBUG_LOG("%d: tag %d, len %d", v, TAG(d->data_header), l);
  }
}

static void dump_stack() {
  const size_t m = state.bf->stack_ptr - ESP;
  DEBUG_LOG("---STACK---\n");
  for (int i = 0; i < m; ++i) {
    print_stack_value(*(ESP + i));
    if (ESP + i == state.ebp) {
      DEBUG_LOG(" <- ebp");
    }
    DEBUG_LOG("\n");
    fflush(stdout);
  }
  DEBUG_LOG("----------\n");
  fflush(stdout);
}

inline static void push(const aint value) {
  if (ESP <= state.bf->stack_ptr - STACK_SIZE) {
    failure("Stack overflow at IP %d\n", state.ip);
  }
  __gc_stack_top -= sizeof(size_t);
  *ESP = value;
}

inline static aint get_locals_num() {
  return UNBOX(*(state.ebp - 2));
}

inline static aint pop() {
  if (ESP >= state.ebp - 3 - (get_locals_num() - 1)) {
    failure("Popping values from stack frame (locals or worse) at IP %d\n", state.ip);
  }
  if (ESP >= state.bf->stack_ptr) {
    failure("Stack underflow at IP %d\n", state.ip);
  }
  __gc_stack_top += sizeof(size_t);
  return *(ESP - 1);
}

inline static aint * global(const unsigned int index) {
  if (index >= state.bf->global_area_size) {
    failure("Global variable %d out of bounds. Number of globals %d\n", index, state.bf->global_area_size);
  }
  return &state.bf->global_ptr[index];
}

inline static aint * local(const unsigned int index) {
  if (index >= get_locals_num()) {
    failure("Local variable %d out of bounds. Number of locals %d\n", index, get_locals_num());
  }
  return state.ebp - 3 - index; // - 2 because we saved the number of args between ebp and locals
}

inline static aint * arg(const unsigned int index) {
  const aint num_args = UNBOX(*(state.ebp - 1));
  return state.ebp + 3 + num_args - 1 - index; // + 3 because we saved ebp and ip of the caller and any function has an implicit first closure argument
}

inline static data * safe_retrieve_closure(const aint closure_ptr) {
  ASSERT_BOXED("CALLC", closure_ptr);
  data * closure = TO_DATA(closure_ptr);
  if (TAG(closure->data_header) != CLOSURE_TAG) {
    failure("Expected closure, got %d tag\n", TAG(closure->data_header));
  }
  return closure;
}

inline static aint * closure(const unsigned int index) {
  const aint closure_ptr = *(state.ebp + 2);
  const data * closure = safe_retrieve_closure(closure_ptr);
  const ptrt captured_vars_num = LEN(closure->data_header) - 1;
  if (index >= captured_vars_num) {
    failure("Closure variable %d out of bounds. Number of vars in closure is %d", index, captured_vars_num);
  }
  return &((aint *) closure->contents)[1 + index]; // 1 + because the first arg of every closure is an offset
}

inline static int read(const unsigned int bytes) {
  if (state.ip + bytes > state.bf->code_ptr + state.bf->code_size) {
    failure("When reading %d bytes IP counter %d can move outside of the code section of size\n", bytes, state.ip,
            state.bf->code_size);
  }
  state.ip += bytes;
  return *(int *)(state.ip - bytes);
}

inline static void jump(const unsigned int offset) {
  if (offset >= state.bf->code_size) {
    failure("Jump with offset %d is outside of code section of size %d\n", offset, state.bf->code_size);
  }
  state.ip = state.bf->code_ptr + offset;
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

inline static aint * var(const unsigned char designation, const unsigned int index, const unsigned char h, const unsigned char l) {
  switch (designation) {
    case GLOBAL:
      DEBUG_LOG("G(%d)", index);
      return global(index);
    case LOCAL:
      DEBUG_LOG("L(%d)", index);
      return local(index);
    case ARG:
      DEBUG_LOG("A(%d)", index);
      return arg(index);
    case CLOSURE_VAR:
      DEBUG_LOG("C(%d)", index);
      return closure(index);
    default:
      FAIL;
  }
}

inline static void eval_binop(unsigned char op);

/* Disassembles the bytecode pool */
void interpret(const bytefile *bf) {
  state.ip = bf->code_ptr + bf->entrypoint_offset;
  state.ebp = bf->stack_ptr;
  state.bf = bf;

  #ifdef DEBUG_PRINT
  static const char* const ops[] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
  static const char* const pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
  #endif
  do {
    const unsigned char x = BYTE, h = (x & 0xF0) >> 4, l = x & 0x0F;
    #ifdef DEBUG_PRINT
      dump_stack();
    #endif
    DEBUG_LOG("0x%.8x:\t", state.ip - state.bf->code_ptr - 1);
    switch (h) {
      case STOP:
        goto stop;

      case BINOP:
        DEBUG_LOG("BINOP\t%s", ops[l - 1]);
        eval_binop(l - 1);
        break;

      case CONST:
        switch (l) {
          case CONST_INT: {
            const aint value = INT;
            DEBUG_LOG("CONST\t%d", value);
            push(BOX(value));
            break;
          }

          case CONST_STRING: {
            const char * s = STRING;
            DEBUG_LOG("STRING\t%s", s);
            push((aint) Bstring((aint *) &s));
            break;
          }

          case MAKE_SEXP: {
            const char * tag = STRING;
            const unsigned int n = INT;
            DEBUG_LOG("SEXP\t%s ", tag);
            if (__gc_stack_top + n * sizeof(aint) > (size_t) state.bf->stack_ptr) {
              failure("Invalid sexpr length %d at %ip", n, state.ip);
            }
            push(LtagHash((char *) tag));
            const aint result = (aint) Bsexp_reversed(ESP, BOX(n + 1));
            __gc_stack_top += (n + 1) * sizeof(size_t);
            push(result);
            DEBUG_LOG("%d", n);
            break;
          }

          case STI: {
            DEBUG_LOG("STI");
            failure("Should not happen. Indirect assignments are temporarily prohibited.\n");
          }

          case STA: {
              DEBUG_LOG("STA");
              const aint value = pop();
              const aint index = pop();
              const aint array = pop();
              push((aint) Bsta((void *) array, index, (void *) value));
              break;
          }

          case JMP: {
            const int offset = INT;
            DEBUG_LOG("JMP\t0x%.8x", offset);
            jump(offset);
            break;
          }

          case END:
          case RET: {
            DEBUG_LOG("END/RET");
            if (state.ebp == bf->stack_ptr) goto stop; // Exiting the main function
            const aint return_value = pop();
            const int args_num = UNBOX(*(state.ebp - 1));
            aint * old_ebp = (aint *) *state.ebp;
            state.ip = (char *) *(state.ebp + 1);
            __gc_stack_top = (size_t) (state.ebp + 3 + args_num - 1); // Pop return address, base pointer of parent function, closure and args
            state.ebp = old_ebp;
            push(return_value);
            break;
          }

          case DROP:
            DEBUG_LOG("DROP");
            pop();
            break;

          case DUP: {
            DEBUG_LOG("DUP");
            const aint value = pop();
            push(value);
            push(value);
            break;
          }

          case SWAP: {
            DEBUG_LOG("SWAP");
            const aint a = pop();
            const aint b = pop();
            push(a);
            push(b);
            break;
          }

          case ELEM: {
            DEBUG_LOG("ELEM");
            const aint index = pop();
            void * array = (void *) pop();
            push((aint) Belem(array, index));
            break;
          }

          default:
            FAIL;
        }
        break;

      case LD: {
        DEBUG_LOG("LD\t");
        const int index = INT;
        const aint value = *var(l, index, h, l);
        DEBUG_LOG("=%d", value);
        push(value);
        break;
      }
      case LDA: {
        DEBUG_LOG("LDA\t");
        failure("Should not happen. Indirect assignments are temporarily prohibited.\n");
      }
      case ST: {
        DEBUG_LOG("ST\t");
        const int index = INT;
        *var(l, index, h, l) = *ESP;
        break;
      }

      case CONTROL:
        switch (l) {
          case CJMPz: {
            const aint offset = INT;
            DEBUG_LOG("CJMPz\t0x%.8x", offset);
            const aint value = UNBOX(pop());
            if (value == 0) {
              jump(offset);
            }
            break;
          }

          case CJMPnz: {
            const aint offset = INT;
            DEBUG_LOG("CJMPnz\t0x%.8x", offset);
            const aint value = UNBOX(pop());
            if (value != 0) {
              jump(offset);
            }
            break;
          }

          case BEGIN:
          case CBEGIN: {
            const int args_num = INT;
            const int locals_num = INT;
            DEBUG_LOG("BEGIN\t%d ", args_num);
            DEBUG_LOG("%d", locals_num);
            push(BOX(args_num));
            push(BOX(locals_num));
            for (int i = 0; i < locals_num; i++) {
              push(EMPTY);
            }
            break;
          }

          case MAKE_CLOSURE: {
            const int offset = INT;
            const unsigned int vars_num = INT;
            DEBUG_LOG("CLOSURE\t0x%.8x\t%d", offset, vars_num);
            aint args[vars_num + 1];
            args[0] = offset;
            for (int i = 1; i < vars_num + 1; i++) {
              const char designation = BYTE;
              const char index = INT;
              args[i] = *var(designation, index, h, l);
            }
            push((aint) Bclosure(args, BOX(vars_num)));
            break;
          }

          case CALLC: {
            const int args_num = INT;
            DEBUG_LOG("CALLC\t%d", args_num);
            if (__gc_stack_top + args_num * sizeof(aint) > (size_t) state.bf->stack_ptr) {
              failure("CALLC have invalid number of arguments %d at %ip", args_num, state.ip);
            }
            const aint closure_ptr = *(ESP + args_num);
            for (int i = args_num - 1; i >= 0; i--) {
              *(ESP + i + 1) = *(ESP + i);
            }
            *ESP = closure_ptr;
            const data * closure = safe_retrieve_closure(closure_ptr);
            const aint offset = ((aint *) closure->contents)[0];
            push((aint) state.ip);
            push((aint) state.ebp);
            state.ebp = ESP;
            jump(offset);
            break;
          }

          case CALL: {
            const int offset = INT;
            const int locals_num = INT;
            DEBUG_LOG("CALL\t0x%.8x %d", offset, locals_num);
            push(EMPTY); // Space for closure. Not empty in CALLC
            push((aint) state.ip);
            push((aint) state.ebp);
            state.ebp = ESP;
            jump(offset);
            break;
          }

          case TAG: {
            const char * tag = STRING;
            const int len = INT;
            DEBUG_LOG("TAG\t%s %d", tag, len);
            push(Btag((void *) pop(), LtagHash((char *) tag), BOX(len)));
            break;
          }

          case MAKE_ARRAY: {
            const int n = INT;
            DEBUG_LOG("ARRAY\t%d", n);
            push(Barray_patt((void*) pop(), BOX(n)));
            break;
          }

          case FAIL_I: {
            const int line = INT;
            const int col = INT;
            DEBUG_LOG("FAIL\t%d", line);
            DEBUG_LOG("%d", col);
            failure("Lama failure at (%d, %d)\n", line, col);
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
            push(Bstring_patt((void *) pop(), (void *) pop()));
            break;
          case PATT_STRING:
            push(Bstring_tag_patt((void *) pop()));
            break;
          case PATT_ARRAY:
            push(Barray_tag_patt((void *) pop()));
            break;
          case PATT_SEXP:
            push(Bsexp_tag_patt((void *) pop()));
            break;
          case PATT_BOXED:
            push(Bboxed_patt((void *) pop()));
            break;
          case PATT_UNBOXED:
            push(Bunboxed_patt((void *) pop()));
            break;
          case PATT_CLOSURE:
            push(Bclosure_tag_patt((void *) pop()));
            break;
          default:
            FAIL;
        }
        break;

      case BUILTIN: {
        switch (l) {
          case BUILTIN_Lread:
            DEBUG_LOG("CALL\tLread");
            push(Lread());
            break;

          case BUILTIN_Lwrite:
            DEBUG_LOG("CALL\tLwrite");
            push(BOX(Lwrite(pop())));
            break;

          case BUILTIN_Llength: {
            DEBUG_LOG("CALL\tLlength");
            push(Llength((void *) pop()));
            break;
          }

          case BUILTIN_Lstring:
            DEBUG_LOG("CALL\tLstring");
            push((aint) Lstring(ESP));
            break;

          case BUILTIN_Barray: {
            const unsigned int len = INT;
            DEBUG_LOG("CALL\tBarray %d", len);
            if (__gc_stack_top + len * sizeof(aint) > (size_t) state.bf->stack_ptr) {
              failure("Invalid array length %d at ip %d\n", len, state.ip);
            }
            const aint result = (aint) Barray_reversed(ESP, BOX(len));
            __gc_stack_top += len * sizeof(size_t);
            push(result);
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

enum Binop {
  ADD, SUB, MUL, DIV, MOD, LT, LTE, GT, GTE, EQ, NEQ, AND, OR
};

inline static void eval_binop(const unsigned char op) {
  void *q = (void *) pop();
  void *p = (void *) pop();
  DEBUG_LOG("\nBinop with args: %ld, %ld", UNBOX(p), UNBOX(q));
  switch (op) {
    case ADD:
      ASSERT_UNBOXED("captured +:1", p);
      ASSERT_UNBOXED("captured +:2", q);

      push(BOX(UNBOX(p) + UNBOX(q)));
      break;
    case SUB:
      if (UNBOXED(p)) {
        ASSERT_UNBOXED("captured -:2", q);
        push(BOX(UNBOX(p) - UNBOX(q)));
        break;
      }

      ASSERT_BOXED("captured -:1", q);
      push(BOX(p - q));
      break;
    case MUL:
      ASSERT_UNBOXED("captured *:1", p);
      ASSERT_UNBOXED("captured *:2", q);

      push(BOX(UNBOX(p) * UNBOX(q)));
      break;
    case DIV:
      ASSERT_UNBOXED("captured /:1", p);
      ASSERT_UNBOXED("captured /:2", q);
      if (q == 0) {
        failure("Division by zero\n");
      }
      push(BOX(UNBOX(p) / UNBOX(q)));
      break;
    case MOD:
      ASSERT_UNBOXED("captured %:1", p);
      ASSERT_UNBOXED("captured %:2", q);

      push(BOX(UNBOX(p) % UNBOX(q)));
      break;
    case LT:
      ASSERT_UNBOXED("captured <:1", p);
      ASSERT_UNBOXED("captured <:2", q);

      push(BOX(UNBOX(p) < UNBOX(q)));
      break;
    case LTE:
      ASSERT_UNBOXED("captured <=:1", p);
      ASSERT_UNBOXED("captured <=:2", q);

      push(BOX(UNBOX(p) <= UNBOX(q)));
      break;
    case GT:
      ASSERT_UNBOXED("captured >:1", p);
      ASSERT_UNBOXED("captured >:2", q);

      push(BOX(UNBOX(p) > UNBOX(q)));
      break;
    case GTE:
      ASSERT_UNBOXED("captured >=:1", p);
      ASSERT_UNBOXED("captured >=:2", q);

      push(BOX(UNBOX(p) >= UNBOX(q)));
      break;
    case EQ:
      push(BOX(p == q));
      break;
    case NEQ:
      ASSERT_UNBOXED("captured !=:1", p);
      ASSERT_UNBOXED("captured !=:2", q);

      push(BOX(UNBOX(p) != UNBOX(q)));
      break;
    case AND:
      ASSERT_UNBOXED("captured &&:1", p);
      ASSERT_UNBOXED("captured &&:2", q);

      push(BOX(UNBOX(p) && UNBOX(q)));
      break;
    case OR:
      ASSERT_UNBOXED("captured !!:1", p);
      ASSERT_UNBOXED("captured !!:2", q);

      push(BOX(UNBOX(p) || UNBOX(q)));
      break;
    default:
      failure("Unknown binop %d\n", op);
  }
  DEBUG_LOG("\nBinop res: %ld", UNBOX(*ESP));
}
