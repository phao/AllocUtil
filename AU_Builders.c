#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "AU.h"
#include "XMalloc.h"

#define ISSUE_ERROR(err) xerror(err, # err)

//////////////////////
//// BYTE Builder ////
//////////////////////

#ifndef NDEBUG

#define ASSERT_VALID_B1(b1) \
  do { \
    assert(b1); \
    assert((b1)->mem); \
    assert((b1)->used <= (b1)->cap); \
    assert((b1)->cap > 0); \
  } while (0)

#else

#define ASSERT_VALID_B1(b1)

#endif

int
AU_B1_Setup(AU_ByteBuilder *b1, long cap) {
  assert(cap > 0);
  b1->cap = cap;
  b1->used = 0;
  b1->mem = xmalloc((size_t) cap);
  if (!b1->mem) {
    ISSUE_ERROR(AU_ERR_XMALLOC);
    return AU_ERR_XMALLOC;
  }
  ASSERT_VALID_B1(b1);
  return 0;
}

long
AU_B1_Append(AU_ByteBuilder *b1, const void *mem, long size) {
  ASSERT_VALID_B1(b1);
  assert(mem);

  long offset = b1->used;
  void *out_addr = AU_B1_AppendForSetup(b1, size);
  if (!out_addr) {
    return -1;
  }
  memcpy(out_addr, mem, size);
  return offset;
}

void *
AU_B1_AppendForSetup(AU_ByteBuilder *b1, long size) {
  ASSERT_VALID_B1(b1);
  assert(size >= 0);

  if (b1->used > LONG_MAX - size) {
    ISSUE_ERROR(AU_ERR_OVERFLOW);
    return 0;
  }
  if (b1->used + size > b1->cap) {
    if (b1->cap == LONG_MAX) {
      // This seems so absurd...
      ISSUE_ERROR(AU_ERR_OVERFLOW);
      return 0;
    }
    long new_cap = b1->cap > LONG_MAX/2 ? LONG_MAX : b1->cap*2;
    void *p = xrealloc(b1->mem, new_cap);
    if (!p) {
      ISSUE_ERROR(AU_ERR_XREALLOC);
      return 0;
    }
    b1->mem = p;
    b1->cap = new_cap;
  }
  void *out_addr = (char*)b1->mem + b1->used;
  b1->used += size;
  return out_addr;
}

void *
AU_B1_GetMemory(const AU_ByteBuilder *b1) {
  ASSERT_VALID_B1(b1);

  return b1->mem;
}


void
AU_B1_DiscardAppends(AU_ByteBuilder *b1) {
  ASSERT_VALID_B1(b1);

  b1->used = 0;
}

void
AU_B1_DiscardLastBytes(AU_ByteBuilder *b1, long n) {
  ASSERT_VALID_B1(b1);
  assert(n >= 0);
  assert(b1->used >= n);

  b1->used -= n;
}

////////////////////////////
//// Fixed Size Builder ////
////////////////////////////

#ifndef NDEBUG

#define ASSERT_VALID_FSB(fsb) \
  do { \
    assert(fsb); \
    assert((fsb)->elt_size > 0); \
    assert((fsb)->elt_size <= LONG_MAX / (fsb)->b1.cap); \
    ASSERT_VALID_B1(&(fsb)->b1); \
  } while (0)

#else

#define ASSERT_VALID_FSB(fsb)

#endif

int
AU_FSB_Setup(AU_FixedSizeBuilder *fsb, long elt_size, long cap) {
  assert(cap > 0);
  assert(elt_size > 0);
  assert(cap <= LONG_MAX/elt_size);

  int b1_res = AU_B1_Setup(&fsb->b1, elt_size*cap);
  if (b1_res < 0) {
    return b1_res;
  }
  fsb->elt_size = elt_size;

  ASSERT_VALID_FSB(fsb);

  return 0;
}

long
AU_FSB_Append(AU_FixedSizeBuilder *fsb, const void *mem, long n) {
  ASSERT_VALID_FSB(fsb);
  assert(mem);
  assert(n >= 0);

  if (n != 0 && fsb->elt_size > LONG_MAX/n) {
    ISSUE_ERROR(AU_ERR_OVERFLOW);
    return AU_ERR_OVERFLOW;
  }

  long offset = AU_B1_Append(&fsb->b1, mem, n*fsb->elt_size);
  if (offset < 0) {
    return offset;
  }
  assert(offset % fsb->elt_size == 0);
  return offset/fsb->elt_size;
}

