//
// Created by enotvtapke on 10/25/25.
//

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "interpreter.h"
// #include "./runtime/runtime.h"
#include "./runtime/runtime.c"

// typedef struct {
//   int locals_num;
//   aint* est;
//   aint* ebp;
// } Stack_frame;

#define DEBUG_PRINT

#ifdef DEBUG_PRINT
#define DEBUG_LOG(...) fprintf(stdout, __VA_ARGS__)
#else
#define DEBUG_LOG(...) (0)
#endif

typedef struct {
  char *ip;
  aint **closure; // Pointer to the current closure in heap. Is null when interpreting top level function or code
  aint *esp;
  aint *ebp;
  bytefile * bf;
  // Stack_frame *call_stack;
  // Stack_frame *frame_pointer;
} State;

static State state;

// static void push_frame(Stack_frame frame) {
//   state.call_stack
// };

static void push(const aint value) {
  // DEBUG_LOG("\nPUSH %d", value);
  --state.esp;
  *state.esp = value;
  __gc_stack_top = (size_t) state.esp;
}

#define EMPTY BOX(0)

static aint pop() {
  // DEBUG_LOG("\nPOP");
  // if (state.esp >= state.ebp - 1) {
  //   failure("Stack underflow");
  // }
  if (state.esp >= state.ebp - 2) {
    DEBUG_LOG("\nMAYBE POPPING LOCAL 0");
  }
  ++state.esp;
  __gc_stack_top = (size_t) state.esp;
  return *(state.esp - 1);
}

static aint get_global(const int index) {
  return state.bf->global_ptr[index];
}

static void set_global(const int index, const aint value) {
  state.bf->global_ptr[index] = value;
}

static aint get_local(const int index) {
  return *(state.ebp - 2 - index); // -K 2 because we saved the number of args between ebp and locals
}

static void set_local(const int index, const aint value) {
  *(state.ebp - 2 - index) = value;
}

static aint get_arg(const int index) {
  return *(state.ebp + 3 + index); // + 3 because we saved ebp and ip of the caller and any function has an implicit first closure argument
}

static void set_arg(const int index, const aint value) {
  *(state.ebp + 3 + index) = value;
}

static data * safe_retrieve_closure(const aint closure_ptr) {
  ASSERT_BOXED("CALLC", closure_ptr);
  data * closure = TO_DATA(closure_ptr);
  if (TAG(closure->data_header) != CLOSURE_TAG) {
    failure("Expected closure, got %d", TAG(closure->data_header));
  }
  return closure;
}

static aint get_closure(const int index) {
  const aint closure_ptr = *(state.ebp + 2);
  const data * closure = safe_retrieve_closure(closure_ptr);
  return ((aint *) closure->contents)[1 + index]; // 1 + because the first arg of every closure is an offset
}

static void set_closure(const int index, const aint value) {
  const aint closure_ptr = *(state.ebp + 2);
  const data * closure = safe_retrieve_closure(closure_ptr);
  ((aint *) closure->contents)[1 + index] = value;
}

// static int read_int() {
//   state.ip += sizeof(int);
//   return *(int *)(state.ip - sizeof(int));
// }
//
// static int read_byte() {
//   return *state.ip++;
// }
//
// static char* read_string() {
//   return get_string(state.bf, read_int());
// }

enum Binop {
  ADD, SUB, MUL, DIV, MOD, LT, LTE, GT, GTE, EQ, NEQ, AND, OR
};

static void eval_binop(const char op) {
  void * b = (void*) pop();
  void * a = (void*) pop();
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
  DEBUG_LOG("\nBinop res: %d", UNBOX(*state.esp));
}

#define INT (state.ip += sizeof(int), *(int *)(state.ip - sizeof(int)))
#define BYTE *(state.ip)++
#define STRING get_string(state.bf, INT)
#define FAIL failure("ERROR: invalid opcode %d-%d\n", h, l)

