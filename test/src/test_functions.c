// https://github.com/ThrowTheSwitch/Unity/blob/master/docs/UnityAssertionsReference.md
#include <stdio.h>
#include <test_functions.h>

AOS_DEFINE(test_fn_resolve_0)
aos_future_t *test_fn_resolve_0(aos_future_t *future) {
  printf("test_fn0 (no_args)\n");
  aos_resolve(future);
  return future;
}

AOS_DEFINE(test_fn_resolve_1, int)
aos_future_t *test_fn_resolve_1(aos_future_t *future) {
  AOS_ARGS_T(test_fn_resolve_1) *args = aos_args_get(future);
  printf("test_fn1 (arg1:%d)\n", args->arg1);
  aos_resolve(future);
  return future;
}
