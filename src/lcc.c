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

uint8_t lcc_initialized= 0;

/**
 * @brief: allocates and initializes a LCC handle
 * @param: handle  A pointer of a LCC_HANDLE * structure
 * @param: type    Type of handle
 * @param: base    For LCC_STATEMENT type the connection object, otherwise NULL.
 * @return LCC_ERRNO ER_OK on success, in case the initialozation failed an error code.
*/
LCC_ERRNO API_FUNC
LCC_init_handle(LCC_HANDLE **handle,  LCC_HANDLE_TYPE type, LCC_HANDLE *connection __attribute__((unused)))
{
  uint16_t rc;

  if (!handle)
    return ER_INVALID_POINTER;

  /* todo: check if system was initialized */

  switch(type) {
    case LCC_CONNECTION:
    {
      if (!(*handle= (LCC_HANDLE *)calloc(1, sizeof(lcc_connection))))
        return ER_OUT_OF_MEMORY;
      (*handle)->type= LCC_CONNECTION;
      if ((rc= lcc_io_init((lcc_connection *)*handle)))
      {
        free(handle);
        return rc;
      }
      break;
    }
    case LCC_RESULT:
    {
      if (!connection)
        return ER_INVALID_HANDLE;
      if (!((lcc_connection *)connection)->column_count)
        return ER_NO_RESULT_AVAILABLE;
      if (!(*handle= (LCC_HANDLE *)calloc(1, sizeof(lcc_result))))
        return ER_OUT_OF_MEMORY;
      (*handle)->type= LCC_RESULT;
      ((lcc_result *)(*handle))->conn= (lcc_connection *)connection;
      lcc_list_add(&((lcc_connection *)connection)->handles, *handle);
      return lcc_read_result_metadata((lcc_result *)*handle);
      break;
    }
    default:
      return ER_INVALID_HANDLE_TYPE;
  }
  return ER_OK;
}

static void lcc_invalidate_connection(LCC_HANDLE *handle)
{
  if (!handle)
    return;
  switch (handle->type) {
  case LCC_RESULT:
    ((lcc_result *)handle)->conn= NULL;
    break;
  default:
    break;
  }
  return;
}

static void lcc_free_connection_mem(lcc_connection *conn)
{
  if (!conn)
    return;

  lcc_configuration_close(conn);
  lcc_list_delete(conn->server.session_state, lcc_clear_session_state);
  free(conn->server.version);
  free(conn->server.info);
}

/**
 * @brief: closes the handle and releases memory
 * @param: handle   A handle which was previously allocated by LCC_init_handle
 * @return: LCC_ERRNO  ER_OK on success, otherwise error code.
 */
LCC_ERRNO API_FUNC
LCC_close_handle(LCC_HANDLE *handle)
{
  CHECK_HANDLE_TYPE(handle, LCC_CONNECTION);

  switch(handle->type) {
    case LCC_CONNECTION:
    {
      lcc_connection *conn= (lcc_connection *)handle;
      LCC_LIST *list= conn->handles;
      lcc_io_close(conn);
      lcc_free_connection_mem(conn);

      /* notify handles which rely on this connection */
      while (list) {
        lcc_invalidate_connection((LCC_HANDLE *)list->data);
        list= list->next;
      }
      lcc_list_delete(conn->handles, NULL);
    }
    break;
    case LCC_RESULT:
    {
      lcc_result *result = (lcc_result *)handle;
      if (result->memory.in_use)
        lcc_mem_close(&result->memory);
      free(result);
    }
    break;
    default:
      return ER_INVALID_HANDLE;
  }
  free(handle);
  return ER_OK;
}

/**
 * @brief:
 *
 *
 *
 */
LCC_ERRNO API_FUNC
LCC_execute(LCC_HANDLE *handle,
            const char *statement,
            size_t length)
{
  if (!handle)
    return ER_INVALID_HANDLE;

  if (!statement || !length)
    return ER_INVALID_VALUE;

  if ((ssize_t)length == -1)
    length= strlen(statement);

  /* Text protocol */
  if (handle->type == LCC_CONNECTION)
  {
    lcc_connection *conn= (lcc_connection *)handle;
    lcc_clear_error(&conn->error);
    return lcc_io_write(conn, CMD_QUERY, (char *)statement, length);
  }
  return ER_OK;
}

/**
 * @brief: returns error information
 * @param: handle  Pointer to a LCC handle
**/
LCC_ERROR * API_FUNC
LCC_get_error(LCC_HANDLE *handle)
{
  if (!handle)
    return NULL;

  switch (handle->type)
  {
    case LCC_CONNECTION:
      return &((lcc_connection *)handle)->error;
      break;
    default:
      return NULL;
  }
}

