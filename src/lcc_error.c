/*
 *
 */
#include <lcc.h>
#include <lcc_priv.h>
#include <lcc_error.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>

const char *lcc_errormsg[]= 
{
  /* 0000
     1999    reserved for server error messages */
     
  /* 2000 */ "Unknown error occured",
  /* 2001 */ "Invalid connection handle",
  /* 2002 */ "Invalid statement handle",
  /* 2003 */ "Invalid socket descriptor",
  /* 2004 */ "Failed to allocate memory (%ld Bytes).",
  /* 2005 */ "Malformed packet (Offset %ld)",
  /* 2006 */ "Unknown or unsupported authentication method: '%s'",
  /* 2007 */ "Invalid handle type specified",
  /* 2008 */ "Unknown or invalid option (%d) specified",
  /* 2009 */ "Error (%d) occured wile reading data from server",
  /* 2010 */ "Error (%d) occured while sending data to server",
  /* 2011 */ "Invalid or null value (Parameter='%s')",
  /* 2012 */ "Handle was already initialized",
  /* 2013 */ "Invalid buffer size",
  /* 2014 */ "This server version is not supported anymore",
  /* 2015 */ "Unknown or invalid handle",
  /* 2016 */ "Unknown field attribute (=%d)."
  /* 2017 */ "No result set available."
  /* 2018 */ "Statement doesn't have parameter(s)."
  /* 2019 */ "Statement can't be executed yet."
};

#define LCC_CLIENT_ERROR(x) lcc_errormsg[(x)-2000]
#define LCC_UNKNOWN_SQLSTATE "HY000"

LCC_ERRNO
lcc_set_error(LCC_ERROR *error,
              const char *file,
              const char *func,
              uint32_t lineno,
              uint16_t error_no,
              const char *sqlstate,
              const char *error_message,
              ...)
{
  va_list ap;
  const char *errmsg;

  if ((error_no < 2000) |
     ((size_t)error_no - 2000 > sizeof(lcc_errormsg) / sizeof(char *)) && !error_message)
  {
    /* we can't determine error message, return unknown error */
    errmsg= LCC_CLIENT_ERROR(ER_UNKNOWN);
  }
  else
  {
    /* if error_message was specified, don't use default error message */
    errmsg= (error_message) ? error_message : LCC_CLIENT_ERROR(error_no);
  }

  /* if sqlstate was not specifiied, use default HY000 */
  strncpy(error->sqlstate, sqlstate ? sqlstate : LCC_UNKNOWN_SQLSTATE, 5);
  error->error_number= error_no;
  
  va_start(ap, error_message);
  vsnprintf(error->error, LCC_MAX_ERROR_LEN, errmsg, ap);
  va_end(ap);

  error->info.file= file;
  error->info.func= func;
  error->info.lineno= lineno;

  return error_no;
}
