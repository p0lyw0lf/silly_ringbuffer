#define SRB_LOG_TRACE
#include "srb.h"

SRB_DECL(static, srb, int);
SRB_DEF(static, srb, int);

int main() {
  srb q;
  SRB_INIT(q, 4);
  srb_int_slice v = (srb_int_slice){(int[]){1, 2, 3}, 3};
  assert(srb_try_push(&q, v) == 0);
  assert(srb_try_pop(&q, v) == 0);
  assert(v.data[0] == 1);
  assert(v.data[1] == 2);
  assert(v.data[2] == 3);

  assert(srb_try_push_one(&q, 4) == 0);
  assert(srb_try_push_one(&q, 5) == 0);
  assert(srb_try_push_one(&q, 6) == 0);
  assert(srb_try_pop(&q, v) == 0);
  assert(v.data[0] == 4);
  assert(v.data[1] == 5);
  assert(v.data[2] == 6);

  v.data = (int[]){7, 8, 9};
  assert(srb_try_push(&q, v) == 0);
  assert(srb_pop_one(&q) == 7);
  assert(srb_pop_one(&q) == 8);
  assert(srb_pop_one(&q) == 9);
  return 0;
}
