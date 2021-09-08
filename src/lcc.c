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
#include <sys/types.h>
#include <libsocket/libinetsocket.h>

u_int8_t lcc_initialized= 0;

/**
 * @brief: allocates and initializes a LCC handle
 * @param: handle  A pointer of a LCC_HANDLE * structure
 * @param: type    Type of handle
 * @param: base    For LCC_STATEMENT type the connection object, otherwise NULL.
 * @return LCC_ERROR ER_OK on success, in case the initialozation failed an error code.
*/
LCC_ERROR API_FUNC
LCC_init_handle(LCC_HANDLE **handle, enum LCC_handle_type type, void *connection __attribute__((unused)))
{
  u_int16_t rc;

  if (!handle)
    return ER_INVALID_POINTER;

  /* todo: check if system was initialized */

  switch(type) {
    case LCC_CONNECTION:
    {
      if (!(*handle= (lcc_handle *)calloc(1, sizeof(lcc_connection))))
        return ER_OUT_OF_MEMORY;
      (*handle)->type= LCC_CONNECTION;
      if ((rc= lcc_comm_init((lcc_connection *)*handle)))
      {
        free(handle);
        return rc;
      }
      break;
    }
    default:
      return ER_INVALID_HANDLE_TYPE;
  }
  return ER_OK;
}

/**
 * @brief: closes the handle and releases memory
 * @param: handle   A handle which was previously allocated by LCC_init_handle
 * @return: LCC_ERROR  ER_OK on success, otherwise error code.
*/
LCC_ERROR API_FUNC
LCC_close_handle(LCC_HANDLE *handle)
{
  CHECK_HANDLE_TYPE(handle, LCC_CONNECTION);

  switch(handle->type) {
    case LCC_CONNECTION:
      lcc_comm_close((lcc_connection *)handle);
      lcc_configuration_close((lcc_connection *)handle);
      break;
    default:
      return ER_INVALID_HANDLE;
  }
  return ER_OK;
}

LCC_ERROR API_FUNC
LCC_get_info(LCC_HANDLE *handle, enum LCC_get_server_info info, void *buffer)
{
  CHECK_HANDLE_TYPE(handle, LCC_CONNECTION);
  buffer= NULL;

  switch(info) {
    default:
      return ER_INVALID_OPTION;
  }

  return 0;
}

int main()
{
  LCC_HANDLE *conn;
  LCC_ERROR rc;
  int sock, ret;
  size_t pkt_len;
  const char *filenames[]= {"/etc/my.cnf","/home/georg/.my.cnf", NULL};

  LCC_init_handle(&conn, LCC_CONNECTION, NULL);

  LCC_configuration_load_file(conn, filenames, NULL);

  ret= sock= create_inet_stream_socket("localhost", "3306", LIBSOCKET_IPv4, 0);

  if (ret < 0) {
      exit(1);
  }

  LCC_configuration_set(conn, NULL, LCC_SOCKET_NO, &sock);
  LCC_configuration_set(conn, NULL, LCC_AUTH_PLUGIN, (void *)"mysql_native_password");
  ((lcc_connection *)conn)->socket= sock;

  rc= lcc_read_server_hello((lcc_connection *)conn);
  rc= lcc_send_client_hello((lcc_connection *)conn);
  rc= lcc_comm_read((lcc_connection *)conn, &pkt_len);
  printf("rc=%ld\n", pkt_len);

  destroy_inet_socket(sock);
}
