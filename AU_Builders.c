#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "AU.h"
#include "XMalloc.h"

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
AU_B1_Setup(AU_ByteBuilder *b1, size_t cap) {
  assert(cap > 0);
  b1->cap = cap;
  b1->used = 0;
  b1->mem = xmalloc(cap);
  if (!b1->mem) {
    return AU_ERR_XMALLOC;
  }
  ASSERT_VALID_B1(b1);
  return 0;
}

int
AU_B1_Append(AU_ByteBuilder *b1,
             const void *mem,
             size_t size,
             size_t *out_offset)
{
  ASSERT_VALID_B1(b1);
  assert(mem);

  void *out_addr;
  int status = AU_B1_AppendForSetup(b1, size, &out_addr, out_offset);
  if (status < 0) {
    return status;
  }
  memcpy(out_addr, mem, size);
  return 0;
}

int
AU_B1_AppendForSetup(AU_ByteBuilder *b1,
                     size_t size,
                     void **out_addr,
                     size_t *out_offset)
{
  ASSERT_VALID_B1(b1);
  assert(out_addr);

  if (b1->used > SIZE_MAX - size) {
    return AU_ERR_OVERFLOW;
  }
  if (b1->used+size > b1->cap) {
    void *p = xrealloc(b1->mem, b1->cap*2);
    if (!p) {
      return AU_ERR_XREALLOC;
    }
    b1->mem = p;
    b1->cap *= 2;
  }
  if (out_offset) {
    *out_offset = b1->used;
  }
  *out_addr = (unsigned char *) b1->mem + b1->used;
  b1->used += size;

#ifndef NDEBUG
  if (size == 0) {
    *out_addr = 0;
  }
#endif

  return 0;
}

void*
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
AU_B1_DiscardLastBytes(AU_ByteBuilder *b1, size_t n) {
  ASSERT_VALID_B1(b1);
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
    assert((fsb)->elt_size <= SIZE_MAX / (fsb)->b1.cap); \
    ASSERT_VALID_B1(&(fsb)->b1); \
  } while (0)

#else

#define ASSERT_VALID_FSB(fsb)

#endif

int
AU_FSB_Setup(AU_FixedSizeBuilder *fsb, size_t elt_size, size_t cap) {
  assert(cap > 0);
  assert(elt_size > 0);
  assert(cap <= SIZE_MAX/elt_size);

  int b1_res = AU_B1_Setup(&fsb->b1, elt_size*cap);
  if (b1_res < 0) {
    return b1_res;
  }
  fsb->elt_size = elt_size;

  ASSERT_VALID_FSB(fsb);

  return 0;
}

int
AU_FSB_Append(AU_FixedSizeBuilder *fsb,
              void *mem,
              size_t n,
              size_t *out_offset)
{
  ASSERT_VALID_FSB(fsb);

  if (fsb->elt_size > SIZE_MAX/n) {
    return AU_ERR_OVERFLOW;
  }
  int res = AU_B1_Append(&fsb->b1, mem, n*fsb->elt_size, out_offset);
  if (res < 0) {
    return res;
  }
  if (out_offset) {
    assert(*out_offset % fsb->elt_size == 0);
    *out_offset /= fsb->elt_size;
  }
  return res;
}

int
AU_FSB_AppendForSetup(AU_FixedSizeBuilder *fsb,
                      size_t n,
                      void **out_addr,
                      size_t *out_offset)
{
  ASSERT_VALID_FSB(fsb);

  if (fsb->elt_size > SIZE_MAX/n) {
    return AU_ERR_OVERFLOW;
  }
  int res = AU_B1_AppendForSetup(&fsb->b1, n*fsb->elt_size, out_addr,
    out_offset);
  if (res < 0) {
    return res;
  }
  if (out_offset) {
    assert(*out_offset % fsb->elt_size == 0);
    *out_offset /= fsb->elt_size;
  }
  return res;
}

void*
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
AU_FSB_DiscardLastAppends(AU_FixedSizeBuilder *fsb, size_t n) {
  ASSERT_VALID_FSB(fsb);
  assert(fsb->elt_size <= SIZE_MAX/n);
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
  ALIGNMENT_BOUNDARY = sizeof (union AlignmentType)
};

static inline size_t
Align(size_t n) {
  assert(n <= SIZE_MAX - ALIGNMENT_BOUNDARY + 1);
  return (n + ALIGNMENT_BOUNDARY - 1)/ALIGNMENT_BOUNDARY * ALIGNMENT_BOUNDARY;
}

int
AU_VSB_Setup(AU_VarSizeBuilder *vsb, size_t cap) {
  return AU_B1_Setup(vsb, Align(cap));
}

int
AU_VSB_Append(AU_VarSizeBuilder *vsb,
              void *mem,
              size_t n,
              size_t *out_offset)
{
  return AU_B1_Append(vsb, mem, Align(n), out_offset);
}

int
AU_VSB_AppendForSetup(AU_VarSizeBuilder *vsb,
                      size_t n,
                      void **out_addr,
                      size_t *out_offset)
{
  return AU_B1_AppendForSetup(vsb, Align(n), out_addr, out_offset);
}

void *
AU_VSB_GetMemory(AU_VarSizeBuilder *vsb) {
  return AU_B1_GetMemory(vsb);
}

void
AU_VSB_DiscardAppends(AU_VarSizeBuilder *vsb) {
  AU_B1_DiscardAppends(vsb);
}
