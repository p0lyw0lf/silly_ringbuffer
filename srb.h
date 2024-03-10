#ifndef SILLY_RINGBUFFER_H
#define SILLY_RINGBUFFER_H

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#ifdef SRB_LOG_TRACE
#include <stdio.h>
#define SRB_PRINTF(...) printf(__VA_ARGS__)
#else // SRB_LOG_TRACE
#define SRB_PRINTF(...)
#endif // SRB_LOG_TRACE

#if _MSC_VER && !__clang__
#define srb_counted_by(n) [size_is(n)]
#else // _MSC_VER && !__clang__
#if __has_attribute(counted_by)
#define srb_counted_by(n) __attribute__((counted_by(n))
#else // __has_attribute(counted_by)
#if __has_attribute(__element_count__)
#define srb_counted_by(n) __attribute__((__element_count__(n)))
#else // __has_attribute(__element_count__)
#define srb_counted_by(n)
#endif // __has_attribute(__element_count__)
#endif // __has_attribute(counted_by)
#endif // _MSC_VER && !__clang__

/**
 * @internal
 * @see SRB_DECL
 */
#define SRB_DECL_slice(LINKAGE, TYPE, ELEM_TYPE)                               \
  /**                                                                          \
   * @brief A pointer and length. For convenience.                             \
   **/                                                                         \
  typedef struct {                                                             \
    srb_counted_by(size) ELEM_TYPE *data;                                      \
    size_t size;                                                               \
  } srb_##ELEM_TYPE##_slice

/**
 * @internal
 * @see SRB_DECL
 */
#define SRB_DECL_type(LINKAGE, TYPE, ELEM_TYPE)                                \
  /**                                                                          \
   * @brief A lock-free ringbuffer                                             \
   */                                                                          \
  typedef struct {                                                             \
    /** @brief Stores all the elements. Also contains the size. */             \
    srb_##ELEM_TYPE##_slice buffer;                                            \
    /** @brief Index of the next element that can be popped. */                \
    atomic_size_t head_valid;                                                  \
    /** @brief Index of the next element that doesn't have any pending pops.   \
     */                                                                        \
    atomic_size_t head_commit;                                                 \
    /** @brief Index of the next available slot to place an element. */        \
    atomic_size_t tail_valid;                                                  \
    /**                                                                        \
     * @brief Index of the next available slot that doesn't have any pending   \
     * pushes.                                                                 \
     */                                                                        \
    atomic_size_t tail_commit;                                                 \
    /**                                                                        \
     * @brief The number of slots that have a valid item or have a             \
     * reservation for a write.                                                \
     *                                                                         \
     * If a push would cause this to be greater than size - 1, the push will   \
     * fail. May be larger than what's indicated by tail_commit and            \
     * head_valid. Needed to prevent ABA.                                      \
     */                                                                        \
    atomic_size_t committed_filled;                                            \
    /**                                                                        \
     * @brief The number of slots without an item or have a reservation for a  \
     * read.                                                                   \
     *                                                                         \
     * If a pop would cause this value to be greater than size, the pop will   \
     * fail. May be larger than what's indicated by head_commit and            \
     * tail_valid. Needed to prevent ABA.                                      \
     */                                                                        \
    atomic_size_t committed_empty;                                             \
  } TYPE

/**
 * @brief Declares a new ringbuffer called `TYPE`, whose elements are of
type
 * `ELEM_TYPE`.
 *
 * Also declares all the functions, prefixed with `TYPE`, to go along
with it.
 *
 * Structure:
 *
 * head_valid --(A)-- head_commit --(B)-- tail_valid --(C)-- tail_commit
--(D)--
 *
 * Pushes and pops are done in two stages: first, the "committed" region
is
 * grown, representing a claim on a region of memory, and then the
"valid"
 * region is grown, representing a finished read or write. Both steps
can be
 * done with a simple atomic addition.
 *
 * Region descriptions:
 * (A): in the process of being read
 * (B): stable stored elements
 * (C): in the process of being written
 * (D): stable empty elements
 *
 * @param TYPE the type name you wish the newly-generated structure to
have.
 * @param ELEM_TYPE the type of elements to be stored in the ringbuffer
 * @param LINKAGE the linkage specifier for the functions to declare
 **/
