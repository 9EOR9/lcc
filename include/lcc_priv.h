#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <lcc_error.h>
/* Helper macros */

#define lcc_MAX(v1,v2)\
({ __typeof__ (v1) _v1= v1;\
   __typeof__ (v2) _v2= v2;\
   (_v1 > _v2) ? _v1 : _v2;\
})

#define lcc_MIN(v1,v2)\
({ __typeof__ (v1) _v1= v1;\
   __typeof__ (v2) _v2= v2;\
   (_v1 < _v2) ? _v1 : _v2;\
})

#define SQLSTATE_LEN 5
#define SCRAMBLE_LEN 20

#define MIN_COM_BUFFER_SIZE 0x1000
#define COMM_CACHE_BUFFER_SIZE 16384
#define LCC_MEM_ALIGN_SIZE 2 * sizeof(void *)
#define LCC_FIELD_PTR(S, OFS, TYPE) ((TYPE *)((char*)(S) + (OFS)))

/* Protocol commands */
typedef enum
{
  CMD_SLEEP = 0,
  CMD_CLOSE,
  CMD_INIT_DB,
  CMD_QUERY,
  CMD_FIELD_LIST,
  CMD_CREATE_DB,
  CMD_DROP_DB,
  CMD_REFRESH,
  CMD_SHUTDOWN,
  CMD_STATISTICS,
  CMD_PROCESS_INFO,
  CMD_CONNECT,
  CMD_PROCESS_KILL,
  CMD_DEBUG,
  CMD_PING,
  CMD_TIME = 15,
  CMD_DELAYED_INSERT,
  CMD_CHANGE_USER,
  CMD_BINLOG_DUMP,
  CMD_TABLE_DUMP,
  CMD_CONNECT_OUT = 20,
  CMD_REGISTER_SLAVE,
  CMD_STMT_PREPARE = 22,
  CMD_STMT_EXECUTE = 23,
  CMD_STMT_SEND_LONG_DATA = 24,
  CMD_STMT_CLOSE = 25,
  CMD_STMT_RESET = 26,
  CMD_SET_OPTION = 27,
  CMD_STMT_FETCH = 28,
  CMD_DAEMON= 29,
  CMD_UNSUPPORTED= 30,
  CMD_RESET_CONNECTION = 31,
  CMD_STMT_BULK_EXECUTE = 250,
  CMD_RESERVED_1 = 254, /* former COM_MULTI, now removed */
  CMD_NONE
} lcc_io_cmd;

typedef enum {
  CONN_STATUS_READY=0,
  CONN_STATUS_RESULT,
  CONN_STATUS_FETCH
} lcc_conn_status;

extern char** lcc_configuration_dirs;

typedef struct st_memory_block {
  void *buffer;
  size_t total_size;
  size_t used_size;
  struct st_memory_block *next;
} lcc_mem_block;

typedef struct {
  size_t prealloc_size;
  uint8_t in_use;
  lcc_mem_block *block;
} lcc_mem;

typedef struct {
  const char *key;
  const char *value;
} lcc_key_val;

typedef struct {
  /* server status callback */
  void (*status_change)(LCC_HANDLE *handle, uint32_t status);
  uint32_t status_flags;
  void (*report_progress)(LCC_HANDLE *handle, uint8_t stage, uint8_t max_stage,
                         double progress, char *info, size_t length);
} lcc_callbacks;

/**
 * @brief: callback function for parameters:
 *
 * @param: data - Parameter data, previously registered by 
                  LCC_stmt_set_parameter function
 * @param: buffer - contains row data in binary format
 * @param: buffer_len - length of row data
 * @param: row_nr - number of row
 */
typedef int (*stmt_param_callback)(void *data, LCC_BIND *bind, uint32_t row_nr);

typedef enum {
  STMT_STATUS_INIT= 0,
  STMT_STATUS_PREPARE_DONE= 1,
  STMT_STATUS_EXECUTE_DONE= 2,
  STMT_STATUS_EXECUTEDIRECT_DONE= 3,
  STMT_STATUS_HAVE_RESULT= 4
} lcc_stmt_status;

typedef struct {
  uint64_t affected_rows;
  uint64_t last_insert_id;
  uint32_t status;
  uint32_t warning_count;
  uint32_t server_version;
  uint32_t field_count;
  char *current_db;
  char *info;
  uint16_t port;
  char *user;
  uint8_t protocol;
  char *version;
  uint8_t is_mariadb;
  uint32_t capabilities;
  uint32_t mariadb_capabilities;
  LCC_LIST *session_state;
  LCC_LIST *current_session_state;
} lcc_server;

typedef struct {
  char scramble[21];
  char *plugin;
  uint8_t scramble_len;
} lcc_scramble;

typedef struct {
  char *tls_key;
  char *tls_cert;
  char *tls_ca;
  char *tls_capath;
  char *tls_crl;
  char *tls_crlpath;
  uint8_t tls;
  char *user;
  char *password;
  uint16_t port;
  char *database;
} lcc_client_options;

typedef struct {
  char *key;
  char *value;
} lcc_connect_attr;

typedef struct {
  uint32_t capabilities;
  uint32_t mariadb_capabilities;
  uint32_t thread_id;
} lcc_client;

