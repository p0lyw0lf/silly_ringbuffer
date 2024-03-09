#ifndef SILLY_RINGBUFFER_H
#define SILLY_RINGBUFFER_H

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#define SRB_SLICE(TYPE)                                                        \
  /**                                                                          \
   * @brief A pointer and length. For convenience.                             \
   **/                                                                         \
  typedef struct {                                                             \
    TYPE *data;                                                                \
    size_t size;                                                               \
  } TYPE##_slice;

#define SRB_VIEW(TYPE)                                                         \
  /**                                                                          \
   * @brief Creates a `TYPE##_slice` from a pointer and length.                \
   *                                                                           \
   * @param p Pointer to data at valid for at least `n` units, in its original \
   *          interpretation.                                                  \
   * @param n The length of the data.                                          \
   */                                                                          \
  TYPE##_slice TYPE##_view(TYPE *p, size_t n) { return (TYPE##_slice){p, n}; }

/**
 * @brief Declares a new ringbuffer called `TYPE`, whose elements are of type
 * `ELEM_TYPE`.
 *
 * Also declares all the functions, prefixed with `TYPE`, to go along with it.
 *
 * Structure:
 *
 * head_valid --(A)-- head_commit --(B)-- tail_valid --(C)-- tail_commit --(D)--
 *
 * Pushes and pops are done in two stages: first, the "committed" region is
 * grown, representing a claim on a region of memory, and then the "valid"
 * region is grown, representing a finished read or write. Both steps can be
 * done with a simple atomic addition.
 *
 * Region descriptions:
 * (A): in the process of being read
 * (B): stable stored elements
 * (C): in the process of being written
 * (D): stable empty elements
 *
 * @param TYPE the type name you wish the newly-generated structure to have.
 * @param ELEM_TYPE the type of elements to be stored in the ringbuffer
 **/
#define SRB_DECL(TYPE, ELEM_TYPE)                                              \
  SRB_SLICE(ELEM_TYPE)                                                         \
  SRB_VIEW(ELEM_TYPE)                                                          \
  typedef struct {                                                             \
    /** @brief Stores all the elements */                                      \
    ELEM_TYPE##_slice buffer;                                                  \
    /** @brief Index of the next element that has yet to be popped */          \
    atomic_size_t head_valid;                                                  \
    /** @brief Index of the next element that can be popped */                 \
    atomic_size_t head_commit;                                                 \
    /** @brief Index of the next available slot to place an element */         \
    atomic_size_t tail_valid;                                                  \
    /** @brief Index of the next available slot to place an element */         \
    atomic_size_t tail_commit;                                                 \
  } TYPE;

SRB_DECL(srb, int);

#define SRB_DEFAULT_SIZE 8
#define TRY(statement)                                                         \
  do {                                                                         \
    int err = statement;                                                       \
    if (err != 0)                                                              \
      return err;                                                              \
  } while (1 == 0)

#define UNWRAP(statement)                                                      \
  do {                                                                         \
    int err = statement;                                                       \
    assert(err == 0);                                                          \
  } while (1 == 0)

/**
 * @brief Initializes a `VAR` to be a ringbuffer of `TYPE`, with `N` spaces
 *
 * @param TYPE the type of the ringbuffer
 * @param VAR the variable to initialize
 * @param N the amount of spaces to reserve
 */
#define SRB_INIT(TYPE, VAR, N)                                                 \
  TYPE VAR;                                                                    \
  VAR.buffer.data = malloc(sizeof(*(VAR.buffer.data)) * N);                    \
  assert(VAR.buffer.data != NULL);                                             \
  VAR.buffer.size = N;                                                         \
  atomic_init(&VAR.head_valid, 0);                                             \
  atomic_init(&VAR.head_commit, 0);                                            \
  atomic_init(&VAR.tail_valid, 0);                                             \
  atomic_init(&VAR.tail_commit, 0);

/**
 * @brief Frees the internal data for the ringbuffer `VAR`
 *
 * @param VAR the ringbuffer to free
 *
 * TODO: put in more sentinel values to mark it as freed
 */
#define SRB_FREE(VAR)                                                          \
  do {                                                                         \
    free((VAR).buffer.data);                                                   \
    (VAR).buffer.data = NULL;                                                  \
  } while (1 == 0)

/**
 * @returns the number of available slots left to place elements in.
 */