#define SRB_DECL(LINKAGE, TYPE, ELEM_TYPE)                                     \
  SRB_DECL_slice(LINKAGE, TYPE, ELEM_TYPE);                                    \
  SRB_DECL_type(LINKAGE, TYPE, ELEM_TYPE);                                     \
  SRB_DECL_push(LINKAGE, TYPE, ELEM_TYPE);                                     \
  SRB_DECL_pop(LINKAGE, TYPE, ELEM_TYPE);

/**
 * @brief Defines all the methods needed to operate on the ringbuffer `TYPE`, as
 * generated by `SRB_DECL`
 *
 * @see SRB_DECL
 * @param TYPE the type name you wish the newly-generated structure to have.
 * @param ELEM_TYPE the type of elements to be stored in the ringbuffer
 * @param LINKAGE the linkage specifier for the functions to declare
 */
#define SRB_DEF(LINKAGE, TYPE, ELEM_TYPE)                                      \
  SRB_DEF_push(LINKAGE, TYPE, ELEM_TYPE);                                      \
  SRB_DEF_pop(LINKAGE, TYPE, ELEM_TYPE);

/**
 * @brief Early-returns if a statement is nonzero
 *
 * Must be run inside a function that returns an integer value.
 */
#define SRB_TRY(statement)                                                     \
  do {                                                                         \
    int srb_internal_err = (statement);                                        \
    if (srb_internal_err != 0)                                                 \
      return srb_internal_err;                                                 \
  } while (0)

/**
 * @brief Panics if a statement is nonzero
 *
 * Uses the assert macro internally, so may be disabled in release builds.
 */
#define SRB_UNWRAP(statement)                                                  \
  do {                                                                         \
    int srb_internal_err = (statement);                                        \
    assert(srb_internal_err == 0);                                             \
  } while (0)

/**
 * @brief Initializes a `VAR` to be a ringbuffer of `TYPE`, with `N` spaces
 *
 * @param TYPE the type of the ringbuffer
 * @param VAR the variable to initialize
 * @param N the amount of spaces to reserve
 */
#define SRB_INIT(VAR, N)                                                       \
  (VAR).buffer.data = malloc(sizeof(*((VAR).buffer.data)) * N);                \
  assert((VAR).buffer.data != NULL);                                           \
  (VAR).buffer.size = N;                                                       \
  atomic_init(&(VAR).head_valid, 0);                                           \
  atomic_init(&(VAR).head_commit, 0);                                          \
  atomic_init(&(VAR).tail_valid, 0);                                           \
  atomic_init(&(VAR).tail_commit, 0);                                          \
  atomic_init(&(VAR).committed_filled, 0);                                     \
  atomic_init(&(VAR).committed_empty, N);

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
 * @brief returns the number of available slots left to place elements in.
 *
 * size_t srb_remaining(size_t head, size_t tail, size_t size);
 */
#define srb_remaining(head, tail, size)                                        \
  ((((head) > (tail)) ? /* Gap is in the middle of the array.                  \
                         * 11111100011                                         \
                         *       t  h                                          \
                         */                                                    \
        (head) - (tail)                                                        \
                      : /* Gaps are at either side of the array.               \
                         * 00111100000                                         \
                         *   h   t                                             \
                         */                                                    \
        (head) + ((size) - (tail))) -                                          \
   1)

/**
 * @brief Effectively does `*out = tail + n`, wrapping around on size, and
 * returning an error if it cannot.
 *
 * int srb_wrapping_push(size_t *out, size_t head, size_t tail, size_t size,
 * size_t n);
 */
#define srb_wrapping_push(out, head, tail, size, n)                            \
  (srb_remaining(head, tail, size) < (n)                                       \
       ? 1                                                                     \
       : (/* TODO: wrapping_add */                                             \
          ((tail) + (n) >= (size) ? (*(out) = (tail) + (n) - (size))           \
                                  : (*(out) = (tail) + (n))),                  \
          0))

/**
 * @internal
 * @see SRB_DECL
 */
#define SRB_DECL_push(LINKAGE, TYPE, ELEM_TYPE)                                \
  LINKAGE int TYPE##_try_push(TYPE *s, srb_##ELEM_TYPE##_slice v);             \
  LINKAGE void TYPE##_push(TYPE *s, srb_##ELEM_TYPE##_slice v);                \
  LINKAGE int TYPE##_try_push_one(TYPE *s, ELEM_TYPE i);                       \
  LINKAGE void TYPE##_push_one(TYPE *s, ELEM_TYPE i);

/**
 * @internal
 * @see SRB_DEF
 */