void *
AU_FSB_AppendForSetup(AU_FixedSizeBuilder *fsb, long n) {
  ASSERT_VALID_FSB(fsb);
  assert(n >= 0);

  if (n != 0 && fsb->elt_size > LONG_MAX/n) {
    ISSUE_ERROR(AU_ERR_OVERFLOW);
    return AU_ERR_OVERFLOW;
  }
  void *mem = AU_B1_AppendForSetup(&fsb->b1, n*fsb->elt_size);
  if (!mem) {
    return 0;
  }
  return mem;
}

void *
AU_FSB_GetMemory(AU_FixedSizeBuilder *fsb) {
  ASSERT_VALID_FSB(fsb);

  return AU_B1_GetMemory(&fsb->b1);
}

void
AU_FSB_DiscardAppends(AU_FixedSizeBuilder *fsb) {
  ASSERT_VALID_FSB(fsb);

  AU_B1_DiscardAppends(&fsb->b1);
}

void
AU_FSB_DiscardLastAppends(AU_FixedSizeBuilder *fsb, long n) {
  ASSERT_VALID_FSB(fsb);
  assert(n >= 0);
  assert(n == 0 || fsb->elt_size <= LONG_MAX/n);
  assert(fsb->b1.used/fsb->elt_size >= n);

  AU_B1_DiscardLastBytes(&fsb->b1, n*fsb->elt_size);
}

///////////////////////////////
//// Variable Size Builder ////
///////////////////////////////

union AlignmentType {
  int i;
  long l;
  long long ll;
  void *vp;
  void (*funp)(void);
  float f;
  double d;
  long double ld;
  float *fp;
  double *dp;
  long double *ldp;
};

enum {
  ALIGNMENT_BOUNDARY = (long)sizeof (union AlignmentType)
};

static inline long
Align(long n) {
  assert(n <= LONG_MAX - ALIGNMENT_BOUNDARY + 1);
  return (n + ALIGNMENT_BOUNDARY - 1)/ALIGNMENT_BOUNDARY * ALIGNMENT_BOUNDARY;
}

int
AU_VSB_Setup(AU_VarSizeBuilder *vsb, long cap) {
  return AU_B1_Setup(vsb, Align(cap));
}

long
AU_VSB_Append(AU_VarSizeBuilder *vsb, const void *mem, long n) {
  return AU_B1_Append(vsb, mem, Align(n));
}

void *
AU_VSB_AppendForSetup(AU_VarSizeBuilder *vsb, long n) {
  return AU_B1_AppendForSetup(vsb, Align(n));
}

void *
AU_VSB_GetMemory(AU_VarSizeBuilder *vsb) {
  return AU_B1_GetMemory(vsb);
}

void
AU_VSB_DiscardAppends(AU_VarSizeBuilder *vsb) {
  AU_B1_DiscardAppends(vsb);
}

/////////////////////////
//// Stack Allocator ////
/////////////////////////

int
AU_SA_Setup(AU_StackAllocator *sa, long cap) {
  assert(cap > 0);

  return AU_VSB_Setup(sa, cap);
}

void *
AU_SA_Alloc(AU_StackAllocator *sa, long n) {
  assert(n >= 0);

  const long where_now = sa->used;
  const long n_align = Align(n);
  const long where_now_align = Align(sizeof where_now);

  assert(sizeof where_now <= Align(sizeof where_now));

  char *mem = AU_VSB_AppendForSetup(sa, n_align + where_now_align);
  if (!mem) {
    return 0;
  }
  *(long*)(mem+n_align) = where_now;
  return mem;
}

void
AU_SA_Free(AU_StackAllocator *sa, long n) {
  assert(n >= 0);

  const where_now_align = Align(sizeof (long));
  assert(sizeof (long) <= Align(sizeof where_now));

  char *mem = (char*)sa->mem + sa->used;
  while (n > 0) {
    mem -= where_now_align;
    mem = (char*)sa->mem + *(long*)mem;
    n--;
  }
  sa->used = mem - (char*)sa->mem;
}

void
AU_SA_Destroy(AU_StackAllocator *sa) {
  xfree(sa->mem);
}