size_t srb_remaining(size_t head, size_t tail, size_t size) {
  if (head > tail) {
    // Gap is in the middle of the array.
    // 11111100011
    //       t  h
    return head - tail;
  } else {
    // Gaps are at either side of the array.
    // 00111100000
    //   h   t
    return head + (size - tail);
  }
}

/**
 * @brief Effectively does `*out = tail + n`, wrapping around on size, and
 * returning an error if it cannot
 */
int srb_wrapping_push(size_t *out, size_t head, size_t tail, size_t size,
                      size_t n) {
  if (srb_remaining(head, tail, size) < n) {
    return 1;
  }

  // TODO: wrapping_add
  size_t next_tail = tail + n;
  if (next_tail >= size) {
    // TODO: wrapping_sub
    next_tail -= size;
  }

  *out = next_tail;
  return 0;
}

int srb_try_push(srb *s, int_slice v) {
  size_t next_tail;
  size_t tail;
  do {
    tail = atomic_load(&s->tail_commit);
    size_t head = atomic_load(&s->head_valid);
    TRY(srb_wrapping_push(&next_tail, head, tail, s->buffer.size, v.size));
  } while (!atomic_compare_exchange_weak(&s->tail_commit, &tail, next_tail));

  // TODO: checked mul
  memcpy(&s->buffer.data[tail], v.data, v.size * sizeof(int));
  // NOTE: This isn't an atomic_add, because of the following scenario:
  // 1. push size 1000 queued (tail_commit = 1000)
  // 2. push size 1 queued (tail_commit = 1001)
  // 3. push size 1 completes
  // We cannot update tail_valid until the first push also completes, hence why
  // this is a compare_exchange
  while (!atomic_compare_exchange_weak(&s->tail_valid, &tail, next_tail))
    ;
  return 0;
}

void srb_push(srb *s, int_slice v) { UNWRAP(srb_try_push(s, v)); }

/**
 * @returns the number of elements present in `s`
 */
size_t srb_len(size_t head, size_t tail, size_t size) {
  if (head <= tail) {
    // Gaps are at either side of the array.
    // 00111100000
    //   h   t
    return tail - head;
  } else {
    // Gap is in the middle of the array.
    // 11111100011
    //       t  h
    return tail + (size - head);
  }
}

/**
 * @brief Effectively does `*out = head + n;`, wrapping around on size, and
 * returning an error if it would overflow
 */
int srb_wrapping_pop(size_t *out, size_t head, size_t tail, size_t size,
                     size_t n) {
  if (srb_len(head, tail, size) < n) {
    return 1;
  }

  // TODO: wrapping_add
  size_t next_head = head + n;
  if (next_head >= size) {
    // TODO: wrapping_sub
    next_head -= size;
  }

  *out = next_head;
  return 0;
}

int srb_try_pop(srb *s, int_slice v) {
  size_t next_head;
  size_t head;
  do {
    head = atomic_load(&s->head_commit);
    size_t tail = atomic_load(&s->tail_valid);
    TRY(srb_wrapping_pop(&next_head, head, tail, s->buffer.size, v.size));
  } while (!atomic_compare_exchange_weak(&s->head_commit, &head, next_head));

  // TODO: checked mul
  memcpy(v.data, &s->buffer.data[head], v.size * sizeof(int));
  // NOTE: This isn't an atomic_add, because of the following scenario:
  // 1. pop size 1000 queued (head_commit = 1000)
  // 2. pop size 1 queued (head_commit = 1001)
  // 3. pop size 1 completes
  // We cannot update head_valid until the first pop also completes, hence why
  // this is a compare_exchange
  while (!atomic_compare_exchange_weak(&s->head_valid, &head, next_head))
    ;
  return 0;
}

void srb_pop(srb *s, int_slice v) { UNWRAP(srb_try_pop(s, v)); }

int srb_try_push_one(srb *s, int i) {
  int_slice out = {&i, 1};
  return srb_try_push(s, out);
}

void srb_push_one(srb *s, int i) { UNWRAP(srb_try_push_one(s, i)); }

int srb_try_pop_one(srb *s, int *i) {
  int_slice out = {i, 1};
  return srb_try_pop(s, out);
}

int srb_pop_one(srb *s) {
  int out;
  UNWRAP(srb_try_pop_one(s, &out));
  return out;
}

#endif // SILLY_RINGBUFFER_H
