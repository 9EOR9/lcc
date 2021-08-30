/* 

   Error messages:

   Client error messages start with 2000, so we will not collide
   with error messages from server.

*/
#pragma once

#include <string.h>

/* clear error and reset sqlstate to "00000" */
static inline void lcc_clear_error(lcc_error *error)
{
  memset(error, '\0', sizeof(lcc_error));
  strcpy(error->sqlstate, "00000");
}

/* set error */
void lcc_set_error(lcc_error *error,
                   u_int16_t error_no,
                   const char *sqlstate,
                   const char *error_message,
                   ...);

#define ER_OK                               0 /* success */

#define ER_UNKNOWN                          2000
#define ER_INVALID_CONNECTION_HANDLE        2001
#define ER_INVALID_STMT_HANDLE              2002
#define ER_INVALID_SOCKET_DESCRIPTOR        2003
#define ER_OUT_OF_MEMORY                    2004
#define ER_MALFORMED_PACKET                 2005
#define ER_UNKNOWN_AUTH_METHOD              2006
#define ER_INVALID_HANDLE_TYPE              2007
#define ER_INVALID_OPTION                   2008
#define ER_NET_READ                         2009
#define ER_NULL_VALUE_PARAMETER             2010
#define ER_ALREADY_INITIALIZED              2011

