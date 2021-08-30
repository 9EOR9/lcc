/*********************************************************
 * License
 *
 * (C) 2021 Georg Richter
 **********************************************************/
#pragma once

#include <lcc_config.h>
#include <sys/types.h>
#include <string.h>
#include <assert.h>
#include <byteswap.h>

#define LCC_MAX_USER_LEN 128

typedef struct {
  u_int32_t packet_len;
  u_int8_t  sequence;
} lcc_packet_header;

#define LCC_PACKET_ERROR(header) (header->packet_len == (u_int32_t)-1)

#define LCC_NET_BUFFER_SIZE 0x2000

enum enum_lcc_time {
  TIME_NONE= 0,
  TIME_ERROR,
  TIME_TIMESTAMP,
  TIME_DATETIME,
  TIME_DATE,
  TIME_TIME
};

typedef struct {
  enum enum_lcc_time type;
  u_int16_t year;
  u_int8_t  month, day;
  u_int8_t  hour, minute, second;
  u_int32_t microseconds;
  u_int8_t  negative;
} LCC_TIME;

/* endianess dependend macros for int values */
#ifdef HAVE_BIGENDIAN
#define i16_to_le(i) bswap_16((i))
#define i32_to_le(i) bswap_32((i))
#define i64_to_le(i) bswap_64((i))

static inline float
float_to_le(float f)
{
  union
  {
    float fval;
    char bytes[4];
  } src, dst;

  src.fval= f;
  dst.bytes[3]= src.bytes[0];
  dst.bytes[2]= src.bytes[1];
  dst.bytes[1]= src.bytes[2];
  dst.bytes[0]= src.bytes[3];

  return dst.fval;
}

#else
#define i16_to_le(i) (i)
#define i32_to_le(i) (i)
#define i64_to_le(i) (i)
#define float_to_le(f) (f)
#endif


/* Macros to store values in protocol format */
   
/* Convert values between host and client/server protocol byte order:
 * In contrast to the network protocol, the client-server protocol for
 * MariaDB/MySQL uses little endian coding.
 */


/*
 *  store/get integer values:
 *  macros with _inc suffix increase the buffer pointer
 */

#define i8_to_p(b, i)       *((int8_t *)(b))= (i)
#define i8_to_p_inc(b, i)   i8_to_p((b),(i)); (b)++
#define ui8_to_p(b, u)      *((u_int8_t *)(b))= (u)
#define ui8_to_p_inc(b, u)  ui8_to_p((b),(u)); (b)++
#define i16_to_p(b, i)      *((int16_t *)(b))= (i)
#define i16_to_p_inc(b, i)  i16_to_p((b),(i)); (b)+= 2
#define ui16_to_p(b, u)     *((u_int16_t *)(b))= (u)
#define ui24_to_p(b, u)     {\
                              *(b)=   (u_char)(u);\
                              *(b+1)= (u_int32_t)(u) >> 8;\
                              *(b+2)= (u_int32_t)(u) >> 16;\
                            }
#define ui24_to_p_inc(b, u) ui24_to_p((b),(u)); (b)+= 3
#define i32_to_p(b, i)      *((int32_t *)(b)) = (i)
#define i32_to_p_inc(b, i)  i32_to_p((b),(i)); b+= 4
#define ui32_to_p(b, u)     *((u_int32_t *)(b)) = (u)
#define ui32_to_p_inc(b,u)  ui32_to_p((b),(u)); (b)+= 4
#define i64_to_p(b, i)      *((int64_t *)(b)) = (i)
#define i64_to_p_inc(b, i)  i64_to_p((b),(i)); b+= 8
#define ui64_to_p(b, u)     *((u_int64_t *)(b)) = (u)
#define ui64_to_p_inc(b, u) ui64_to_p((b),(u)); b+= 8

#define p_to_i8(b)       *((int8_t *)(b))
#define p_to_i8_inc(b)   p_to_i8((b)); b++;
#define p_to_ui8(b)      (u_int8_t)p_to_i8(b)
#define p_to_ui8_inc(b)  p_to_ui8((b)++);
#define p_to_i16(b)      *((int16_t *)(b))
#define p_to_ui16(b)     *((u_int16_t *)(b))
#define p_to_ui16_inc(b) *((u_int16_t *)(b)); b+= 2;
#define p_to_ui24(b)     (u_int32_t)(*((u_int32_t *)(b)) & 0xFFFFFF)
#define p_to_i32(b)      *((int32_t *)(b))
#define p_to_ui32(b)     *((u_int32_t *)(b))
#define p_to_i64(b)      *((int64_t *)(b))
#define p_to_ui64(b)     *((u_int64_t *)(b))

