//
// Created by enotvtapke on 10/25/25.
//

#ifndef HW2_INTERPRETER_H
#define HW2_INTERPRETER_H
#include "shared.h"

extern "C" {
  #include "runtime_common.h"
}
#include <stdio.h>

#define STACK_SIZE 1048576

void interpret(const bytefile *bf);

#endif //HW2_INTERPRETER_H