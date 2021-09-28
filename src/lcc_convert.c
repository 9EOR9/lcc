/* conversion functions */

#include <lcc.h>
#include <lcc_priv.h>
#include <lcc_error.h>
#include <ctype.h>
#include <math.h>

/**
 * @brief: converts a string with given length to 
 *         an unsigned 64-bit integer value
 *
 **/
static uint64_t
lcc_convert_str2ui64(const char *buf, size_t buflen, int *error)
{
  uint8_t i;
  uint64_t ret= 0;
  const char *pos= buf;

  if (!error)
    error= (int *)alloca(sizeof(int));

  *error= 0;

  for (i=0; i < buflen; i++)
  {
    if (!isdigit(*pos))
      break;

    /* overflow check */
    if (ret > UINT64_MAX / 10 ||
        ret * 10 > UINT64_MAX - (*pos - '0'))
    {
      *error= ERANGE;
      break;
    }
    ret+= ret * 10 + (*pos - '0');
  }
  if (pos == buf)
    *error= ERANGE;
  return ret;
}

/**
 * @brief: converts a string with given length to 
 *         an unsigned 64-bit integer value
 *
 **/
static int64_t
lcc_convert_str2i64(const char *buf, size_t buflen, int *error)
{
  uint8_t negative= 0;
  uint64_t ret= 0;
  const char *pos= buf;

  if (!error)
    error= (int *)alloca(sizeof(int));

  *error= 0;

  /* remove trailing whitespeaces */
  while (pos < buf + buflen && isspace(*pos))
    pos++;

  if (pos == buf + buflen)
  {
    *error= ERANGE;
    return 0;
  }

  /* checking sign */
  if (*pos == '-')
  {
    negative= 1;
    pos++;
  }

  ret= lcc_convert_str2ui64(pos, buflen - (pos - buf), error);
  if (*error)
    return ret;

  /* check range */
  if (ret > (uint64_t)INT64_MAX - negative)
  {
    *error= ERANGE;
    ret= INT64_MAX - negative;
  }
  return (negative) ? (int64_t)ret * -1 : (int64_t)ret;
}

static uint64_t lcc_convert_str2i(const char *buf, size_t buflen, uint8_t is_unsigned, uint8_t bytes, int *error)
{
  int64_t max_limit;
  int64_t min_limit;
  int64_t ret;

  if (!error)
    error= (int *)alloca(sizeof(int));

  if (bytes > 4 || bytes < 1)
  {
    *error= ERANGE;
    return 0;
  }

  max_limit = (is_unsigned) ? pow(256, bytes) - 1 : (pow(256, bytes) - 2) / 2;
  min_limit = (is_unsigned) ? 0 : - pow(256, bytes) / 2;

  if (!error)
    error= (int *)alloca(sizeof(int));

  ret= lcc_convert_str2i64(buf, buflen, error);
  if (error)
    return 0;
  if (ret > max_limit || ret < min_limit)
    *error= ERANGE;
  return ret; 
}

static inline uint8_t lcc_convert_str2ui8(char *buf, size_t buflen, int *error)
{
  return (uint8_t)lcc_convert_str2i(buf, buflen, 1, 1, error);
}

static inline int8_t lcc_convert_str2i8(char *buf, size_t buflen, int *error)
{
  return (int8_t)lcc_convert_str2i(buf, buflen, 0, 1, error);
}

static inline uint16_t lcc_convert_str2ui16(char *buf, size_t buflen, int *error)
{
  return (uint16_t)lcc_convert_str2i(buf, buflen, 1, 2, error);
}

static inline int16_t lcc_convert_str2i16(char *buf, size_t buflen, int *error)
{
  return (int16_t)lcc_convert_str2i(buf, buflen, 0, 2, error);
}

static inline uint32_t lcc_convert_str2ui32(char *buf, size_t buflen, int *error)
{
  return (uint32_t)lcc_convert_str2i(buf, buflen, 1, 4, error);
}

static inline int32_t lcc_convert_str2i32(char *buf, size_t buflen, int *error)
{
  return (int32_t)lcc_convert_str2i(buf, buflen, 0, 4, error);
}
