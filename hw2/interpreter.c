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
  bytefile *bf;
} State;

static State state;

#define ESP (((aint *) __gc_stack_top) + 1)

static void print_stack_value(const aint v) {
  if (UNBOXED(v)) {
    DEBUG_LOG("%d", UNBOX(v));
  } else {
    data * d = TO_DATA(v);
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
    failure("Stack overflow");
  }
  __gc_stack_top -= sizeof(size_t);
  *ESP = value;
}

inline static aint get_locals_num() {
  return UNBOX(*(state.ebp - 2));
}

inline static aint pop() {
  if (ESP >= state.ebp - 3 - (get_locals_num() - 1)) {
    failure("Popping values from stack frame (locals or worse)");
  }
  if (ESP >= state.bf->stack_ptr) { // TODO I could check popping locals if I saved the number of locals
    failure("Stack underflow");
  }
  __gc_stack_top += sizeof(size_t);
  return *(ESP - 1);
}

inline static aint get_global(const int index) {
  if (index >= state.bf->global_area_size) {
    failure("Global variable %d out of bounds. Number of globals %d", index, state.bf->global_area_size);
  }
  return state.bf->global_ptr[index];
}

inline static void set_global(const int index, const aint value) {
  if (index >= state.bf->global_area_size) {
    failure("Global variable %d out of bounds. Number of globals %d", index, state.bf->global_area_size);
  }
  state.bf->global_ptr[index] = value;
}

inline static aint get_local(const int index) {
  if (index >= get_locals_num()) {
    failure("Local variable %d out of bounds. Number of locals %d", index, get_locals_num());
  }
  return *(state.ebp - 3 - index); // - 2 because we saved the number of args between ebp and locals
}

inline static void set_local(const int index, const aint value) {
  if (index >= get_locals_num()) {
    failure("Local variable %d out of bounds. Number of locals %d", index, get_locals_num());
  }
  *(state.ebp - 3 - index) = value;
}

inline static aint get_arg(const int index) {
  const aint num_args = UNBOX(*(state.ebp - 1)); // TODO maybe slow because args in wrong order
  return *(state.ebp + 3 + num_args - 1 - index); // + 3 because we saved ebp and ip of the caller and any function has an implicit first closure argument
}

inline static void set_arg(const int index, const aint value) {
  const aint num_args = UNBOX(*(state.ebp - 1));
  *(state.ebp + 3 + num_args - 1 - index) = value;
}

inline static data * safe_retrieve_closure(const aint closure_ptr) {
  ASSERT_BOXED("CALLC", closure_ptr);
  data * closure = TO_DATA(closure_ptr);
  if (TAG(closure->data_header) != CLOSURE_TAG) {
    failure("Expected closure, got %d tag", TAG(closure->data_header));
  }
  return closure;
}

inline static aint get_closure(const int index) {
  const aint closure_ptr = *(state.ebp + 2);
  const data * closure = safe_retrieve_closure(closure_ptr);
  return ((aint *) closure->contents)[1 + index]; // 1 + because the first arg of every closure is an offset
}

inline static void set_closure(const int index, const aint value) {
  const aint closure_ptr = *(state.ebp + 2);
  const data * closure = safe_retrieve_closure(closure_ptr);
  ((aint *) closure->contents)[1 + index] = value;
}

inline static void check_code_size() {
  if (state.ip >= state.bf->code_ptr + state.bf->code_size) {
    failure("Reached end of code section");
  }
}

#define INT (check_code_size(), state.ip += sizeof(int), *(int *)(state.ip - sizeof(int)))
#define BYTE (check_code_size(), *(state.ip)++)
#define STRING get_string(state.bf, INT)
#define FAIL failure("ERROR: invalid opcode %d-%d\n", h, l)

inline static aint get_var(FILE *f, const char designation, const int index, const char h, const char l) {
  switch (designation) {
    case 0:
      DEBUG_LOG("G(%d)", index);
      return get_global(index);
    case 1:
      DEBUG_LOG("L(%d)", index);
      return get_local(index);
    case 2:
      DEBUG_LOG("A(%d)", index);
      return get_arg(index);
    case 3:
      DEBUG_LOG("C(%d)", index);
      return get_closure(index);
    default:
      FAIL;
  }
}

inline static void set_var(FILE *f, const char designation, const int index, const aint value, const char h, const char l) {
  switch (designation) {
    case 0:
      DEBUG_LOG("G(%d)=%d", index, value);
      set_global(index, value);
      break;
    case 1:
      DEBUG_LOG("L(%d)=%d", index, value);
      set_local(index, value);
      break;
    case 2:
      DEBUG_LOG("A(%d)=%d", index, value);
      set_arg(index, value);
      break;
    case 3:
      DEBUG_LOG("C(%d)=%d", index, value);
      set_closure(index, value);
      break;
    default:
      FAIL;
  }
}

