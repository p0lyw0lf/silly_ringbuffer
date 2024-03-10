#include "srb.h"
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

SRB_DECL(static, srb, size_t);
SRB_DEF(static, srb, size_t);

static atomic_size_t counter;
static size_t iterations;
static srb q;

int reader(void *arg) {
  uintptr_t r = (uintptr_t)arg;
  size_t last_i = 0;
  size_t i;
  do {
    // printf("reader %" PRIuPTR ": now reading\n", r);
    while (srb_try_pop_one(&q, &i)) {
      thrd_yield();
    }
    // printf("reader %" PRIuPTR ": read %zu\n", r, i);
    assert(last_i <= i);
    thrd_yield();
  } while (i < iterations);
  return 0;
}

int writer(void *arg) {
  uintptr_t w = (uintptr_t)arg;
  size_t i = 0;
  while (i < iterations) {
    i = atomic_fetch_add(&counter, 1);
    // printf("writer %" PRIuPTR ": now writing %zu\n", w, i);
    while (srb_try_push_one(&q, i)) {
      thrd_yield();
    }
    // printf("writer %" PRIuPTR ": finished writing\n", w);
    thrd_yield();
  }
  return 0;
}

long parse_arg(int n, int argc, char **argv, long default_arg) {
  if (n < 1 || n >= argc) {
    return default_arg;
  } else {
    char *end;
    return strtol(argv[n], &end, 10);
  }
}

int main(int argc, char **argv) {
  uintptr_t num_readers = parse_arg(1, argc, argv, 1);
  uintptr_t num_writers = parse_arg(2, argc, argv, 1);
  iterations = parse_arg(3, argc, argv, 100000);
  size_t buffer_size = parse_arg(3, argc, argv, 256);

  atomic_init(&counter, 0);

  thrd_t *readers = malloc(sizeof(thrd_t) * num_readers);
  thrd_t *writers = malloc(sizeof(thrd_t) * num_writers);

  SRB_INIT(q, buffer_size);

  // Start all threads
  for (uintptr_t r = 0; r < num_readers; r++) {
    assert(thrd_create(&readers[r], reader, (void *)r) == thrd_success);
  }
  for (uintptr_t w = 0; w < num_writers; w++) {
    assert(thrd_create(&writers[w], writer, (void *)w) == thrd_success);
  }

  // Join on all writers (readers should always be pulling to let these finish)
  for (uintptr_t w = 0; w < num_writers; w++) {
    assert(thrd_join(writers[w], NULL) == thrd_success);
  }

  // Push kill values to let all readers know it's time to stop
  for (uintptr_t r = 1; r < num_readers; r++) {
    while (srb_try_push_one(&q, iterations))
      ;
  }

  for (uintptr_t r = 0; r < num_readers; r++) {
    assert(thrd_join(readers[r], NULL) == thrd_success);
  }

  free(readers);
  free(writers);

  SRB_FREE(q);

  return 0;
}
