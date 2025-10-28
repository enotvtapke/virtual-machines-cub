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
    failure("Stack overflow\n");
  }
  __gc_stack_top -= sizeof(size_t);
  *ESP = value;
}

inline static aint get_locals_num() {
  return UNBOX(*(state.ebp - 2));
}

inline static aint pop() {
  if (ESP >= state.ebp - 3 - (get_locals_num() - 1)) {
    failure("Popping values from stack frame (locals or worse)\n");
  }
  if (ESP >= state.bf->stack_ptr) {
    failure("Stack underflow\n");
  }
  __gc_stack_top += sizeof(size_t);
  return *(ESP - 1);
}

inline static aint get_global(const int index) {
  if (index >= state.bf->global_area_size) {
    failure("Global variable %d out of bounds. Number of globals %d\n", index, state.bf->global_area_size);
  }
  return state.bf->global_ptr[index];
}

inline static void set_global(const int index, const aint value) {
  if (index >= state.bf->global_area_size) {
    failure("Global variable %d out of bounds. Number of globals %d\n", index, state.bf->global_area_size);
  }
  state.bf->global_ptr[index] = value;
}

inline static aint get_local(const int index) {
  if (index >= get_locals_num()) {
    failure("Local variable %d out of bounds. Number of locals %d\n", index, get_locals_num());
  }
  return *(state.ebp - 3 - index); // - 2 because we saved the number of args between ebp and locals
}

inline static void set_local(const int index, const aint value) {
  if (index >= get_locals_num()) {
    failure("Local variable %d out of bounds. Number of locals %d\n", index, get_locals_num());
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
    failure("Expected closure, got %d tag\n", TAG(closure->data_header));
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
  if (state.ip < state.bf->code_ptr || state.ip >= state.bf->code_ptr + state.bf->code_size) {
    failure("ip counter is outside of code section\n");
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
  state.ip = bf->code_ptr + bf->entrypoint_offset;
  state.ebp = bf->stack_ptr;
  state.bf = bf;

  #ifdef DEBUG_PRINT
  static const char* const ops[] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
  static const char* const pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
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

            push(LtagHash(tag));
            const aint result = (aint) Bsexp_reversed(ESP, BOX(n + 1));
            __gc_stack_top += (n + 1) * sizeof(size_t);
            push(result);
            DEBUG_LOG("%d", n);
            break;
          }

          case 3: {
            DEBUG_LOG("STI");
            failure("Should not happen. Indirect assignments are temporarily prohibited.\n");
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
        failure("Should not happen. Indirect assignments are temporarily prohibited.\n");
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
              check_code_size();
            }
            break;
          }

          case 1: {
            const aint offset = INT;
            DEBUG_LOG("CJMPnz\t0x%.8x", offset);
            const aint value = UNBOX(pop());
            if (value != 0) {
              state.ip = bf->code_ptr + offset;
              check_code_size();
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
            failure("Lama failure at (%d, %d)\n", line, col);
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
  fprintf(f, "<done>\n");
}

enum Binop {
  ADD, SUB, MUL, DIV, MOD, LT, LTE, GT, GTE, EQ, NEQ, AND, OR
};

inline static void eval_binop(const char op) {
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
