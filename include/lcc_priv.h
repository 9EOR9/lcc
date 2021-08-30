#pragma once

#include <stdio.h>

#define LCC_MAX_ERROR_LEN 255UL
#define SQLSTATE_LEN 5

extern char** lcc_configuration_dirs;

typedef struct {
  char sqlstate[6];
  u_int16_t error_number;
  char error[LCC_MAX_ERROR_LEN];
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
} lcc_server;

typedef struct {
  char scramble[20];
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
  u_int8_t tls_verify_peer;
} lcc_configuration;

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
  char *net_buffer;
  char *net_pos;
} lcc_connection;

/* type of configuration option */
enum enum_configuration_type {
  LCC_CONF_STR= 0,
  LCC_CONF_INT8,
  LCC_CONF_INT32,
  LCC_CONF_INT64,
  LCC_CONF_FLAG,
  LCC_CONF_UNKNOWN
};

typedef struct {
  enum LCC_option option;
  size_t offset;
  enum enum_configuration_type type;
  const char **keys;
} lcc_configuration_options;

u_int16_t 
lcc_auth(lcc_connection *conn,
         const char *password,
         unsigned char *buffer,
         size_t buflen);


void 
lcc_configuration_close(lcc_connection *conn);