static aint get_var(FILE *f, const char designation, const int index, const char h, const char l) {
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

static void set_var(FILE *f, const char designation, const int index, const aint value, const char h, const char l) {
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

/* Disassembles the bytecode pool */
void interpret(FILE *f, bytefile *bf) {
  state.ip = bf->code_ptr;
  state.closure = NULL;
  state.esp = bf->stack_ptr;
  __gc_stack_top = (size_t) state.esp;
  state.ebp = bf->stack_ptr;
  state.bf = bf;
  // state.call_stack = malloc(sizeof(Stack_frame) * CALL_STACK_SIZE);

  // char *ip = state.bf->code_ptr;
  char *ops[] = {"+", "-", "*", "/", "%", "<", "<=", ">", ">=", "==", "!=", "&&", "!!"};
  char *pats[] = {"=str", "#string", "#array", "#sexp", "#ref", "#val", "#fun"};
  char *lds[] = {"LD", "LDA", "ST"};
  do {
    char x = BYTE,
        h = (x & 0xF0) >> 4,
        l = x & 0x0F;
    DEBUG_LOG("0x%.8x:\t", state.ip - state.bf->code_ptr - 1);
    // DEBUG_LOG("%d ", get_local(0));

    switch (h) {
      case 15:
        goto stop;

      /* BINOP */
      case 0:
        DEBUG_LOG("BINOP\t%s", ops[l - 1]);
        ops[l - 1];
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
            int n = INT;
            DEBUG_LOG("SEXP\t%s ", tag);

            aint args[n + 1]; // I could not use args if the stack grew upwards
            for (int i = 0; i < n; i++) {
              args[i] = pop();
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
            if (state.ebp == bf->stack_ptr) goto stop; // Exiting main function
            const aint res = pop();
            const int args_num = UNBOX(state.ebp - 1);
            state.esp = state.ebp;
            __gc_stack_top = (size_t) state.esp;
            state.ebp = (aint *) pop();
            state.ip = (char *) pop();
            pop(); // Pop the closure pointer
            for (int i = 0; i < args_num; i++) {
              pop();
            }
            push(res);
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
        const aint v = get_var(f, l, index, h, l);
        DEBUG_LOG("=%d", v);
        push(v);
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
        set_var(f, l, index, *state.esp, h, l);
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
            for (int i = 0; i < locals_num; i++) {
              push(EMPTY);
            }
            break;
          }

          // case 3: {
          //   DEBUG_LOG("CBEGIN\t%d ", INT);
          //   DEBUG_LOG("%d", INT);
          //   failure("Should not happen.");
          //   break;
          // }

          case 4: {
            const int offset = INT;
            DEBUG_LOG("CLOSURE\t0x%.8x", offset);
            const int vars_num = INT;
            aint args[vars_num + 1];
            args[0] = offset;
            for (int i = 1; i < vars_num + 1; i++) {
              char a = BYTE;
              char b = INT;
              args[i] = get_var(f, a, b, h, l);
            }
            push((aint) Bclosure(args, BOX(vars_num)));
            break;
          }

          case 5: {
            const int args_num = INT;
            DEBUG_LOG("CALLC\t%d", args_num);

            // Can be very slow
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
            state.ebp = state.esp;
            state.ip = state.bf->code_ptr + offset;
            break;
          }

          case 6: {
            const int offset = INT;
            const int locals_num = INT;
            DEBUG_LOG("CALL\t0x%.8x ", offset);
            DEBUG_LOG("%d", locals_num);
            push(EMPTY); // Space for closure. Occupied in CALLC
            push((aint) state.ip);
            push((aint) state.ebp);
            state.ebp = state.esp;
            state.ip = state.bf->code_ptr + offset;
            break;
          }

          case 7: {
            char * tag = STRING;
            const int len = INT;
            DEBUG_LOG("TAG\t%s ", tag);
            DEBUG_LOG("%d", len);
            push(Btag((void *) pop(), LtagHash(tag), len));
            break;
          }

          case 8: {
            const int n = INT;
            DEBUG_LOG("ARRAY\t%d", n);
            push(Barray_patt((void*) pop, n));
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
            push(Bboxed_patt((void *) pop()));
            break;
          case 4:
            push(Bunboxed_patt((void *) pop()));
            break;
          case 5:
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
            push(Lwrite(pop()));
            break;

          case 2: {
            DEBUG_LOG("CALL\tLlength");
            aint v = Llength((void *) pop());
            push(v);
            break;
          }

          case 3:
            DEBUG_LOG("CALL\tLstring");
            push((aint) Lstring(state.esp));
            pop();
            break;

          case 4: {
            const int len = INT;
            DEBUG_LOG("CALL\tBarray\t%d", len);
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
  fprintf(f, "<end>\n");
}