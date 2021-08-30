#include <lcc_test.h>
#include <lcc_pack.h>
#include <limits.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <alloca.h>


static int64_t gen_random_range(int64_t min, int64_t max)
{
   return min + (double)rand() / ((double)RAND_MAX / (max - min + 1) + 1);
}

static int test_intvals(void)
{
  int i= 0;
  char buffer[10];

  srand(time(NULL));

  for (i=0; i < 100; i++)
  {
    u_int8_t ui8;
    int8_t i8;
    u_int16_t ui16;
    int16_t i16;
    u_int32_t ui32;
    int32_t i32;
    u_int64_t ui64;
    int64_t i64;
    union {
      int64_t ival;
      double dval;
    } swap_dbl;
    double dval;
    float fval1, fval2;

    ui8= gen_random_range(0, UCHAR_MAX);
    ui8_to_p(buffer, ui8);
    ASSERT_EQ(ui8,p_to_ui8(buffer), "Values are not equal: %d, %d", ui8, p_to_ui8(buffer));
    i8= gen_random_range(CHAR_MIN, CHAR_MAX);
    i8_to_p(buffer, i8);
    ASSERT_EQ(i8, p_to_i8(buffer), "Values are not equal: %d, %d", i8, p_to_i8(buffer));

    ui16= gen_random_range(0, USHRT_MAX);
    ui16_to_p(buffer, ui16);
    ASSERT_EQ(ui16, p_to_ui16(buffer), "Values are not equal: %d, %d", ui16, p_to_ui16(buffer));
    i16= gen_random_range(SHRT_MIN, SHRT_MAX);
    i16_to_p(buffer, i16);
    ASSERT_EQ(i16, p_to_i16(buffer), "Values are not equal: %d, %d", i16, p_to_i16(buffer));

    ui32= gen_random_range(0, UINT_MAX);
    ui32_to_p(buffer, ui32);
    ASSERT_EQ(ui32,p_to_ui32(buffer), "Values are not equal: %d, %d", ui32, p_to_ui32(buffer));
    i32= gen_random_range(INT_MIN, INT_MAX);
    i32_to_p(buffer, i32);
    ASSERT_EQ(i32, p_to_i32(buffer), "Values are not equal: %d, %d", i32, p_to_i32(buffer));

    ui64= gen_random_range(0, LLONG_MAX);
    ui64_to_p(buffer, ui64);
    ASSERT_EQ(ui64,p_to_ui64(buffer), "Values are not equal: %d, %d", ui64, p_to_ui64(buffer));
    i64= gen_random_range(LLONG_MIN, LLONG_MAX);
    i64_to_p(buffer, i64);
    ASSERT_EQ(i64, p_to_i64(buffer), "Values are not equal: %d, %d", i64, p_to_i64(buffer));

    swap_dbl.ival= gen_random_range(LLONG_MIN, LLONG_MAX);
    d_to_p(buffer, swap_dbl.dval);
    ASSERT_EQ(swap_dbl.dval, p_to_d(buffer), "Values are not equal: %f %f", swap_dbl.dval, p_to_d(buffer));

    fval1= (float)gen_random_range(INT_MIN, INT_MAX);
    f4_to_p(buffer, fval1);
    p_to_f4(buffer, fval2);
    
    ASSERT_EQ(fval1, fval2, "Values are not equal: %f %f", fval1, fval2);
  }
  return OK;
}

static int test_lenc(void)
{
  u_int64_t ui64;
  u_char *buf= alloca(10), *p;
  u_int64_t max_range[] = {0, USHRT_MAX, UINT_MAX, ULLONG_MAX};

  p= buf;
  ui64= 0;
  p= lenc_to_p(p, ui64);
  p= buf;
  ASSERT_EQ(ui64, p_to_lenc(&p), "Wrong len encoding (len=%lld)", ui64);

  for (u_int8_t i= 0; i < 20; i++)
  {
    for (u_int8_t j= 0; j < 4; j++)
    {
      p= buf;
      ui64= gen_random_range(max_range[j] + 1, max_range[j + 1]);
      p= lenc_to_p(p, ui64);
      p= buf;
      ASSERT_EQ(ui64, p_to_lenc(&p), "Wrong len encoding (len=%lld)", ui64);
    }
  }
  return 0; 
}

static int test_timeenc()
{
  LCC_TIME l1, l2;
  u_int8_t len;
  char buf[20];

  memset(&l1, 0, sizeof(LCC_TIME));
  memset(&l2, 0, sizeof(LCC_TIME));
  memset(buf, 0, sizeof(buf));

  l1.type= TIME_DATE;
  l1.year= 2020;
  l1.month= 12;
  l1.day= 24;

  time_to_p(&l1, buf);
  p_to_time(buf, &l2);

  if (memcmp(&l1, &l2, sizeof(LCC_TIME)) != 0)
  {
    diag("wrong data");
    return FAIL;
  }

  memset(&l1, 0, sizeof(LCC_TIME));
  memset(&l2, 0, sizeof(LCC_TIME));
  memset(buf, 0, sizeof(buf));

  l1.type= TIME_DATETIME;
  l1.year= 2020;
  l1.month= 12;
  l1.day= 24;
  l1.hour = 8;
  l1.minute = 16;
  l1.second= 32;

  time_to_p(&l1, buf);
  p_to_time(buf, &l2);

  if (memcmp(&l1, &l2, sizeof(LCC_TIME)) != 0)
  {
    diag("wrong data");
    return FAIL;
  }

  memset(&l1, 0, sizeof(LCC_TIME));
  memset(&l2, 0, sizeof(LCC_TIME));
  memset(buf, 0, sizeof(buf));

  l1.type= TIME_DATETIME;
  l1.year= 2020;
  l1.month= 12;
  l1.day= 24;
  l1.hour = 8;
  l1.minute = 16;
  l1.second= 32;
  l1.microseconds= 0xFFFFA;

  time_to_p(&l1, buf);
  p_to_time(buf, &l2);

  if (memcmp(&l1, &l2, sizeof(LCC_TIME)) != 0)
  {
    diag("wrong data");
    return FAIL;
  }

  memset(&l1, 0, sizeof(LCC_TIME));
  memset(&l2, 0, sizeof(LCC_TIME));
  memset(buf, 0, sizeof(buf));

  l1.type= TIME_TIME;
  l1.hour = 8;
  l1.minute = 16;
  l1.second= 32;
  l1.microseconds= 0xFFFFA;

  time_to_p(&l1, buf);
  p_to_time(buf, &l2);

  if (memcmp(&l1, &l2, sizeof(LCC_TIME)) != 0)
  {
    diag("wrong data");
    return FAIL;
  }

  return OK;
}

int main()
{
  plan(3);
  ok(!test_intvals());
  ok(!test_lenc());
  ok(!test_timeenc());

  done_testing();
  return 0;
}
