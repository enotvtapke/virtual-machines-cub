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
  --state.esp;
  *state.esp = value;
  __gc_stack_top = (size_t) state.esp;
}

#define EMPTY BOX(0)

static aint pop() {
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
  return *(state.ebp - 1 - index); // - 1 because we saved the number of args between ebp and locals
}

static void set_local(const int index, const aint value) {
  *(state.ebp - 1 - index) = value;
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
  void * a = (void*) pop();
  void * b = (void*) pop();
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
}

#define INT (state.ip += sizeof(int), *(int *)(state.ip - sizeof(int)))
#define BYTE *(state.ip)++
#define STRING get_string(state.bf, INT)
#define FAIL failure("ERROR: invalid opcode %d-%d\n", h, l)

static aint get_var(FILE *f, const char designation, const int index, const char h, const char l) {
  switch (designation) {
    case 0:
      fprintf(f, "G(%d)", index);
      return get_global(index);
    case 1:
      fprintf(f, "L(%d)", index);
      return get_local(index);
    case 2:
      fprintf(f, "A(%d)", index);
      return get_arg(index);
    case 3:
      fprintf(f, "C(%d)", index);
      return get_closure(index);
    default:
      FAIL;
  }
}

static void set_var(FILE *f, const char designation, const int index, const aint value, const char h, const char l) {
  switch (designation) {
    case 0:
      fprintf(f, "G(%d)", index);
      set_global(index, value);
      break;
    case 1:
      fprintf(f, "L(%d)", index);
      set_local(index, value);
      break;
    case 2:
      fprintf(f, "A(%d)", index);
      set_arg(index, value);
      break;
    case 3:
      fprintf(f, "C(%d)", index);
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

    fprintf(f, "0x%.8x:\t", state.ip - state.bf->code_ptr - 1);

    switch (h) {
      case 15:
        goto stop;

      /* BINOP */
      case 0:
        fprintf(f, "BINOP\t%s", ops[l - 1]);
        eval_binop(l - 1);
        break;

      case 1:
        switch (l) {
          case 0: {
            const aint value = INT;
            fprintf(f, "CONST\t%d", value);
            push(BOX(value));
            break;
          }

          case 1: {
            char * s = STRING;
            fprintf(f, "STRING\t%s", s);
            push((aint) Bstring((aint *) &s));
            break;
          }

          case 2: {
            char * tag = STRING;
            int n = INT;
            fprintf(f, "SEXP\t%s ", tag);

            aint args[n + 1]; // I could not use args if the stack grew upwards
            for (int i = 0; i < n; i++) {
              args[i] = pop();
            }
            args[n] = LtagHash(tag);
            push((aint) Bsexp(args, BOX(n + 1)));
            fprintf(f, "%d", n);
            break;
          }

          case 3: {
            fprintf(f, "STI");
            failure("Should not happen. Indirect assignments are temporarily prohibited.");
            break;
          }

          case 4: {
              fprintf(f, "STA");
              const aint value = pop();
              const aint index = pop();
              const aint array = pop();
              push((aint) Bsta((void *) array, index, (void *) value));
              break;
          }

          case 5: {
            const int offset = INT;
            fprintf(f, "JMP\t0x%.8x", offset);
            state.ip = bf->code_ptr + offset;
            break;
          }

          case 6:
          case 7: {
            fprintf(f, "END/RET");
            if (state.ebp == bf->stack_ptr) break; // Exiting main function
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
            fprintf(f, "DROP");
            pop();
            break;

          case 9: {
            fprintf(f, "DUP");
            const aint value = pop();
            push(value);
            push(value);
            break;
          }

          case 10: {
            fprintf(f, "SWAP");
            const aint a = pop();
            const aint b = pop();
            push(a);
            push(b);
            break;
          }

          case 11: {
            fprintf(f, "ELEM");
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
        fprintf(f, "LD\t");
        push(get_var(f, l, INT, h, l));
        break;
      }
      case 3: {
        fprintf(f, "LDA\t");
        failure("Should not happen. Indirect assignments are temporarily prohibited.");
        break;
      }
      case 4: {
        fprintf(f, "ST\t");
        set_var(f, l, INT, *state.esp, h, l);
        break;
      }

      case 5:
        switch (l) {
          case 0: {
            const aint offset = INT;
            fprintf(f, "CJMPz\t0x%.8x", offset);
            const aint value = UNBOX(pop());
            if (value == 0) {
              state.ip = bf->code_ptr + offset;
            }
            break;
          }

          case 1: {
            const aint offset = INT;
            fprintf(f, "CJMPnz\t0x%.8x", offset);
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
            fprintf(f, "BEGIN\t%d ", args_num);
            fprintf(f, "%d", locals_num);
            push(BOX(args_num));
            for (int i = 0; i < locals_num; i++) {
              push(EMPTY);
            }
            break;
          }

          // case 3: {
          //   fprintf(f, "CBEGIN\t%d ", INT);
          //   fprintf(f, "%d", INT);
          //   failure("Should not happen.");
          //   break;
          // }

          case 4: {
            const int offset = INT;
            fprintf(f, "CLOSURE\t0x%.8x", offset);
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
            fprintf(f, "CALLC\t%d", args_num);

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
            fprintf(f, "CALL\t0x%.8x ", offset);
            fprintf(f, "%d", locals_num);
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
            fprintf(f, "TAG\t%s ", tag);
            fprintf(f, "%d", len);
            push(Btag((void *) pop(), LtagHash(tag), len));
            break;
          }

          case 8: {
            const int n = INT;
            fprintf(f, "ARRAY\t%d", n);
            push(Barray_patt((void*) pop, n));
            break;
          }

          case 9: {
            const int line = INT;
            const int col = INT;
            fprintf(f, "FAIL\t%d", line);
            fprintf(f, "%d", col);
            failure("Lama failure at (%d, %d)", line, col);
            break;
          }

          case 10:
            fprintf(f, "LINE\t%d", INT);
            break;

          default:
            FAIL;
        }
        break;

      case 6:
        fprintf(f, "PATT\t%s", pats[l]);
        switch (l) {
          case 0:
            Bstring_patt((void *) pop(), (void *) pop());
            break;
          case 1:
            Bstring_tag_patt((void *) pop());
            break;
          case 2:
            Barray_tag_patt((void *) pop());
            break;
          case 3:
            Bboxed_patt((void *) pop());
            break;
          case 4:
            Bunboxed_patt((void *) pop());
            break;
          case 5:
            Bclosure_tag_patt((void *) pop());
            break;
          default:
            FAIL;
        }
        break;

      case 7: {
        switch (l) {
          case 0:
            fprintf(f, "CALL\tLread");
            push(Lread());
            break;

          case 1:
            fprintf(f, "CALL\tLwrite");
            Lwrite(pop());
            break;

          case 2:
            fprintf(f, "CALL\tLlength");
            push(Llength((void *) pop()));
            break;

          case 3:
            fprintf(f, "CALL\tLstring");
            push((aint) Lstring(state.esp));
            pop();
            break;

          case 4: {
            const int len = INT;
            fprintf(f, "CALL\tBarray\t%d", len);
            aint arr[len];
            for (int i = 0; i < len; i++) {
              arr[len - 1 - i] = pop();
            }
            Barray(arr, len);
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

    fprintf(f, "\n");
  } while (1);
stop:
  fprintf(f, "<end>\n");
}