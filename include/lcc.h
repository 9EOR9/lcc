#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <lcc_config.h>

#ifdef _WIN32
#define API_FUNC __stdcall
#else
#define API_FUNC
#endif

#define LCC_NTS -1 /** Zero terminated string **/
#define LCC_MAX_ERROR_LEN 255UL

#define CHECK_HANDLE_TYPE(hdl, hdl_type) \
if (!(hdl)) \
  return ER_INVALID_POINTER; \
if ((hdl)->type != (hdl_type)) \
  return ER_INVALID_HANDLE;

typedef enum {
  SERVER_INFO_CURRENT_DB= 0,
  SERVER_INFO_CAPABILITIES,
  SERVER_INFO_STATUS,
  SERVER_INFO_AFFECTED_ROWS,
  SERVER_INFO_LAST_INSERT_ID,
  SERVER_INFO_WARNING_COUNT,
  SERVER_INFO_COLUMN_COUNT,
  RESULT_INFO_ROW_COUNT,
  RESULT_INFO_COLUMNS,
  RESULT_INFO_PROTOCOL,
  RESULT_INFO_ROW
} LCC_INFO;

typedef enum {
  LCC_OPT_CURRENT_DB= 1,
  LCC_OPT_SOCKET_NO,
  LCC_OPT_TLS_CERT,
  LCC_OPT_TLS_KEY,
  LCC_OPT_TLS_CA,
  LCC_OPT_TLS_CA_PATH,
  LCC_OPT_TLS_CIPHER,
  LCC_OPT_TLS_CRL,
  LCC_OPT_TLS_CRL_PATH,
  LCC_OPT_TLS_VERIFY_PEER,
  LCC_OPT_AUTH_PLUGIN,
  LCC_OPT_REMEMBER_CONFIG,
  LCC_OPT_USER,
  LCC_OPT_PASSWORD,
  LCC_OPT_STATUS_CALLBACK,
  LCC_OPT_SESSION_STATUS_CALLBACK,
  LCC_OPT_PROGRESS_REPORT_CALLBACK,
  LCC_OPT_INVALID_OPTION= 0xFFFF
} LCC_OPTION;

typedef enum {
  LCC_CONNECTION= 0,
  LCC_STATEMENT,
  LCC_RESULT
} LCC_HANDLE_TYPE;

/* Capabilities */
#define CAP_MYSQL                                   1
#define CAP_FOUND_ROWS                              2
#define CAP_LONG_FLAG                               4
#define CAP_CONNECT_WITH_DB                         8
#define CAP_NO_SCHEMA                              16
#define CAP_COMPRESS                               32
#define CAP_ODBC                                   64
#define CAP_LOCAL_FILES                           128
#define CAP_IGNORE_SPACE                          256
#define CAP_PROTOCOL_41                           512
#define CAP_INTERACTIVE	                         1024
#define CAP_TLS                                  2048
#define CAP_IGNORE_SIGPIPE                       4096
#define CAP_TRANSACTIONS                         8192
#define CAP_RESERVED                            16384
#define CAP_SECURE_CONNECTION                   32768  
#define CAP_MULTI_STATEMENTS               (1UL << 16)
#define CAP_MULTI_RESULTS                  (1UL << 17)
#define CAP_PS_MULTI_RESULTS               (1UL << 18)
#define CAP_PLUGIN_AUTH                    (1UL << 19)
#define CAP_CONNECT_ATTRS                  (1UL << 20)
#define CAP_PLUGIN_AUTH_LENENC_CLIENT_DATA (1UL << 21)
#define CAP_CAN_HANDLE_EXPIRED_PASSWORDS   (1UL << 22)
#define CAP_SESSION_TRACKING               (1UL << 23)
#define CAP_TLS_VERIFY_SERVER_CERT         (1UL << 30)
#define CAP_REMEMBER_OPTIONS               (1UL << 31)

#define CAP_PROGRESS                       (1ULL << 32)
#define CAP_STMT_BULK_OPERATIONS           (1ULL << 34)
#define CAP_EXTENDED_METADATA              (1ULL << 35)
#define CAP_CACHE_METADATA                 (1ULL << 36)

#define MARIADB_CAPS                   (CAP_PROGRESS |\
                                        CAP_STMT_BULK_OPERATIONS|\
                                        CAP_EXTENDED_METADATA)

#define CLIENT_CAP_FLAGS               (CAP_MYSQL |\
                                        CAP_FOUND_ROWS |\
                                        CAP_LONG_FLAG |\
                                        CAP_CONNECT_WITH_DB |\
                                        CAP_NO_SCHEMA |\
                                        CAP_COMPRESS |\
                                        CAP_LOCAL_FILES |\
                                        CAP_IGNORE_SPACE |\
                                        CAP_INTERACTIVE |\
                                        CAP_TLS |\
                                        CAP_IGNORE_SIGPIPE |\
                                        CAP_TRANSACTIONS |\
                                        CAP_PROTOCOL_41 |\
                                        CAP_RESERVED |\
                                        CAP_SECURE_CONNECTION |\
                                        CAP_MULTI_STATEMENTS |\
                                        CAP_MULTI_RESULTS |\
                                        CAP_PROGRESS |\
                                        CAP_TLS_VERIFY_SERVER_CERT |\
                                        CAP_REMEMBER_OPTIONS |\
                                        CAP_PLUGIN_AUTH |\
                                        CAP_SESSION_TRACKING |\
                                        CAP_CONNECT_ATTRS)

