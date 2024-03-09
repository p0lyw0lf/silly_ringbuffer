#include "srb.h"
#include <stdio.h>

int main() {
  printf("Starting tests...\n");

  // Default-initialization is fine.
  SRB_INIT(srb, q, 8);
  // Can push single elements
  srb_push_one(&q, 1);
  srb_push_one(&q, 2);
  // Can also push multiple elements at a time
  int vs[] = {3, 4, 5};
  srb_push(&q, int_view(vs, 3));

  // Can pop one, or multiple elements
  assert(srb_pop_one(&q) == 1);
  assert(srb_try_pop(&q, int_view(vs, 3)) == 0);
  assert(vs[0] == 2);
  assert(vs[1] == 3);
  assert(vs[2] == 4);
  assert(!srb_empty(&q));
  assert(srb_pop_one(&q) == 5);
  assert(srb_empty(&q));

  // Can also initialize with a given buffer size ahead of time.
  /*
  SRB_INITN(charq, &c, 8);
  assert(charq_push(&c, srb_view_nt("foo")) == 0);
  assert(charq_push(&c, srb_view_nt("bar!!")) == 0);
  assert(charq_full(&c));
  assert(charq_try_push(&c, '?') != 0);
  */

  SRB_FREE(q);
  printf("Completed successfully\n");

  return 0;
}