inline static void eval_binop(char op);

/* Disassembles the bytecode pool */
void interpret(FILE *f, bytefile *bf) {
  state.ip = bf->code_ptr;
  state.ebp = bf->stack_ptr;
  state.bf = bf;

  #ifdef DEBUG_PRINT
  char *ops[] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
  char *pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
  #endif
  do {
    const char x = BYTE, h = (x & 0xF0) >> 4, l = x & 0x0F;
    #ifdef DEBUG_PRINT
      dump_stack();
    #endif
    DEBUG_LOG("0x%.8x:\t", state.ip - state.bf->code_ptr - 1);
    switch (h) {
      case 15:
        goto stop;

      /* BINOP */
      case 0:
        DEBUG_LOG("BINOP\t%s", ops[l - 1]);
        eval_binop(l - 1);
        break;

      case 1:
        switch (l) {
          case 0: {
            const aint value = INT;
            DEBUG_LOG("CONST\t%d", value);
            push(BOX(value));
            break;
          }

          case 1: {
            char * s = STRING;
            DEBUG_LOG("STRING\t%s", s);
            push((aint) Bstring((aint *) &s));
            break;
          }

          case 2: {
            char * tag = STRING;
            const int n = INT;
            DEBUG_LOG("SEXP\t%s ", tag);

            aint args[n + 1]; // TODO I could not reverse args if the stack grew upwards
            for (int i = 0; i < n; i++) {
              args[n - i - 1] = pop();
            }
            args[n] = LtagHash(tag);
            push((aint) Bsexp(args, BOX(n + 1)));
            DEBUG_LOG("%d", n);
            break;
          }

          case 3: {
            DEBUG_LOG("STI");
            failure("Should not happen. Indirect assignments are temporarily prohibited.");
            break;
          }

          case 4: {
              DEBUG_LOG("STA");
              const aint value = pop();
              const aint index = pop();
              const aint array = pop();
              push((aint) Bsta((void *) array, index, (void *) value));
              break;
          }

          case 5: {
            const int offset = INT;
            DEBUG_LOG("JMP\t0x%.8x", offset);
            state.ip = bf->code_ptr + offset;
            break;
          }

          case 6:
          case 7: {
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

          case 8:
            DEBUG_LOG("DROP");
            pop();
            break;

          case 9: {
            DEBUG_LOG("DUP");
            const aint value = pop();
            push(value);
            push(value);
            break;
          }

          case 10: {
            DEBUG_LOG("SWAP");
            const aint a = pop();
            const aint b = pop();
            push(a);
            push(b);
            break;
          }

          case 11: {
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

      case 2: {
        DEBUG_LOG("LD\t");
        const int index = INT;
        const aint value = get_var(f, l, index, h, l);
        DEBUG_LOG("=%d", value);
        push(value);
        break;
      }
      case 3: {
        DEBUG_LOG("LDA\t");
        failure("Should not happen. Indirect assignments are temporarily prohibited.");
        break;
      }
      case 4: {
        DEBUG_LOG("ST\t");
        const int index = INT;
        set_var(f, l, index, *ESP, h, l);
        break;
      }

      case 5:
        switch (l) {
          case 0: {
            const aint offset = INT;
            DEBUG_LOG("CJMPz\t0x%.8x", offset);
            const aint value = UNBOX(pop());
            if (value == 0) {
              state.ip = bf->code_ptr + offset;
            }
            break;
          }

          case 1: {
            const aint offset = INT;
            DEBUG_LOG("CJMPnz\t0x%.8x", offset);
            const aint value = UNBOX(pop());
            if (value != 0) {
              state.ip = bf->code_ptr + offset;
            }
            break;
          }

          case 2:
          case 3: {
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

          case 4: {
            const int offset = INT;
            const int vars_num = INT;
            DEBUG_LOG("CLOSURE\t0x%.8x\t%d", offset, vars_num);
            aint args[vars_num + 1];
            args[0] = offset;
            for (int i = 1; i < vars_num + 1; i++) {
              const char designation = BYTE;
              const char index = INT;
              args[i] = get_var(f, designation, index, h, l);
            }
            push((aint) Bclosure(args, BOX(vars_num)));
            break;
          }

          case 5: {
            const int args_num = INT;
            DEBUG_LOG("CALLC\t%d", args_num);

            // TODO Can be slow. It is better to store the closure as the last argument, not first
            aint args[args_num];
            for (int i = 0; i < args_num; i++) {
              args[i] = pop();
            }
            const aint closure_ptr = pop();
            for (int i = 0; i < args_num; i++) {
              push(args[i]);
            }
            push(closure_ptr);
            const data * closure = safe_retrieve_closure(closure_ptr);
            // ====
            const aint offset = ((aint *) closure->contents)[0];
            push((aint) state.ip);
            push((aint) state.ebp);
            state.ebp = ESP;
            state.ip = state.bf->code_ptr + offset;
            break;
          }

          case 6: {
            const int offset = INT;
            const int locals_num = INT;
            DEBUG_LOG("CALL\t0x%.8x %d", offset, locals_num);
            push(EMPTY); // Space for closure. Not empty in CALLC
            push((aint) state.ip);
            push((aint) state.ebp);
            state.ebp = ESP;
            state.ip = state.bf->code_ptr + offset;
            break;
          }

          case 7: {
            char * tag = STRING;
            const int len = INT;
            DEBUG_LOG("TAG\t%s %d", tag, len);
            push(Btag((void *) pop(), LtagHash(tag), BOX(len)));
            break;
          }

          case 8: {
            const int n = INT;
            DEBUG_LOG("ARRAY\t%d", n);
            push(Barray_patt((void*) pop(), BOX(n)));
            break;
          }

          case 9: {
            const int line = INT;
            const int col = INT;
            DEBUG_LOG("FAIL\t%d", line);
            DEBUG_LOG("%d", col);
            failure("Lama failure at (%d, %d)", line, col);
            break;
          }

          case 10: {
            int line = INT;
            DEBUG_LOG("LINE\t%d", line);
            break;
          }

          default:
            FAIL;
        }
        break;

      case 6:
        DEBUG_LOG("PATT\t%s", pats[l]);
        switch (l) {
          case 0:
            push(Bstring_patt((void *) pop(), (void *) pop()));
            break;
          case 1:
            push(Bstring_tag_patt((void *) pop()));
            break;
          case 2:
            push(Barray_tag_patt((void *) pop()));
            break;
          case 3:
            push(Bsexp_tag_patt((void *) pop()));
            break;
          case 4:
            push(Bboxed_patt((void *) pop()));
            break;
          case 5:
            push(Bunboxed_patt((void *) pop()));
            break;
          case 6:
            push(Bclosure_tag_patt((void *) pop()));
            break;
          default:
            FAIL;
        }
        break;

      case 7: {
        switch (l) {
          case 0:
            DEBUG_LOG("CALL\tLread");
            push(Lread());
            break;

          case 1:
            DEBUG_LOG("CALL\tLwrite");
            push(BOX(Lwrite(pop())));
            break;

          case 2: {
            DEBUG_LOG("CALL\tLlength");
            push(Llength((void *) pop()));
            break;
          }

          case 3:
            DEBUG_LOG("CALL\tLstring");
            push((aint) Lstring(ESP));
            break;

          case 4: {
            const int len = INT;
            DEBUG_LOG("CALL\tBarray %d", len);
            aint arr[len];
            for (int i = 0; i < len; i++) {
              arr[len - 1 - i] = pop();
            }
            push((aint) Barray(arr, BOX(len)));
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
  fprintf(f, "<done>\n");
}

enum Binop {
  ADD, SUB, MUL, DIV, MOD, LT, LTE, GT, GTE, EQ, NEQ, AND, OR
};

inline static void eval_binop(const char op) {
  void *b = (void *) pop();
  void *a = (void *) pop();
  DEBUG_LOG("\nBinop with args: %d, %d", UNBOX(a), UNBOX(b));
  switch (op) {
    case ADD:
      push(Ls__Infix_43(a, b));
      break;
    case SUB:
      push(Ls__Infix_45(a, b));
      break;
    case MUL:
      push(Ls__Infix_42(a, b));
      break;
    case DIV:
      push(Ls__Infix_47(a, b));
      break;
    case MOD:
      push(Ls__Infix_37(a, b));
      break;
    case LT:
      push(Ls__Infix_60(a, b));
      break;
    case LTE:
      push(Ls__Infix_6061(a, b));
      break;
    case GT:
      push(Ls__Infix_62(a, b));
      break;
    case GTE:
      push(Ls__Infix_6261(a, b));
      break;
    case EQ:
      push(Ls__Infix_6161(a, b));
      break;
    case NEQ:
      push(Ls__Infix_3361(a, b));
      break;
    case AND:
      push(Ls__Infix_3838(a, b));
      break;
    case OR:
      push(Ls__Infix_3333(a, b));
      break;
    default:
      failure("Unknown binop %d", op);
  }
  DEBUG_LOG("\nBinop res: %d", UNBOX(*ESP));
}
