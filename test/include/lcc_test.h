#ifndef _LCC_TEST_H_
#define _LCC_TEST_H_

#include <tap.h>

#define OK   0
#define FAIL 1
#define SKIP 2

#define ASSERT_EQ(val1, val2, text, ...)\
if ((val1) != (val2))\
{\
  diag("File: %s:%d", __FILE__, __LINE__);\
  diag(text, ##__VA_ARGS__);\
  return FAIL;\
}

#endif
