#include <lcc.h>
#include <lcc_pack.h>
#include <lcc_priv.h>
#include <lcc_error.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define MARIADB_RPL_HACK "5.5.5-"
#define packet_error (u_int32_t)-1

/* utf8mb4 character set number is 45 */
#define UTF8MB4 45

extern lcc_key_val default_conn_attr[];

/* default values, they can be overwritten via options */
u_int32_t max_packet_size= 0x40000000;
u_int32_t comm_buffer_length= 0x2000;

static inline LCC_ERROR
err_malformed_packet(lcc_error *error, size_t offset)
{
  lcc_set_error(error, LCC_ERROR_INFO, ER_MALFORMED_PACKET, "HY000", NULL, offset);
  return ER_MALFORMED_PACKET;  
}

/* read error packet:
   u_int8_t   errorpacket (0xFF)
   u_int16_t  error number
   if errornumber == 0xFFFF
     progressindication (requires registration of a callback function):
     u_int8_t  min_stage
     u_int8_t  max_stage
     u_int24_t progress
     lencstr   progress_info
   else
     char       sqlstate marker '#'
     string[5]  sqlstate
     str <eop>  error string (until end of packet)
 */
static u_int16_t
lcc_read_server_error_packet(char *buffer, size_t buffer_length, lcc_error *error)
{
  char *p= buffer, 
         *end= buffer + buffer_length;

  if ((end - p) < 2)
    return ER_MALFORMED_PACKET;
  error->error_number= p_to_ui16(p);
  p+= 2;
  memset(error, 0, sizeof(lcc_error));
  if (error->error_number != 0xFFFF)
  {
    if ((end - p) < 6)
      return ER_MALFORMED_PACKET;
    if (*p != '#')
      return ER_MALFORMED_PACKET;
    p++;
    memcpy(error->sqlstate, p, 5);
    p+=5;
    memcpy(error->error, p, end - p);
  }
  return error->error_number;
}

LCC_ERROR lcc_send_client_hello(lcc_connection *conn)
{
  u_char buffer[LCC_NET_BUFFER_SIZE];
  u_char *p= buffer;
  u_char *end= buffer + LCC_NET_BUFFER_SIZE - 4;
  u_int32_t client_flags= (u_int32_t)CLIENT_CAPS;
  LCC_ERROR rc;
  size_t attr_len= 0;
  u_int32_t i;

  memset(p, 0, LCC_NET_BUFFER_SIZE);

  /* client capabilities */
  if (conn->options.tls)
    client_flags|= CAP_TLS;

  if (conn->configuration.current_db)
    client_flags|= CAP_CONNECT_WITH_DB;
  ui32_to_p(p, client_flags);
  p+= 4;

  /* maximum packet size */
  ui32_to_p(p, max_packet_size);
  p+= 4;

  /* default character set */
  ui8_to_p(p, UTF8MB4);
  p++;

  /* filler: not in use (19 bytes) */
  memset(p, 0, 19);
  p+= 19;

  /* if we talk to MariaDB server, we need
     to send mariadb specific capabilities */
  if (!(conn->server.capabilities & CAP_MYSQL))
    ui32_to_p(p, (u_int32_t)(MARIADB_CAPS >> 32));
  else
    ui32_to_p(p, 0);
  p+= 4;
 
  /* Todo: if TLS is active we stop here,
   * and perform TLS handshake
   */

  /* user: zero terminated string */
  if (conn->configuration.user && conn->configuration.user[0])
  {
    strncat((char *)p, conn->configuration.user, LCC_MAX_USER_LEN * 4);
    p += strlen(conn->configuration.user);
  } 
  *p++= 0;

  /* no password provided -> length = 0 */
  if (!conn->configuration.password)
  {
    *p++= 0;
  } else
  {
    size_t auth_len= end - p;
    u_char buf[SCRAMBLE_LEN];

    if (!(conn->server.capabilities & CAP_PLUGIN_AUTH_LENENC_CLIENT_DATA))
      return ER_UNSUPPORTED_SERVER_VERSION;

    if ((rc= lcc_auth(conn, conn->configuration.password, buf, &auth_len)))
      return rc;

    p= lenc_to_p(p, auth_len);
    memcpy(p, buf, SCRAMBLE_LEN);
    p+= auth_len;
  }

  /* zero terminated database name */
  if (conn->configuration.current_db) {
    memcpy(p, conn->configuration.current_db, strlen(conn->configuration.current_db));
    p+= strlen(conn->configuration.current_db);
  }
  *p++= '\0';

  /* zero terminated authentication plugin name */
  memcpy(p, conn->scramble.plugin, strlen(conn->scramble.plugin));
  p+= strlen(conn->scramble.plugin);
  *p++= 0;

  /* connection attributes */
  for (i=0; default_conn_attr[i].key; i++)
  {
    attr_len+= strlen(default_conn_attr[i].key) +
               strlen(default_conn_attr[i].value) +
               lenc_length(strlen(default_conn_attr[i].key)) +
               lenc_length(strlen(default_conn_attr[i].value));
  }

  /* boundary check */
  if (p + attr_len > end)
  {
    return ER_OUT_OF_MEMORY;
  }

  p= lenc_to_p(p, (u_int64_t)attr_len);
  for (i=0; default_conn_attr[i].key; i++)
  {
     p= lenc_to_p((u_char *)p, (u_int64_t)strlen(default_conn_attr[i].key));
     strcpy((char *)p, default_conn_attr[i].key);
     p+= strlen(default_conn_attr[i].key);
     p= lenc_to_p((u_char *)p, (u_int64_t)strlen(default_conn_attr[i].value));
     strcpy((char *)p, default_conn_attr[i].value);
     p+= strlen(default_conn_attr[i].value);
  }

  rc= lcc_comm_write(conn, 0, (char *)buffer, p-buffer);
  return rc;
}