/* store/get float value (IEEE 754 single precision) */
#define f4_to_p(b,f)    memcpy((void *)(b), (&f), sizeof(float))
#define p_to_f4(b, f)   memcpy((&f), (void *)b, sizeof(float));

/* store/get double values (IEEE 754 double precision) */
typedef union {
  double dval;
  int64_t ival;
} dbl_swap;

static inline void d_to_p(u_char *b, double d)
{
  dbl_swap ds;
  ds.dval= d;
  i64_to_p(b, ds.ival);
}

static inline double p_to_d(u_char *b)
{
  dbl_swap ds;
  ds.ival= p_to_i64(b);
  return ds.dval;
}

/* store length encoded and increase buffer offset */
static inline u_char *lenc_to_p(u_char *b, u_int64_t lenc)
{
  if (lenc < 251ULL)
  {
    ui8_to_p(b, (u_char)lenc);
    return b+1;
  } else if (lenc < 65536ULL)
  {
    *b++= 0xFC;
    ui16_to_p(b, (u_int16_t)lenc);
    return b+2;
  } else if (lenc < 16777216ULL)
  {
    *b++= 0xFD;
    ui24_to_p(b, (u_int32_t)lenc);
    return b+3;
  }
  *b++= 0xFE;
  ui64_to_p(b, lenc);
  return b+8;
}

static inline u_int64_t p_to_lenc(u_char **p)
{
  u_char *pos= *p;

  if (*pos < 251)
  {
    *p++;
    return (u_int64_t)p_to_ui8(pos);
  }
  if (*pos == 0xFB)
  {
    p++;
    return ((u_int64_t)~0);
  }
  if (*pos == 0xFC)
  {
    p+=3;
    return (u_int64_t)p_to_ui16(pos+1);
  } else if (*pos == 0xFD)
  {
    p+=4;
    return (u_int64_t)p_to_ui24(pos+1);
  } else if (*pos == 0xFE)
  {
    p+=9;
    return p_to_ui64(pos+1);
  }
  return 0;
}

static inline u_int8_t time_to_p(LCC_TIME *ltime, u_char *b)
{
  u_int8_t len= 0;
  switch(ltime->type)
  {
    case TIME_DATE:
      len= 4;
      break;
    case TIME_DATETIME:
      len= 7 + 4 * (ltime->microseconds > 0);
      break;
    case TIME_TIME:
      len= 9 + 4 * (ltime->microseconds > 0);
      break;
    default:
      return 0;
  }
  ui8_to_p_inc(b, len);
  if (ltime->type == TIME_TIME)
  {
    ui8_to_p_inc(b, ltime->negative);
  }
  ui16_to_p(b, ltime->year); 
  b+= 2;
  ui8_to_p_inc(b, ltime->month);
  ui8_to_p_inc(b, ltime->day);

  if (ltime->type == TIME_DATE)
    return len;

  ui8_to_p_inc(b, ltime->hour);
  ui8_to_p_inc(b, ltime->minute);
  ui8_to_p_inc(b, ltime->second);

  if (ltime->microseconds)
    ui32_to_p(b, ltime->microseconds);
  
  return len;
}

/* converts buffer into time structure, returns
   number of characters read from buffer */
static inline u_int8_t p_to_time(u_char *b, LCC_TIME *ltime)
{
  u_int8_t len= *b;
  u_int8_t has_ms= 0;

  memset(ltime, 0, sizeof(LCC_TIME));

  switch (len) {
    case 4:
      ltime->type= TIME_DATE;
      break;
    case 7:
    case 11:
      ltime->type= TIME_DATETIME;
      has_ms= (len == 11);
      break;
    case 9:
    case 13:
      ltime->type= TIME_TIME;
      has_ms= (len == 13);
      break;
    default:
      /* invalid length */
      ltime->type= TIME_ERROR;
      return len;
  }
  b++;
  if (ltime->type == TIME_TIME)
  {
    ltime->negative= p_to_ui8_inc(b);
  }
  ltime->year= p_to_ui16(b);
  b+= 2;
  ltime->month= p_to_ui8_inc(b);
  ltime->day= p_to_ui8_inc(b);
  
  ltime->hour= p_to_ui8_inc(b);
  ltime->minute= p_to_ui8_inc(b);
  ltime->second= p_to_ui8_inc(b);

  if (has_ms)
  {
    ltime->microseconds= p_to_ui32(b);
  }
  return len;
}

