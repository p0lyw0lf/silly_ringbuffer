#ifndef SILLY_RINGBUFFER_H
#define SILLY_RINGBUFFER_H

#include <assert.h>
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
 * @param TYPE the type name you wish the newly-generated structure to have.
 * @param ELEM_TYPE the type of elements to be stored in the ringbuffer
 **/
#define SRB_DECL(TYPE, ELEM_TYPE)                                              \
  SRB_SLICE(ELEM_TYPE)                                                         \
  SRB_VIEW(ELEM_TYPE)                                                          \
  typedef struct {                                                             \
    /** @brief Stores all the elements */                                      \
    ELEM_TYPE##_slice buffer;                                                  \
    /** @brief Index of the next element that can be popped */                 \
    size_t head;                                                               \
    /** @brief Index of the next available slot to place an element */         \
    size_t tail;                                                               \
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
  VAR.head = 0;                                                                \
  VAR.tail = 0;

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

int srb_empty(srb *s) { return s->head == s->tail; }

/**
 * @returns the number of available slots left to place elements in.
 */
size_t srb_remaining(srb *s) {
  if (s->head > s->tail) {
    // Gap is in the middle of the array.
    // 11111100011
    //       t  h
    return s->head - s->tail;
  } else {
    // Gaps are at either side of the array.
    // 00111100000
    //   h   t
    return s->head + (s->buffer.size - s->tail);
  }
}

int srb_try_push(srb *s, int_slice v) {
  if (srb_remaining(s) < v.size) {
    return 1;
  }

  // TODO: wrapping_add
  size_t next_tail = s->tail + v.size;
  if (next_tail >= s->buffer.size) {
    // TODO: wrapping_sub
    next_tail -= s->buffer.size;
  }

  // TODO: checked mul
  memcpy(&s->buffer.data[s->tail], v.data, v.size * sizeof(int));
  s->tail = next_tail;
  return 0;
}

void srb_push(srb *s, int_slice v) { UNWRAP(srb_try_push(s, v)); }

/**
 * @returns the number of elements present in `s`
 */
size_t srb_len(srb *s) {
  if (s->head <= s->tail) {
    // Gaps are at either side of the array.
    // 00111100000
    //   h   t
    return s->tail - s->head;
  } else {
    // Gap is in the middle of the array.
    // 11111100011
    //       t  h
    return s->tail + (s->buffer.size - s->head);
  }
}

int srb_try_pop(srb *s, int_slice v) {
  if (srb_len(s) < v.size) {
    return 1;
  }
  // TODO: wrapping_add
  size_t next_head = s->head + v.size;
  if (next_head >= s->buffer.size) {
    next_head -= v.size;
  }

  // TODO: checked mul
  memcpy(v.data, &s->buffer.data[s->head], v.size * sizeof(int));
  s->head = next_head;
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
