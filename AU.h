#ifndef ALLOC_UTILS_H
#define ALLOC_UTILS_H

/**
 * With the allocator and builder types, you're supposed to use them through
 * the provided typedefs. The underlying representation of those types are
 * reserved for the library authors to decide at their own will. It can change
 * from non-debug to debug verion, from this version to the next, and so forth.
 *
 * They are only defined here inside the header file so you can have them on
 * automatic storage or static storage. You can still put them on allocated
 * storage, but you're not required. That is, the types are only defined here
 * so you can better choose the storage of your variables.
 */

#include <stdlib.h>
#include <limits.h>

enum {
  AU_ERR_XMALLOC = INT_MIN,
  AU_ERR_XREALLOC,
  AU_ERR_XCALLOC,
  AU_ERR_OVERFLOW
};

//////////////////////
//// BYTE Builder ////
//////////////////////

struct AU_ByteBuilder {
  void *mem;
  size_t used, cap;
};

typedef struct AU_ByteBuilder AU_ByteBuilder;

/**
 * @param cap Must be a positive number.
 *
 * @return 0 on succes, a negative value on failure.
 */
int
AU_B1_Setup(AU_ByteBuilder *b1, size_t cap);

/**
 * @param b1 Must be non null properly set up byte builder.
 * @param mem Must be non null address.
 * @param size Positive integer telling how many bytes are in mem. If 0,
 * this operation is basically a no-op.
 * @param out_offset If not null, it'll be used to store the resulting offset
 * from the base address where those bytes went.
 *
 * @return 0 on succes, a negative value on failure.
 *
 * @note The bytes of an append will start right after the last byte of a
 * previous attempt. There won't be gaps in between appends.
 */
int
AU_B1_Append(AU_ByteBuilder *b1,
             const void *mem,
             size_t size,
             size_t *out_offset);

/**
 * @param b1 Must be non null properly set up byte builder.
 * @param size Positive integer telling how many bytes are in mem. If 0,
 * this operation is basically a no-op.
 * @param out_addr Must be a non null void** telling where to put the address
 * which you can then use to set up the bytes. If n is 0, *out_addr is left
 * as it was.
 * @param out_offset If not null, it'll be used to store the resulting offset
 * from the base address where those bytes went.
 *
 * @return 0 on succes, a negative value on failure.
 *
 * @note The bytes of an append will start right after the last byte of a
 * previous attempt. There won't be gaps in between appends.
 */
int
AU_B1_AppendForSetup(AU_ByteBuilder *b1,
                     size_t size,
                     void **out_addr,
                     size_t *out_offset);

/**
 * @param b1 Must be a non null properly set up byte builder.
 *
 * @return Never should be null. If all is right, this retuns a pointer to the
 * underlying memory.
 */
void*
AU_B1_GetMemory(const AU_ByteBuilder *b1);

/**
 * @param b1 Must be a non null properly set up byte builder.
 */
void
AU_B1_DiscardAppends(AU_ByteBuilder *b1);

/**
 * @param b1 Must be a non null properly set up byte builder.
 * @param n The number of bytes at the end to discard. This number can be at
 * most the total sum of the amount of bytes you've allocated so far. It's an
 * error to pass a larger number than that.
 */
void
AU_B1_DiscardLastBytes(AU_ByteBuilder *b1, size_t n);

////////////////////////////
//// Fixed Size Builder ////
////////////////////////////

struct AU_FixedSizeBuilder {
  AU_ByteBuilder b1;
  size_t elt_size;
};

/**
 * As with all other other builder/allocator types, the representation of an
 * AU_FixedSizeBuilder shouldn't be relied upon (check the other comment in
 * the beginning of this file).
 *
 * Functions on fixed size buffers have sizes in temrs of the element size,
 * whose case is the only exception to this rule. The element size you specify
 * in bytes during the setup function (AU_FSB_Setup). All the other sizes and
 * offsets are specified in terms of element sizes.
 */
typedef struct AU_FixedSizeBuilder AU_FixedSizeBuilder;

/**
 * Sets up the given fixed size builder.
 *
 * @param fsb Must be non null.
 * @param elt_size A positive number containing the size in bytes of the
 * elements to be added.
 * @param cap The initial capacity of elements. Must be positive.
 *
 * @note cap*elt_size are assumed to not overflow. There is no check on
 * whether it does or not. You must not call this function if you want to
 * avoid overflow.
 *
 * @return 0 on succes, a negative value on failure.
 */
int
AU_FSB_Setup(AU_FixedSizeBuilder *fsb, size_t elt_size, size_t cap);

