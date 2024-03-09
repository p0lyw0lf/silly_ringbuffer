#include "srb.h"

SRB_DECL(, srb, int);
SRB_DEF(, srb, int);

int main() {
  // Default-initialization is fine.
  SRB_INIT(srb, q, 6);
  // Can push single elements
  assert(srb_try_push_one(&q, 1) == 0);
  assert(srb_try_push_one(&q, 2) == 0);
  // Can also push multiple elements at a time
  int vs[] = {3, 4, 5};
  assert(srb_try_push(&q, (srb_int_slice){vs, 3}) == 0);
  // Trying to push elements beyond the capacity will return an error.
  // Note that the capacity is actually 5; we always keep an empty space around.
  assert(srb_try_push_one(&q, 6) != 0);

  // Can pop one, or multiple elements
  assert(srb_pop_one(&q) == 1);
  assert(srb_try_pop(&q, (srb_int_slice){vs, 3}) == 0);
  assert(vs[0] == 2);
  assert(vs[1] == 3);
  assert(vs[2] == 4);
  int x;
  assert(srb_try_pop_one(&q, &x) == 0);
  assert(x == 5);
  // Trying to pop an empty queue will return an error
  assert(srb_try_pop_one(&q, &x) != 0);

  SRB_FREE(q);

  return 0;
}
