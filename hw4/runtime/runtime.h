#ifndef __LAMA_RUNTIME__
#define __LAMA_RUNTIME__

#include "runtime_common.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WORD_SIZE (CHAR_BIT * sizeof(ptrt))

// Failure reporting
void failure (char *s, ...);

// Assertions used across interpreter and runtime (copied from runtime.c)
#define ASSERT_BOXED(memo, x) do { if (UNBOXED(x)) failure("boxed value expected in %s\n", memo); } while (0)
#define ASSERT_UNBOXED(memo, x) do { if (!UNBOXED(x)) failure("unboxed value expected in %s\n", memo); } while (0)
#define ASSERT_STRING(memo, x) do { if (!UNBOXED(x) && TAG(TO_DATA(x)->data_header) != STRING_TAG) failure("string value expected in %s\n", memo); } while (0)

// Prototypes for runtime functions referenced by interpreter.cpp
// S-expression utilities
void *Bsexp(aint* args, aint bn);
void *Bsexp_reversed(aint* args, aint bn);
aint   Btag(void *d, aint t, aint n);
aint   LtagHash(char *s);
aint   LkindOf(void *p);
aint   LcompareTags(void *p, void *q);

// Arrays/strings/closures builders and accessors
void *Barray(aint* args, aint bn);
void *Barray_reversed(aint* args, aint bn);
void *Bstring(aint* args);
void *Bclosure(aint* args, aint bn);
void *Belem(void *p, aint i);
void *Bsta(void *x, aint i, void *v);

// Pattern helpers
aint Barray_patt(void *d, aint n);
aint Bstring_patt(void *x, void *y);
aint Bstring_tag_patt(void *x);
aint Barray_tag_patt(void *x);
aint Bsexp_tag_patt(void *x);
aint Bboxed_patt(void *x);
aint Bunboxed_patt(void *x);
aint Bclosure_tag_patt(void *x);

// IO and small helpers
aint Lread();
aint Lwrite(aint n);
aint Llength(void *p);
void *Lstring(aint* args);

#ifdef __cplusplus
}
#endif

#endif