#define SRB_DEF_push(LINKAGE, TYPE, ELEM_TYPE)                                 \
  LINKAGE int TYPE##_try_push(TYPE *s, srb_##ELEM_TYPE##_slice v) {            \
    /* First, reserve space in the single counter. This prevents ABA with the  \
     * two counters below */                                                   \
    size_t next_filled;                                                        \
    size_t filled;                                                             \
    do {                                                                       \
      filled = atomic_load(&s->committed_filled);                              \
      /* TODO: checked add */                                                  \
      next_filled = filled + v.size;                                           \
      if (next_filled > s->buffer.size - 1) {                                  \
        return 1;                                                              \
      }                                                                        \
    } while (!atomic_compare_exchange_strong(&s->committed_filled, &filled,    \
                                             next_filled));                    \
    size_t next_tail;                                                          \
    size_t tail;                                                               \
    do {                                                                       \
      tail = atomic_load(&s->tail_commit);                                     \
      size_t head = atomic_load(&s->head_valid);                               \
      /* This try _should_ be unecessary, since we reserved space above, but   \
       * always better safe than sorry. */                                     \
      SRB_TRY(                                                                 \
          srb_wrapping_push(&next_tail, head, tail, s->buffer.size, v.size));  \
      /* And even though we reserved space for the push previously, we still   \
       * have to do compare exchange again, because we could be running        \
       * concurrently with other pushes */                                     \
    } while (                                                                  \
        !atomic_compare_exchange_strong(&s->tail_commit, &tail, next_tail));   \
                                                                               \
    SRB_PRINTF("push: v.size %zu s->buffer.size %zu tail %zu next_tail %zu\n", \
               v.size, s->buffer.size, tail, next_tail);                       \
                                                                               \
    /* TODO: checked mul */                                                    \
    if (tail + v.size <= s->buffer.size) {                                     \
      memcpy(&s->buffer.data[tail], v.data, v.size * sizeof(ELEM_TYPE));       \
    } else {                                                                   \
      size_t before_end = s->buffer.size - tail;                               \
      memcpy(&s->buffer.data[tail], v.data, before_end * sizeof(ELEM_TYPE));   \
      memcpy(s->buffer.data, &v.data[before_end],                              \
             (v.size - before_end) * sizeof(ELEM_TYPE));                       \
    }                                                                          \
                                                                               \
    /* NOTE: This isn't an atomic_add, because of the following scenario:      \
     * 1. push size 1000 queued (tail_commit = 1000)                           \
     * 2. push size 1 queued (tail_commit = 1001)                              \
     * 3. push size 1 completes                                                \
     * We cannot update tail_valid until the first push also completes, hence  \
     * why this is a compare_exchange. A bit sad it has to be this way,        \
     * spinning is not efficient at all, but this is the only way to do things \
     * without another concurrency control structure.                          \
     */                                                                        \
    while (!atomic_compare_exchange_weak(&s->tail_valid, &tail, next_tail))    \
      ;                                                                        \
                                                                               \
    /* Finally, now that the data is written and we've updated tail_valid, we  \
     * can update the number of empty slots */                                 \
    atomic_fetch_sub(&s->committed_empty, v.size);                             \
    return 0;                                                                  \
  }                                                                            \
                                                                               \
  LINKAGE void TYPE##_push(TYPE *s, srb_##ELEM_TYPE##_slice v) {               \
    SRB_UNWRAP(TYPE##_try_push(s, v));                                         \
  }                                                                            \
                                                                               \
  LINKAGE int TYPE##_try_push_one(TYPE *s, ELEM_TYPE i) {                      \
    srb_##ELEM_TYPE##_slice out = {&i, 1};                                     \
    return TYPE##_try_push(s, out);                                            \
  }                                                                            \
                                                                               \
  LINKAGE void TYPE##_push_one(TYPE *s, ELEM_TYPE i) {                         \
    SRB_UNWRAP(TYPE##_try_push_one(s, i));                                     \
  }

/**
 * @returns the number of elements present in `s`
 *
 * size_t srb_len(size_t head, size_t tail, size_t size);
 */
#define srb_len(head, tail, size)                                              \
  (((head) <= (tail)) ? /* Gaps are at either side of the array.               \
                         * 00111100000                                         \
                         *   h   t                                             \
                         */                                                    \
       ((tail) - (head))                                                       \
                      : /* Gap is in the middle of the array.                  \
                         * 11111100011                                         \
                         *       t  h                                          \
                         */                                                    \
       ((tail) + ((size) - (head))))