LCC_ERROR
lcc_read_server_hello(lcc_connection *conn)
{
  size_t pkt_len;
  char *start, *pos, *end;
  LCC_ERROR rc;

  CHECK_HANDLE_TYPE(conn, LCC_CONNECTION);

  /* Description of server hello packet

     u_int8_t       protocol version (=10)
                    or error packet (=0xFF)
     str + '\0'     server version, for MariaDB
                    it is prefixed with 5.5.5-
     u_int32_t      thread/connection id
     string[8]      1st part of scramble
     string[1]      unused
     u_int16_t      server capabilities 1
     u_int8_t       server collation
     u_int16_t      server status flags
     u_int16_t      server capabilities 2
  */

  /* Boundary checking:
   * read() function already checkd that the number of bytes
   * read is not smaller than reported packet length.
   * That means for boundary checking we need to make sure that
   * we will always check to not exceed end_ptr limit.
   */

  rc= lcc_comm_read(conn, &pkt_len);
  if (rc)
    return rc;

  if (pkt_len == packet_error ||
      pkt_len == 0)
    return ER_COMM_READ;

  start= pos= conn->comm.readbuf;
  end= pos + pkt_len;

  /*
   * In very rare cases we can get an error packet
   * instead of server_hello packet, e.g. if number
   * of failing connection attempts reaches
   * max_connect_errors
   */

  if ((u_char)*pos == 0xFF)
  {
    pos++;
    return lcc_read_server_error_packet(pos, end - pos, &conn->error);
  }

  /* Protocol: 1 byte */
  conn->server.protocol= p_to_ui8(pos);

  if (pos < end)
    pos++;
  else
    return err_malformed_packet(&conn->error, pos - start);

  /* Server version */
  if (end - pos <= 7)
    return err_malformed_packet(&conn->error, pos - start);

  /* If we are connected to a MariaDB server,
   * the version will be prefixed with so called
   * "rpl_hack" ('=5.5.5-'). We will remove this
   * prefix and remember that we are connected to
   * a mariadb server 
   */
  
  if (!memcmp(pos, MARIADB_RPL_HACK, 6))
  {
    pos+= 6;
    conn->server.is_mariadb= 1;
  }

  if (!memchr(pos, '\0', end - pos))
    return err_malformed_packet(&conn->error, pos - start);

  conn->server.version= strdup(pos);
  pos+= (strlen(conn->server.version) + 1);

  /* boundary checkpoint */
  if (end - pos < 33)
    return err_malformed_packet(&conn->error, pos - start);

  /* connection id */
  conn->client.thread_id= p_to_ui32(pos);
  pos+= 4;

  /* 1st part of scramble */
  memcpy(conn->scramble.scramble, pos, 8);
  pos+= 8;

  /* unused byte */
  pos++;

  /* server capabilities 1st part */
  conn->server.capabilities= p_to_ui16(pos);
  pos+= 2;

  /* server collation, not used by client */
  pos++;

  /* server status */
  conn->server.status= p_to_ui16(pos);
  pos+= 2;

  /* server capabilities 2nd part */
  conn->server.capabilities|= p_to_ui16(pos) << 16;
  pos+= 2;

  /* scramble length */
  conn->scramble.scramble_len= p_to_ui8(pos);
  pos++;

  /* unused */
  pos+= 6;

  /* boundary checkpoint */
  if (end - pos < 4)
    return err_malformed_packet(&conn->error, pos - start);

  if (!(conn->server.capabilities & CAP_MYSQL))
    conn->server.mariadb_capabilities= p_to_ui32(pos);
  pos+= 4;

  /* boundary checkpoint */
  if (end - pos < 13)
    return err_malformed_packet(&conn->error, pos - start);

  if (conn->server.capabilities & CAP_SECURE_CONNECTION)
  {
    memcpy(&conn->scramble.scramble[8], pos, lcc_MAX(12, conn->scramble.scramble_len - 9));
    conn->scramble.scramble[conn->scramble.scramble_len]= 0;
    pos+= lcc_MAX(12, conn->scramble.scramble_len - 9);
    pos++; /* reserved byte */
  }

  if (conn->server.capabilities & CAP_PLUGIN_AUTH)
  {
    if (conn->scramble.plugin)
      free(conn->scramble.plugin);
    /* boundary checkpoint */
    if (!memchr(pos, 0, end - pos))
      return ER_MALFORMED_PACKET;
    conn->scramble.plugin= strdup(pos);
  }
  
  return ER_OK;
}