/**
 * brief:
 *
 */
LCC_ERRNO API_FUNC
LCC_get_info(LCC_HANDLE *handle, LCC_INFO info, void *buffer)
{
  switch(info) {
    case RESULT_INFO_ROW_COUNT:
      CHECK_HANDLE_TYPE(handle, LCC_RESULT);
      *((uint64_t *)buffer)= ((lcc_result *)handle)->row_count;
      break;
    case RESULT_INFO_ROW:
      CHECK_HANDLE_TYPE(handle, LCC_RESULT);
      *((LCC_STRING **)buffer)= ((lcc_result *)handle)->data;
      break;
    case RESULT_INFO_COLUMNS:
      CHECK_HANDLE_TYPE(handle, LCC_RESULT);
      *((LCC_COLUMN **)buffer)= ((lcc_result *)handle)->columns;
      break;
 
    default:
      return ER_INVALID_OPTION;
  }
  return ER_OK;
}

/**
 * @brief: returns session tracking information
 *
 * @param: handle  Connection handle
 * @param: last    pointer to a previously returned session tracking information or
                   NULL for retrieving first session trackinf recor.
 * @param: type    session state type (see LCC_SESSION_STATE_TYPE enumeration
 *
 * @return: a pointer to a LCC_SESSION_TRACK_INFO record or NULL if no (or no more)
 *          information is available.
 */
LCC_SESSION_TRACK_INFO * API_FUNC
LCC_get_session_track_info(LCC_HANDLE *hdl,
                           LCC_SESSION_TRACK_INFO *last,
                           LCC_SESSION_STATE_TYPE type)
{
  lcc_connection *conn;

  if (!lcc_valid_handle(hdl, LCC_CONNECTION))
    return NULL;

  conn= (lcc_connection *)hdl;

  if (!last)
    conn->server.current_session_state= conn->server.session_state;
  else
    conn->server.current_session_state= conn->server.current_session_state->next;

  while (conn->server.current_session_state)
  {
    LCC_SESSION_TRACK_INFO *info= (LCC_SESSION_TRACK_INFO *)conn->server.current_session_state->data;
    if (info->type == type)
      return info;
    conn->server.current_session_state= conn->server.current_session_state->next;
  }
  return NULL;
}

int main()
{
  LCC_HANDLE *conn, *result;
  LCC_ERRNO rc;
  LCC_COLUMN *columns;
  uint8_t eof= 0;
  int sock, ret;
  const char *filenames[]= {"/etc/my.cnf","/home/georg/.my.cnf", NULL};

  LCC_init_handle(&conn, LCC_CONNECTION, NULL);

  LCC_configuration_load_file(conn, filenames, NULL);

  ret= sock= create_inet_stream_socket("localhost", "3306", LIBSOCKET_IPv4, 0);

  if (ret < 0) {
      exit(1);
  }

  LCC_configuration_set(conn, NULL, LCC_OPT_SOCKET_NO, &sock);
  LCC_configuration_set(conn, NULL, LCC_OPT_AUTH_PLUGIN, (void *)"mysql_native_password");
  ((lcc_connection *)conn)->socket= sock;

  rc= lcc_read_server_hello((lcc_connection *)conn);
  printf("rc=%d\n", rc);

  rc= lcc_send_client_hello((lcc_connection *)conn);
  printf("rc=%d\n", rc);
  rc= lcc_read_response((lcc_connection *)conn);
  printf("rc=%d\n", rc);

  rc= LCC_execute(conn, "SELECT 1,2,'foo' UNION SELECT 2,3,'bar' UNION SELECT 3,4,'foobar'", -1);
  printf("rc=%d\n", rc);
  rc= lcc_read_response((lcc_connection *)conn);
  printf("rc=%d\n", rc);
  LCC_init_handle(&result, LCC_RESULT, conn);

  rc= LCC_get_info(result, RESULT_INFO_COLUMNS, &columns);
  printf("rc=%d\n", rc);

  while (!eof && rc == ER_OK) {
    rc= lcc_result_fetch_one((lcc_result *)result, &eof);
    if (!eof && !rc) {
      int i;
      for (i= 0; i < 3; i++)
        printf("%.*s ", (int)((lcc_result *)result)->data[i].len, ((lcc_result *)result)->data[i].str);
      printf("\n");
    }
  }
  printf("max_length: %d  length: %d \n", columns[1].max_column_size, columns[1].column_size);
  printf("num_rwos: %lu\n", ((lcc_result *)result)->row_count);

  LCC_close_handle(result);
  LCC_close_handle(conn);

  destroy_inet_socket(sock);
}
