#pragma once

#include <stdio.h>

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

#define LCC_MAX_ERROR_LEN 255UL
#define SQLSTATE_LEN 5
#define SCRAMBLE_LEN 20

#define MIN_COM_BUFFER_SIZE 0x1000
extern char** lcc_configuration_dirs;

typedef struct {
  const char *file;
  const char *func;
  u_int32_t lineno;
} lcc_error_info;

typedef struct {
  const char *key;
  const char *value;
} lcc_key_val;

typedef struct {
  char sqlstate[6];
  u_int16_t error_number;
  char error[LCC_MAX_ERROR_LEN];
  lcc_error_info info;
} lcc_error;

typedef struct {
  /* socket handle */
  int fd;
  /* tls_context, obtained by tls_handshake callback */
  void *tls; 
  ssize_t (*read)(int handle, char *buffer, size_t len);
  /* read encrypted data from server */
  ssize_t (*tls_read)(void *tls, char *buffer, size_t len);
  /* send data to server */
  ssize_t (*write)(int handle, char *buffer, size_t len);
  /* send encrypted data to server */
  ssize_t (*tls_write)(void *tls, char *buffer, size_t len);
  /* perform handshake */
  void *  (*tls_handshake)(void *data);
  /* verification after handshake */
  u_int8_t (*tls_verify)(void *tls);
} lcc_callbacks;

typedef struct {
  u_int64_t affected_rows;
  u_int64_t last_insert_id;
  u_int32_t status;
  u_int32_t warning_count;
  u_int32_t server_version;
  u_int32_t field_count;
  char *current_db;
  u_int16_t port;
  char *user;
  u_int8_t protocol;
  char *version;
  u_int8_t is_mariadb;
  u_int32_t capabilities;
  u_int32_t mariadb_capabilities;
  LCC_LIST *session_state;
} lcc_server;

typedef struct {
  char scramble[21];
  char *plugin;
  u_int8_t scramble_len;
} lcc_scramble;

typedef struct {
  char *tls_key;
  char *tls_cert;
  char *tls_ca;
  char *tls_capath;
  char *tls_crl;
  char *tls_crlpath;
  u_int8_t tls;
  char *user;
  char *password;
  u_int16_t port;
  char *database;
} lcc_client_options;

typedef struct {
  char *key;
  char *value;
} lcc_connect_attr;

typedef struct {
  u_int32_t capabilities;
  u_int32_t mariadb_capabilities;
  u_int32_t thread_id;
} lcc_client;

typedef struct {
  char *auth_plugin;
  char *current_db;
  u_int8_t remember;
  char *tls_ca;
  char *tls_ca_path;
  char *tls_cert;
  char *tls_cipher;
  char *tls_crl;
  char *tls_crl_path;
  char *tls_key;
  char *user;
  char *password;
  u_int8_t tls_verify_peer;
  int read_timeout;
  int write_timeout;
  lcc_connect_attr *conn_attr;
} lcc_configuration;

typedef struct {
  char *readbuf;
  char *writebuf;
  char *read_pos;
  char *read_end;
  char *cached;
  size_t read_size;
  size_t write_size;
  u_int8_t read_pkt;
  char *write_pos;
} lcc_communication;

typedef struct {
  u_int32_t type;
  int socket;
  lcc_error error;
  lcc_callbacks callbacks;
  lcc_server server;
  lcc_client client;
  lcc_scramble scramble;
  lcc_client_options options;
  lcc_configuration configuration;
  lcc_communication comm;
} lcc_connection;

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
  enum LCC_option option;
  size_t offset;
  enum enum_configuration_type type;
  const char **keys;
} lcc_configuration_options;

LCC_ERROR 
lcc_auth(lcc_connection *conn,
         const char *password,
         u_char *buffer,
         size_t *buflen);

LCC_ERROR
lcc_comm_init(lcc_connection *conn);

void
lcc_comm_close(lcc_connection *conn);

LCC_ERROR
lcc_comm_write(lcc_connection *conn, u_char command, char *buffer, size_t len);

LCC_ERROR
lcc_comm_read(lcc_connection *conn, size_t *pkt_len);

void 
lcc_configuration_close(lcc_connection *conn);

LCC_ERROR
lcc_send_client_hello(lcc_connection *conn);

LCC_ERROR
lcc_read_server_hello(lcc_connection *conn);

typedef void (*lcc_delete_callback)(void *);
typedef u_int8_t (*lcc_find_callback)(void *data, void *search);

LCC_ERROR
lcc_list_add(LCC_LIST **list, void *data);

LCC_LIST *
lcc_list_find(LCC_LIST *list, void *search, lcc_find_callback func);

void
lcc_list_delete(LCC_LIST *list, lcc_delete_callback func);


