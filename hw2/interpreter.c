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
  *state.esp = value;
  --state.esp;
}

static aint pop() {
  ++state.esp;
  return *(state.esp - 1);
}

static void jump(const int offset) {
  state.ip = state.bf->code_ptr + offset;
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

/* Disassembles the bytecode pool */
void interpret(FILE *f, bytefile *bf) {
#define INT (state.ip += sizeof(int), *(int *)(state.ip - sizeof(int)))
#define BYTE *(state.ip)++
#define STRING get_string(state.bf, INT)
#define FAIL failure("ERROR: invalid opcode %d-%d\n", h, l)

  state.ip = bf->code_ptr;
  state.closure = NULL;
  state.esp = bf->stack_ptr;
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
            push(value);
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

            aint args[n + 1]; // I could not use args if the stack grows upwards
            for (int i = 0; i < n; i++) {
              args[i] = pop();
            }
            args[n] = LtagHash(tag);
            Bsexp(args, BOX(n + 1));
            fprintf(f, "%d", n);
            break;
          }

          case 3: {
            fprintf(f, "STI");
            failure("Should not happen. Indirect assignemts are temporarily prohibited.");
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

          case 6: {
            fprintf(f, "END");
            break;
          }

          case 7:
            fprintf(f, "RET");
            break;

          case 8:
            fprintf(f, "DROP");
            break;

          case 9:
            fprintf(f, "DUP");
            break;

          case 10:
            fprintf(f, "SWAP");
            break;

          case 11:
            fprintf(f, "ELEM");
            break;

          default:
            FAIL;
        }
        break;

      case 2:
      case 3:
      case 4:
        fprintf(f, "%s\t", lds[h - 2]);
        switch (l) {
          case 0:
            fprintf(f, "G(%d)", INT);
            break;
          case 1:
            fprintf(f, "L(%d)", INT);
            break;
          case 2:
            fprintf(f, "A(%d)", INT);
            break;
          case 3:
            fprintf(f, "C(%d)", INT);
            break;
          default:
            FAIL;
        }
        break;

      case 5:
        switch (l) {
          case 0:
            fprintf(f, "CJMPz\t0x%.8x", INT);
            break;

          case 1:
            fprintf(f, "CJMPnz\t0x%.8x", INT);
            break;

          case 2:
            fprintf(f, "BEGIN\t%d ", INT);
            fprintf(f, "%d", INT);
            break;

          case 3:
            fprintf(f, "CBEGIN\t%d ", INT);
            fprintf(f, "%d", INT);
            break;

          case 4:
            fprintf(f, "CLOSURE\t0x%.8x", INT);
            {
              int n = INT;
              for (int i = 0; i < n; i++) {
                switch (BYTE) {
                  case 0:
                    fprintf(f, "G(%d)", INT);
                    break;
                  case 1:
                    fprintf(f, "L(%d)", INT);
                    break;
                  case 2:
                    fprintf(f, "A(%d)", INT);
                    break;
                  case 3:
                    fprintf(f, "C(%d)", INT);
                    break;
                  default:
                    FAIL;
                }
              }
            };
            break;

          case 5:
            fprintf(f, "CALLC\t%d", INT);
            break;

          case 6: {
            const int offset = INT;
            const int locals_num = INT;
            fprintf(f, "CALL\t0x%.8x ", offset);
            fprintf(f, "%d", locals_num);
            push((aint) state.esp);
            push((aint) state.ebp);
            state.ebp = state.esp;
            jump(offset);
            break;
          }

          case 7:
            fprintf(f, "TAG\t%s ", STRING);
            fprintf(f, "%d", INT);
            break;

          case 8:
            fprintf(f, "ARRAY\t%d", INT);
            break;

          case 9:
            fprintf(f, "FAIL\t%d", INT);
            fprintf(f, "%d", INT);
            break;

          case 10:
            fprintf(f, "LINE\t%d", INT);
            break;

          default:
            FAIL;
        }
        break;

      case 6:
        fprintf(f, "PATT\t%s", pats[l]);
        break;

      case 7: {
        switch (l) {
          case 0:
            fprintf(f, "CALL\tLread");
            break;

          case 1:
            fprintf(f, "CALL\tLwrite");
            break;

          case 2:
            fprintf(f, "CALL\tLlength");
            break;

          case 3:
            fprintf(f, "CALL\tLstring");
            break;

          case 4:
            fprintf(f, "CALL\tBarray\t%d", INT);
            break;

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