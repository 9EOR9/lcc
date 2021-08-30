#pragma once

#include <sys/types.h>
#include <stddef.h>

enum LCC_get_server_info {
  GET_CURRENT_DB= 0,
  GET_CAPABILITIES,
  GET_STATUS,
  GET_AFFECTED_ROWS,
  GET_LAST_INSERT_ID,
  GET_WARNING_COUNT
};

enum LCC_option {
  LCC_CURRENT_DB= 1,
  LCC_SOCKET_NO,
  LCC_TLS_CERT,
  LCC_TLS_KEY,
  LCC_TLS_CA,
  LCC_TLS_CA_PATH,
  LCC_TLS_CIPHER,
  LCC_TLS_CRL,
  LCC_TLS_CRL_PATH,
  LCC_TLS_VERIFY_PEER,
  LCC_AUTH_PLUGIN,
  LCC_REMEMBER_CONFIG,
  LCC_INVALID_OPTION= 0xFFFF
};

enum LCC_handle_type {
  LCC_CONNECTION= 0,
  LCC_STATEMENT
};

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

/* Compress and TLS will be activated on demand */
#define CLIENT_DEFAULT_CAPS             ((CLIENT_CAP_FLAGS & ~CAP_COMPRESS) & ~CAP_TLS)

typedef struct {
  enum LCC_handle_type type;
} lcc_handle;

typedef lcc_handle LCC_HANDLE;

/* API calls */
u_int32_t
LCC_init_handle(LCC_HANDLE **handle, enum LCC_handle_type type, void *base_handle);
u_int32_t
LCC_get_info(LCC_HANDLE *handle, enum LCC_get_server_info info, void *buffer);
u_int8_t
LCC_configuration_set(LCC_HANDLE *handle,
                      const char *option_str,
                      enum LCC_option option,
                      void *buffer);

u_int8_t
LCC_configuration_load_file(LCC_HANDLE *handle, const char **filenames, const char *section);
