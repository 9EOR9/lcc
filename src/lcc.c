#include <lcc.h>
#include <lcc_pack.h>
#include <lcc_priv.h>
#include <lcc_error.h>
#include <stdio.h>
#include <stdlib.h>
#include <alloca.h>
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <libsocket/libunixsocket.h>


u_int32_t
LCC_init_handle(LCC_HANDLE **handle, enum LCC_handle_type type, void *base)
{
  switch(type) {
    case LCC_CONNECTION:
    {
      if (!(*handle= (lcc_handle *)calloc(1, sizeof(lcc_connection))))
        return ER_OUT_OF_MEMORY;
      (*handle)->type= LCC_CONNECTION;
      break;
    }
    default:
      return ER_INVALID_HANDLE_TYPE;
  }
  return ER_OK;
}

void LCC_close_handle(LCC_HANDLE *handle)
{
  switch(handle->type) {
    case LCC_CONNECTION:
      lcc_configuration_close((lcc_connection *)handle);
      break;
  }
}

u_int32_t LCC_get_info(LCC_HANDLE *handle, enum LCC_get_server_info info, void *buffer)
{
  if (((lcc_handle *)(handle))->type != LCC_CONNECTION)
    return ER_INVALID_HANDLE_TYPE;

  switch(info) {
    default:
      return ER_INVALID_OPTION;
  }

  return 0;
}

extern u_int16_t lcc_read_server_hello(lcc_connection *conn, lcc_scramble *scramble);

int main()
{
  LCC_HANDLE *conn;
  char buf[2000];
  ssize_t x=0;
  u_int32_t pkt_len= 0;
  char *p= buf;
  u_int8_t protocol= 0;
  lcc_scramble scramble;
  const char *filenames[]= {"/etc/my.cnf", NULL};

  memset(&scramble, 0, sizeof(lcc_scramble));

  LCC_init_handle(&conn, LCC_CONNECTION, NULL);

  LCC_configuration_load_file(conn, filenames, NULL);

  int sock= create_unix_stream_socket("/tmp/mysql.sock", 0);
  LCC_configuration_set(conn, NULL, LCC_SOCKET_NO, &sock);
  LCC_configuration_set(conn, NULL, LCC_AUTH_PLUGIN, (void *)"mysql_native_password");
  ((lcc_connection *)conn)->socket= sock;

  lcc_read_server_hello((lcc_connection *)conn, &scramble);

  destroy_unix_socket(sock);


}