/**
 * @brief Effectively does `*out = head + n;`, wrapping around on size, and
 * returning an error if it would overflow
 *
 * int srb_wrapping_pop(size_t *out, size_t head, size_t tail, size_t size,
 * size_t n);
 */
#define srb_wrapping_pop(out, head, tail, size, n)                             \
  (srb_len(head, tail, size) < (n)                                             \
       ? 1 /* TODO: wrapping_add */                                            \
       : (((head) + (n) >= (size) ? (*(out) = (head) + (n) - (size))           \
                                  : (*(out) = (head) + (n))),                  \
          0))

/**
 * @internal
 * @see SRB_DECL
 */
#define SRB_DECL_pop(LINKAGE, TYPE, ELEM_TYPE)                                 \
  LINKAGE int TYPE##_try_pop(TYPE *s, srb_##ELEM_TYPE##_slice v);              \
  LINKAGE void TYPE##_pop(TYPE *s, srb_##ELEM_TYPE##_slice v);                 \
  LINKAGE int TYPE##_try_pop_one(TYPE *s, ELEM_TYPE *i);                       \
  LINKAGE ELEM_TYPE TYPE##_pop_one(TYPE *s);

/**
 * @internal
 * @see SRB_DEF
 */
#define SRB_DEF_pop(LINKAGE, TYPE, ELEM_TYPE)                                  \
  LINKAGE int TYPE##_try_pop(TYPE *s, srb_##ELEM_TYPE##_slice v) {             \
    /* This function heavily mirrors TYPE##_try_pop, see that for explanation  \
     * of all the atomic operations occurring in here. */                      \
    size_t next_empty;                                                         \
    size_t empty;                                                              \
    do {                                                                       \
      empty = atomic_load(&s->committed_empty);                                \
      /* TODO: checked add */                                                  \
      next_empty = empty + v.size;                                             \
      if (next_empty > s->buffer.size) {                                       \
        return 1;                                                              \
      }                                                                        \
    } while (!atomic_compare_exchange_strong(&s->committed_empty, &empty,      \
                                             next_empty));                     \
    size_t next_head;                                                          \
    size_t head;                                                               \
    do {                                                                       \
      head = atomic_load(&s->head_commit);                                     \
      size_t tail = atomic_load(&s->tail_valid);                               \
      SRB_TRY(                                                                 \
          srb_wrapping_pop(&next_head, head, tail, s->buffer.size, v.size));   \
    } while (                                                                  \
        !atomic_compare_exchange_weak(&s->head_commit, &head, next_head));     \
                                                                               \
    SRB_PRINTF("pop: v.size %zu s->buffer.size %zu head %zu next_head %zu\n",  \
               v.size, s->buffer.size, head, next_head);                       \
                                                                               \
    /* TODO: checked mul */                                                    \
    if (head + v.size <= s->buffer.size) {                                     \
      memcpy(v.data, &s->buffer.data[head], v.size * sizeof(ELEM_TYPE));       \
    } else {                                                                   \
      size_t before_end = s->buffer.size - head;                               \
      memcpy(v.data, &s->buffer.data[head], before_end * sizeof(ELEM_TYPE));   \
      memcpy(&v.data[before_end], s->buffer.data,                              \
             (v.size - before_end) * sizeof(ELEM_TYPE));                       \
    }                                                                          \
                                                                               \
    while (!atomic_compare_exchange_weak(&s->head_valid, &head, next_head))    \
      ;                                                                        \
                                                                               \
    atomic_fetch_sub(&s->committed_filled, v.size);                            \
    return 0;                                                                  \
  }                                                                            \
                                                                               \
  LINKAGE void TYPE##_pop(TYPE *s, srb_##ELEM_TYPE##_slice v) {                \
    SRB_UNWRAP(TYPE##_try_pop(s, v));                                          \
  }                                                                            \
                                                                               \
  LINKAGE int TYPE##_try_pop_one(TYPE *s, ELEM_TYPE *i) {                      \
    srb_##ELEM_TYPE##_slice out = {i, 1};                                      \
    return TYPE##_try_pop(s, out);                                             \
  }                                                                            \
                                                                               \
  LINKAGE ELEM_TYPE TYPE##_pop_one(TYPE *s) {                                  \
    ELEM_TYPE out;                                                             \
    SRB_UNWRAP(TYPE##_try_pop_one(s, &out));                                   \
    return out;                                                                \
  }

#endif // SILLY_RINGBUFFER_H