/**
 * Append n elements (of size specified in the setup of the given fsb) into
 * the underlying memory of the given fsb.
 *
 * @param fsb Pointer to a properly setup fixed size builder.
 * @param mem A non-null pointer to the first byte in the byte sequence for
 * all the n elements.
 * @param n How many elements to append. This value can be 0, in which case
 * a call to this function would be just "an expensive no-op".
 * @param out_offset If not null, this will be put the offset (in terms of
 * the element size for this FSB) where the first byte mem points to was
 * added (within the FSB's underlying memory block).
 *
 * @return 0 on succes, a negative value on failure.
 */
int
AU_FSB_Append(AU_FixedSizeBuilder *fsb,
              void *mem,
              size_t n,
              size_t *out_offset);

/**
 * Set aside memory for n elements with the given builder. Put the result
 * in the third parameter.
 *
 * @param fsb A pointer to a properly initialized fixed size buffer.
 * @param n How many elements to set aside. It can be 0, and if so this
 * operation will be basically a no-op.
 * @param out_addr Must be a non null void** telling where to put the address
 * which you can then use to set up the bytes. If n is 0, *out_addr is left
 * as it was.
 * @param out_offset If not null, this will be put the offset (in terms of
 * the element size for this FSB) of the first byte reserved for setup.
 *
 * @return 0 on succes, a negative value on failure.
 */
int
AU_FSB_AppendForSetup(AU_FixedSizeBuilder *fsb,
                      size_t n,
                      void **out_addr,
                      size_t *out_offset);

/**
 * @param fsb A pointer to a properly initialized fixed size buffer.
 *
 * @return The underlying memory the builder kept appending to for you to
 * use. Remember that each append call can invalidade this pointer.
 */
void*
AU_FSB_GetMemory(AU_FixedSizeBuilder *fsb);

/**
 * Discards all the appends done to this builder. The set aside memory will
 * be recycled.
 *
 * @param fsb A pointer to a properly initialized fixed size buffer.
 */
void
AU_FSB_DiscardAppends(AU_FixedSizeBuilder *fsb);

/**
 * Discards all the last n appends done to this builder. The set aside memory
 * will be recycled.
 *
 * @param fsb A pointer to a properly initialized fixed size buffer.
 *
 * @note It's an error to pass in a value for n larger than how many elements
 * were already added to the buffer. As an example, you cannot do
 * AU_FSB_DiscardLastAppends(my_fsb, SIZE_MAX); as a way to discar all the
 * elements.
 */
void
AU_FSB_DiscardLastAppends(AU_FixedSizeBuilder *fsb, size_t n);

///////////////////////////////
//// Variable Size Builder ////
///////////////////////////////

/**
 * As with all other other builder/allocator types, the representation of an
 * AU_VarSizeBuilder shouldn't be relied upon (check the other comment in
 * the beginning of this file).
 *
 * Functions on variable size buffers have sizes and offsets specified in terms
 * of bytes, just like a byte builder.
 */
typedef AU_ByteBuilder AU_VarSizeBuilder;

/**
 * Sets up the variable size builder given through the pointer. Initial byte
 * capacity is the second parameter.
 *
 * @param vsb Not null pointer pointing to the VSB to be set up.
 * @param cap Initial byte capacity for the builder. Must be positive.
 *
 * @return 0 on success, negative on error.
 */
int
AU_VSB_Setup(AU_VarSizeBuilder *vsb, size_t cap);

/**
 * @param vsb A pointer to a properly initialized VSB.
 * @param mem A pointer to the first byte of the memory region from which to
 * get bytes.
 * @param n How many bytes to take from the region. It can be 0.
 * @param out_offset If not null, it'll be used to output the offset of this
 * append (offset within this VSB's underlying memory in which this append is
 * taking place.
 *
 * @return 0 on success or negative on error.
 */
int
AU_VSB_Append(AU_VarSizeBuilder *vsb,
              void *mem,
              size_t n,
              size_t *out_offset);


/**
 * @param vsb A pointer to a properly initialized VSB.
 * @param n How many bytes to set aside for setup.
 * @param out_addr Non null pointer to put the address of the first byte in
 * the memory you have for your own setup.
 * @param out_offset If not null, it'll be used to output the offset of this
 * append (offset within this VSB's underlying memory in which this append is
 * taking place.
 *
 * @return 0 on success or negative on error.
 */
int
AU_VSB_AppendForSetup(AU_VarSizeBuilder *vsb,
                      size_t n,
                      void **out_addr,
                      size_t *out_offset);

/**
 * Get the underlying memory for this builder.
 */
void *
AU_VSB_GetMemory(AU_VarSizeBuilder *vsb);

/**
 * Discard all appends done in this builder. The set aside memory isn't freed,
 * it's only recycled.
 */
void
AU_VSB_DiscardAppends(AU_VarSizeBuilder *vsb);

#endif