#define CLIENT_CAPS               	    (CAP_MYSQL | \
                                         CAP_LONG_FLAG |\
                                         CAP_TRANSACTIONS |\
                                         CAP_SECURE_CONNECTION |\
                                         CAP_MULTI_RESULTS | \
                                         CAP_PS_MULTI_RESULTS |\
                                         CAP_PROTOCOL_41 |\
                                         CAP_PLUGIN_AUTH |\
                                         CAP_PLUGIN_AUTH_LENENC_CLIENT_DATA | \
                                         CAP_SESSION_TRACKING |\
                                         CAP_CONNECT_ATTRS)


/* Compress and TLS will be activated on demand */
#define CLIENT_DEFAULT_CAPS             ((CLIENT_CAP_FLAGS & ~CAP_COMPRESS) & ~CAP_TLS)

typedef struct {
  LCC_HANDLE_TYPE type;
} LCC_HANDLE;

static inline uint8_t lcc_valid_handle(LCC_HANDLE *handle, LCC_HANDLE_TYPE type)
{
  return (handle && handle->type == type);
}

typedef struct {
  char *str;
  size_t len;
} LCC_STRING;

typedef uint16_t LCC_ERRNO;

typedef struct {
  const char *file;
  const char *func;
  uint32_t lineno;
} LCC_ERROR_INFO;

typedef struct {
  char sqlstate[6];
  uint16_t error_number;
  char error[LCC_MAX_ERROR_LEN];
  LCC_ERROR_INFO info;
} LCC_ERROR;

typedef struct lcc_list {
  struct lcc_list *next;
  void *data;
} LCC_LIST;

typedef enum lcc_session_state_type {
  TRACK_SYSTEM_VARIABLES= 0,
  TRACK_SCHEMA,
  TRACK_STATE_CHANGE,
  TRACK_GTID,
  TRACK_TRANSACTION_CHARACTERISTICS,
  TRACK_TRANSACTION_STATE
} LCC_SESSION_STATE_TYPE;

typedef struct {
  LCC_SESSION_STATE_TYPE type;
  LCC_STRING str;
} LCC_SESSION_TRACK_INFO;

typedef enum {
  LCC_FIELD_ATTR_DATA_TYPE,
  LCC_FIELD_ATTR_FORMAT
} LCC_FIELD_ATTR;

#define LCC_MAX_FIELD_ATTRS LCC_FIELD_ATTR_FORMAT + 1

typedef struct {
  const char *catalog;
  const char *column_name;
  const char *column_alias;
  const char *table_name;
  const char *table_alias;
  const char *schema;
  const char *default_value;
  const char *metadata[LCC_MAX_FIELD_ATTRS];
  uint8_t decimals;
  uint16_t flags;
  uint16_t charset_nr;
  uint32_t column_size;
  uint32_t max_column_size;
  uint8_t type;
} LCC_COLUMN;

/* Server status flags */
#define LCC_STATUS_IN_TRANS               1
#define LCC_STATUS_AUTOCOMMIT             2
#define LCC_STATUS_MORE_RESULTS_EXIST     8
#define LCC_STATUS_NO_GOOD_INDEX_USED    16
#define LCC_STATUS_NO_INDEX_USED         32
#define LCC_STATUS_CURSOR_EXISTS         64
#define LCC_STATUS_LAST_ROW_SENT        128
#define LCC_STATUS_DB_DROPPED             1 << 8 
#define LCC_STATUS_NO_BACKSLASH_ESCAPE    1 << 9
#define LCC_STATUS_METADATA_CHANGED       1 << 10
#define LCC_STATUS_SLOW_QUERY             1 << 11
#define LCC_STATUS_PS_OUTPARAMS           1 << 12
#define LCC_STATUS_IN_TRANS_READONLY      1 << 13
#define LCC_STATUS_SESSION_STATE_CHANGED  1 << 14

/* API calls */
LCC_ERRNO API_FUNC
LCC_init_handle(LCC_HANDLE **handle, LCC_HANDLE_TYPE type, LCC_HANDLE *connection);

LCC_ERRNO API_FUNC
LCC_get_info(LCC_HANDLE *handle, LCC_INFO info, void *buffer);

LCC_ERRNO API_FUNC
lcc_close_handle(LCC_HANDLE *handle);

LCC_ERRNO API_FUNC
LCC_configuration_set(LCC_HANDLE *handle,
                      const char *option_str,
                      LCC_OPTION option,
                      void *buffer);

const LCC_COLUMN API_FUNC
*LCC_result_columns(LCC_HANDLE *handle);

LCC_ERRNO 
LCC_set_option(LCC_HANDLE *hdl, LCC_OPTION option, ...);

uint8_t
LCC_configuration_load_file(LCC_HANDLE *handle, const char **filenames, const char *section);

#ifdef __cplusplus
}
#endif
