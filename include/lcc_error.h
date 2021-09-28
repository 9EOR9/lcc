/* 

   Error messages:

   Client error messages start with 2000, so we will not collide
   with error messages from server.

*/
#pragma once

#include <string.h>
#include <errno.h>

#define LCC_ERROR_INFO __FILE__, __func__, __LINE__

/* clear error and reset sqlstate to "00000" */
static inline void lcc_clear_error(LCC_ERROR *error)
{
  memset(error, '\0', sizeof(LCC_ERROR));
  strcpy(error->sqlstate, "00000");
}

/* set error */
LCC_ERRNO
lcc_set_error(LCC_ERROR *error,
              const char *file,
              const char *func,
              uint32_t lineno,
              uint16_t error_no,
              const char *sqlstate,
              const char *error_message,
              ...);

#define ER_OK                               0 /* success */

#define ER_UNKNOWN                          2000
#define ER_INVALID_POINTER                  2001
#define ER_INVALID_HANDLE                   2002
#define ER_INVALID_SOCKET_DESCRIPTOR        2003
#define ER_OUT_OF_MEMORY                    2004
#define ER_MALFORMED_PACKET                 2005
#define ER_UNKNOWN_AUTH_METHOD              2006
#define ER_INVALID_HANDLE_TYPE              2007
#define ER_INVALID_OPTION                   2008
#define ER_COMM_READ                        2009
#define ER_COMM_WRITE                       2010
#define ER_NULL_VALUE_PARAMETER             2011
#define ER_ALREADY_INITIALIZED              2012
#define ER_INVALID_BUFFER_SIZE              2013
#define ER_UNSUPPORTED_SERVER_VERSION       2014
#define ER_INVALID_VALUE                    2015
#define ER_UNKNOWN_FIELD_ATTR               2016
#define ER_NO_RESULT_AVAILABLE              2017
#define ER_STMT_WITHOUT_PARAMETERS          2018
#define ER_STMT_NOT_READY                   2019

