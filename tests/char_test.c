#include "srb.h"
#include <string.h>

SRB_DECL(, charq, char);
SRB_DEF(, charq, char);

srb_char_slice srb_string_view(char *s) {
  return (srb_char_slice){s, strlen(s)};
}
srb_char_slice srb_string(size_t len) {
  srb_char_slice out = {malloc((len + 1) * sizeof(char)), len};
  assert(out.data != NULL);
  out.data[len] = '\0';
  return out;
}

int main() {
  charq c;
  SRB_INIT(c, 8);
  assert(charq_try_push(&c, srb_string_view("foo")) == 0);
  assert(charq_try_push(&c, srb_string_view("bar!")) == 0);
  assert(charq_try_push_one(&c, '?') != 0);

  srb_char_slice out = srb_string(7);
  assert(charq_try_pop(&c, out) == 0);
  assert(strcmp(out.data, "foobar!") == 0);

  return 0;
}