static void lcc_clear_session_state(void *data)
{
  LCC_SESSION_TRACK_INFO *info= (LCC_SESSION_TRACK_INFO*)data;
  free((char *)info->str.str);
  free(info);
}

LCC_ERROR lcc_read_response(lcc_connection *conn)
{
  size_t pkt_len;
  char *pos, *end;
  LCC_ERROR rc;
  u_int8_t error;

  rc= lcc_comm_read(conn, &pkt_len);
  if (rc)
    return rc;
  pos= (char *)conn->comm.readbuf;
  end= pos + pkt_len;
  
  /* Error packet */
  if ((u_char)*pos == 0xFF)
    return lcc_read_server_error_packet(pos, end - pos, &conn->error);

  /* OK packet */
  if (*pos == 0x00)
  {
    conn->server.affected_rows= p_to_lenc((u_char **)&pos, (u_char *)end, &error);
    if (error)
      return err_malformed_packet(&conn->error, pos - conn->comm.readbuf);
    conn->server.last_insert_id= p_to_lenc((u_char **)&pos, (u_char *)end, &error);
    if (error)
      return err_malformed_packet(&conn->error, pos - conn->comm.readbuf);
    if (end -pos < 4)
      return err_malformed_packet(&conn->error, pos - conn->comm.readbuf);

    conn->server.status= p_to_ui32(pos);
    pos+= 2;
    conn->server.warning_count= p_to_ui32(pos);
    pos+= 2;

    if (pos < end &&
        conn->server.capabilities & CAP_SESSION_TRACKING)
    {
      lcc_list_delete(conn->server.session_state, lcc_clear_session_state);
      if (conn->server.status & LCC_STATUS_SESSION_STATE_CHANGED)
      {
        char *start_pos;
        size_t sess_len= p_to_lenc((u_char **)&pos, (u_char *)end, &error);

        if (error)
          return err_malformed_packet(&conn->error, pos - conn->comm.readbuf);

        /* boundary check */
        if (pos + sess_len > end)
          return err_malformed_packet(&conn->error, pos - conn->comm.readbuf);

        start_pos= pos;

        while ((size_t)(pos - start_pos) + 2 < sess_len)
        {
          LCC_SESSION_TRACK_INFO *info;

          if (!(info= (LCC_SESSION_TRACK_INFO *)calloc(1, sizeof(LCC_SESSION_TRACK_INFO))))
            return ER_OUT_OF_MEMORY;

          info->type= p_to_ui8(pos);
          pos++;

          info->str.len= p_to_lenc((u_char **)&pos, (u_char *)end, &error);
          if (error)
            return err_malformed_packet(&conn->error, pos - conn->comm.readbuf);
          if ((info->str.str= (char *)malloc(info->str.len)))
            return ER_OUT_OF_MEMORY;
          memcpy(info->str.str, pos, info->str.len);
          pos+= info->str.len;
        }
      }
    }
  }
  return ER_OK;
}