typedef struct {
  char *auth_plugin;
  char *current_db;
  uint8_t remember;
  char *tls_ca;
  char *tls_ca_path;
  char *tls_cert;
  char *tls_cipher;
  char *tls_crl;
  char *tls_crl_path;
  char *tls_key;
  char *user;
  char *password;
  uint8_t tls_verify_peer;
  int read_timeout;
  int write_timeout;
  lcc_connect_attr *conn_attr;
  lcc_callbacks callbacks;
} lcc_configuration;

typedef struct {
  char *readbuf;
  char *writebuf;
  char *read_pos;
  char *read_end;
  size_t read_size;
  size_t write_size;
  uint8_t read_pkt;
  char *write_pos;
} lcc_io;

typedef struct {
  LCC_HANDLE_TYPE type;
  int socket;
  lcc_conn_status status;
  LCC_ERROR error;
  lcc_server server;
  lcc_client client;
  lcc_scramble scramble;
  lcc_client_options options;
  lcc_configuration configuration;
  lcc_io io;
  uint32_t column_count;
  LCC_LIST *handles;  /* list of handles which depend on connection */
} lcc_connection;

typedef struct {
  LCC_HANDLE_TYPE type;
  /* internal */
  lcc_connection *conn;
  lcc_mem     memory;
  LCC_COLUMN  *columns;
  LCC_STRING  *data;
  uint64_t    row_count;
} lcc_result;

typedef struct {
  LCC_HANDLE_TYPE type;
  /* internal */
  lcc_connection *conn;
  lcc_stmt_status status;
  LCC_ERROR      error;
  lcc_mem        memory;
  lcc_result     *result;
  LCC_BIND       *params;
  stmt_param_callback param_callback;
  void           *callback_data;
  uint32_t       id;
  uint16_t       column_count;
  uint16_t       param_count;
  LCC_BUFFER     execbuf;
  /* execbuf.len has aligned size, so we need to store the
     exact length for io.write() */
  size_t         exec_len;
} lcc_stmt;

typedef struct {
  size_t display_len;
  size_t store_len;
  /* todo:
  store and fetch functions */
} lcc_bin_type;

/* type of configuration option */
enum enum_configuration_type {
  LCC_CONF_STR= 0,
  LCC_CONF_INT8,
  LCC_CONF_INT32,
  LCC_CONF_INT64,
  LCC_CONF_FLAG,
  LCC_KEY_VALUE,
  LCC_CONF_UNKNOWN
};

typedef struct {
  LCC_OPTION option;
  size_t offset;
  enum enum_configuration_type type;
  const char **keys;
} lcc_configuration_options;

static inline void lcc_clear_session_state(void *data)
{
  LCC_SESSION_TRACK_INFO *info= (LCC_SESSION_TRACK_INFO*)data;
  free((char *)info->str.str);
  free(info);
}

static inline size_t lcc_align_size(size_t align_size, size_t size) {
  return (size + (align_size - 1)) & ~(align_size - 1);
}

static inline uint8_t lcc_validate_handle(LCC_HANDLE *handle, LCC_HANDLE_TYPE type)
{
  return (handle && handle->type == type) ? ER_OK : ER_INVALID_HANDLE;
}


LCC_ERRNO 
lcc_auth(lcc_connection *conn,
         const char *password,
         u_char *buffer,
         size_t *buflen);

LCC_ERRNO
lcc_io_init(lcc_connection *conn);

void
lcc_io_close(lcc_connection *conn);

LCC_ERRNO
lcc_io_write(lcc_connection *conn, lcc_io_cmd command, char *buffer, size_t len);

void 
lcc_io_close(lcc_connection *conn);

LCC_ERRNO
lcc_io_read(lcc_connection *conn, size_t *pkt_len);

uint32_t
lcc_buffered_error_packet(lcc_connection *conn);

void 
lcc_configuration_close(lcc_connection *conn);

LCC_ERRNO
lcc_send_client_hello(lcc_connection *conn);

LCC_ERRNO
lcc_read_server_hello(lcc_connection *conn);

LCC_ERRNO
lcc_read_response(lcc_connection *conn);

LCC_ERRNO
lcc_read_result_metadata(lcc_result *result);

LCC_ERRNO
lcc_result_fetch_one(lcc_result *result, uint8_t *eof);

LCC_ERRNO
lcc_read_prepare_response(lcc_stmt *stmt);

typedef void (*lcc_delete_callback)(void *);
typedef uint8_t (*lcc_find_callback)(void *data, void *search);

LCC_ERRNO
lcc_list_add(LCC_LIST **list, void *data);
void lcc_list_clear_element(LCC_LIST *list, void *search);

LCC_LIST *
lcc_list_find(LCC_LIST *list, void *search, lcc_find_callback func);

void
lcc_list_delete(LCC_LIST *list, lcc_delete_callback func);

void lcc_mem_close(lcc_mem *mem);
void *lcc_mem_alloc(lcc_mem *mem,
                    size_t size);
LCC_ERRNO
lcc_mem_init(lcc_mem *mem, size_t prealloc);

void lcc_mem_reset(lcc_mem *mem);

void lcc_stmt_init_bin_types();
