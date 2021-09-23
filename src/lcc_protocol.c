#include <lcc.h>
#include <lcc_pack.h>
#include <lcc_priv.h>
#include <lcc_error.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define MARIADB_RPL_HACK "5.5.5-"
#define packet_error (uint32_t)-1

/* utf8mb4 character set number is 45 */
#define UTF8MB4 45

extern lcc_key_val default_conn_attr[];

/* default values, they can be overwritten via options */
uint32_t max_packet_size= 0x40000000;
uint32_t comm_buffer_length= 0x2000;

static inline LCC_ERRNO
err_malformed_packet(LCC_ERROR *error, size_t offset)
{
  lcc_set_error(error, LCC_ERROR_INFO, ER_MALFORMED_PACKET, "HY000", NULL, offset);
  return ER_MALFORMED_PACKET;  
}

/* read error packet:
   uint8_t   errorpacket (0xFF)
   uint16_t  error number
   if errornumber == 0xFFFF
     progressindication (requires registration of a callback function):
     uint8_t  min_stage
     uint8_t  max_stage
     uint24_t progress
     lencstr   progress_info
   else
     char       sqlstate marker '#'
     string[5]  sqlstate
     str <eop>  error string (until end of packet)
 */
static uint16_t
lcc_read_server_error_packet(char *buffer, size_t buffer_length, LCC_ERROR *error)
{
  char *p= buffer, 
         *end= buffer + buffer_length;

  memset(error, 0, sizeof(LCC_ERROR));
  if ((end - p) < 2)
    return ER_MALFORMED_PACKET;
  error->error_number= p_to_ui16(p);

  p+= 2;
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

LCC_ERRNO lcc_send_client_hello(lcc_connection *conn)
{
  u_char buffer[LCC_NET_BUFFER_SIZE];
  u_char *p= buffer;
  u_char *end= buffer + LCC_NET_BUFFER_SIZE - 4;
  uint32_t client_flags= (uint32_t)CLIENT_CAPS;
  LCC_ERRNO rc;
  size_t attr_len= 0;
  uint32_t i;

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
    ui32_to_p(p, (uint32_t)(MARIADB_CAPS >> 32));
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

  p= lenc_to_p(p, (uint64_t)attr_len);
  for (i=0; default_conn_attr[i].key; i++)
  {
     p= lenc_to_p((u_char *)p, (uint64_t)strlen(default_conn_attr[i].key));
     strcpy((char *)p, default_conn_attr[i].key);
     p+= strlen(default_conn_attr[i].key);
     p= lenc_to_p((u_char *)p, (uint64_t)strlen(default_conn_attr[i].value));
     strcpy((char *)p, default_conn_attr[i].value);
     p+= strlen(default_conn_attr[i].value);
  }

  rc= lcc_io_write(conn, CMD_NONE, (char *)buffer, p-buffer);
  return rc;
}

LCC_ERRNO
lcc_read_server_hello(lcc_connection *conn)
{
  size_t pkt_len;
  char *start, *pos, *end;
  LCC_ERRNO rc;

  CHECK_HANDLE_TYPE(conn, LCC_CONNECTION);

  /* Description of server hello packet

     uint8_t       protocol version (=10)
                    or error packet (=0xFF)
     str + '\0'     server version, for MariaDB
                    it is prefixed with 5.5.5-
     uint32_t      thread/connection id
     string[8]      1st part of scramble
     string[1]      unused
     uint16_t      server capabilities 1
     uint8_t       server collation
     uint16_t      server status flags
     uint16_t      server capabilities 2
  */

  /* Boundary checking:
   * read() function already checkd that the number of bytes
   * read is not smaller than reported packet length.
   * That means for boundary checking we need to make sure that
   * we will always check to not exceed end_ptr limit.
   */

  rc= lcc_io_read(conn, &pkt_len);
  if (rc)
    return rc;

  if (pkt_len == packet_error ||
      pkt_len == 0)
    return ER_COMM_READ;

  start= pos= conn->io.read_pos;
  end= pos + pkt_len;
  conn->io.read_pos= end;

  /*
   * In very rare cases we can get an error packet
   * instead of server_hello packet, e.g. if number
   * of failing connection attempts reaches
   * max_connect_errors
   */

  if ((u_char)*pos == 0xFF)
  {
    return lcc_read_server_error_packet(pos, end - pos, &conn->error);
  }

  /* Protocol: 1 byte */
  conn->server.protocol= p_to_ui8(pos);

  if (pos < end)
    pos++;
  else
    goto malformed_packet;

  /* Server version */
  if (end - pos <= 7)
    goto malformed_packet;

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
    goto malformed_packet;

  conn->server.version= strdup(pos);
  pos+= (strlen(conn->server.version) + 1);

  /* boundary checkpoint */
  if (end - pos < 33)
    goto malformed_packet;

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
    goto malformed_packet;

  if (!(conn->server.capabilities & CAP_MYSQL))
    conn->server.mariadb_capabilities= p_to_ui32(pos);
  pos+= 4;

  /* boundary checkpoint */
  if (end - pos < 13)
    goto malformed_packet;

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
      goto malformed_packet;
    conn->scramble.plugin= strdup(pos);
    pos+= strlen(pos) + 1;
  }
  return ER_OK;
malformed_packet:
  return lcc_set_error(&conn->error, LCC_ERROR_INFO, ER_MALFORMED_PACKET, "HY000", NULL, pos - start);
}

static size_t column_offsets[]=
{
  offsetof(LCC_COLUMN, catalog),
  offsetof(LCC_COLUMN, schema),
  offsetof(LCC_COLUMN, table_alias),
  offsetof(LCC_COLUMN, table_name),
  offsetof(LCC_COLUMN, column_alias),
  offsetof(LCC_COLUMN, column_name)
};

LCC_ERRNO lcc_read_response(lcc_connection *conn)
{
  size_t pkt_len= 0;
  char *pos, *end;
  LCC_ERRNO rc;
  uint8_t error= 0;
  size_t len;

start:
  rc= lcc_io_read(conn, &pkt_len);
  if (rc)
    return rc;
  pos= (char *)conn->io.read_pos;
  end= pos + pkt_len;
  conn->io.read_pos= end;
  
  /* Error packet */
  if ((u_char)*pos == 0xFF)
  {
    uint16_t error_no;
    uint8_t stage, max_stage;
    size_t len= 0;
    double progress;
    char *info= NULL;

    pos++;
    if (end - pos < 2)
      goto malformed_packet;

    /* special case: errorcode=0xFFFF is a progress
       indication packet */
    error_no= p_to_ui16(pos);
    if (error_no != 0xFFFF)
    {
      return lcc_read_server_error_packet(pos, end - pos, &conn->error);
    }

    if (conn->configuration.callbacks.report_progress)
    {
      pos+=2;

      if (end - pos < 5)
        goto malformed_packet;

      stage= p_to_ui8(pos);
      pos++;
      max_stage= p_to_ui8(pos);
      pos++;
      progress= p_to_ui24(pos) / 1000.0;

      if (pos < end)
      {
        len= p_to_lenc((u_char **)&pos, (u_char *)end, &error);
        info= pos;
      }
      conn->configuration.callbacks.report_progress((LCC_HANDLE *)conn, stage, max_stage, progress, info, len);
    }   
    goto start;
  }

  /* EOF packet */
  if ((u_char)*pos == 0xFE && 
      pkt_len < 0xFFFFFF)
  {
    pos++;
    if (pos + 4 > end)
      goto malformed_packet;
    conn->server.warning_count= p_to_ui32(pos);
    pos+= 2;
    conn->server.status= p_to_ui32(pos);

    if (conn->configuration.callbacks.status_change &&
        conn->server.status & conn->configuration.callbacks.status_flags)
      conn->configuration.callbacks.status_change((LCC_HANDLE *)conn, conn->server.status);
    return ER_OK;
  }

  /* LOCAL DATA/XML INFILE */
  if ((u_char)*pos == 0xFB)
  {
    /* todo */
  }

  /* OK packet */
  if (*pos == 0x00)
  {
    pos++;
    conn->server.affected_rows= p_to_lenc((u_char **)&pos, (u_char *)end, &error);
    if (error)
      goto malformed_packet;
    conn->server.last_insert_id= p_to_lenc((u_char **)&pos, (u_char *)end, &error);
    if (error)
      goto malformed_packet;
    if (end -pos < 4)
      goto malformed_packet;

    conn->server.status= p_to_ui16(pos);
    pos+= 2;

    if (conn->configuration.callbacks.status_change &&
        conn->server.status & conn->configuration.callbacks.status_flags)
      conn->configuration.callbacks.status_change((LCC_HANDLE *)conn, conn->server.status);

    conn->server.warning_count= p_to_ui32(pos);
    pos+= 2;

    if (pos == end)
      return ER_OK;

    /* info */
    len= p_to_lenc((u_char **)&pos, (u_char *)end, &error);
    if (error || pos + len > end)
      goto malformed_packet;
    if (len &&
        !(conn->server.info= strndup(pos, len)))
      return lcc_set_error(&conn->error, LCC_ERROR_INFO, ER_OUT_OF_MEMORY, "HY000", NULL, len);
    pos+= len;

    if (pos < end &&
        conn->server.capabilities & CAP_SESSION_TRACKING)
    {
      lcc_list_delete(conn->server.session_state, lcc_clear_session_state);
      conn->server.current_session_state= NULL;

      if (conn->server.status & LCC_STATUS_SESSION_STATE_CHANGED)
      {
        char *start_pos;
        size_t sess_len= p_to_lenc((u_char **)&pos, (u_char *)end, &error);

        if (error)
          goto malformed_packet;

        /* boundary check */
        if (pos + sess_len > end)
          goto malformed_packet;

        start_pos= pos;

        while ((size_t)(pos - start_pos) < sess_len)
        {
          LCC_SESSION_TRACK_INFO *info;

          if (!(info= (LCC_SESSION_TRACK_INFO *)calloc(1, sizeof(LCC_SESSION_TRACK_INFO))))
            return lcc_set_error(&conn->error, LCC_ERROR_INFO, ER_OUT_OF_MEMORY, "HY000", NULL);

          info->type= p_to_ui8(pos);
          pos++;

          if (info->type == TRACK_GTID)
          {
            /* skip length */
            (void)p_to_lenc((u_char **)&pos, (u_char *)end, &error);
            if (error)
              goto malformed_packet;
          }

          info->str.len= p_to_lenc((u_char **)&pos, (u_char *)end, &error);
          if (error)
            goto malformed_packet;
          if (!(info->str.str= (char *)malloc(info->str.len)))
            return lcc_set_error(&conn->error, LCC_ERROR_INFO, ER_OUT_OF_MEMORY, "HY000", NULL);
          memcpy(info->str.str, pos, info->str.len);
          pos+= info->str.len;

          lcc_list_add(&conn->server.session_state, info);
        }
      }
    }
    return ER_OK;
  }
  /* Without a special header byte,
     we will get number of columns */
  conn->column_count= p_to_lenc((u_char **)&pos, (u_char *)end, &error);
  if (error)
    goto malformed_packet;
  return ER_OK;

malformed_packet:
  return lcc_set_error(&conn->error, LCC_ERROR_INFO, ER_MALFORMED_PACKET, "HY000", NULL, pos - conn->io.read_pos);
}

LCC_ERRNO
lcc_read_result_metadata(lcc_result *result)
{
  char *pos, *end;
  size_t pkt_len, len;
  LCC_ERRNO rc;
  uint32_t i;
  uint8_t error= 0;
  lcc_connection *conn= result->conn;

  if (!result || result->type != LCC_RESULT)
    return ER_INVALID_HANDLE;

  if (lcc_mem_init(&result->memory, 8192) != ER_OK)
    return lcc_set_error(&conn->error, LCC_ERROR_INFO, ER_OUT_OF_MEMORY, "HY000", NULL, 8192);

  if (!(result->columns= (LCC_COLUMN *)lcc_mem_alloc(&result->memory,
                              conn->column_count * sizeof(LCC_COLUMN))))
    return lcc_set_error(&conn->error, LCC_ERROR_INFO, ER_OUT_OF_MEMORY, "HY000", NULL,
                         sizeof(LCC_COLUMN) * conn->column_count);

  memset(result->columns, 0, sizeof(LCC_COLUMN) * conn->column_count);

  for (i=0; i < conn->column_count; i++)
  {
    uint8_t j;
    LCC_COLUMN column;

    memset(&column, 0, sizeof(LCC_COLUMN));
    rc= lcc_io_read(conn,&pkt_len);
    if (rc)
      return rc;

    pos= conn->io.read_pos;
    end= pos + pkt_len;
    conn->io.read_pos= end;


    /* string values */
    for (j=0; j < sizeof(column_offsets) / sizeof(size_t); j++)
    {
      char **tmp= (char **)((char *)&column + column_offsets[j]);
      len= p_to_lenc((u_char **)&pos, (u_char *)end, &error);
      if (error || pos + len > end)
        goto malformed_packet;
      if (len)
      {
        if (!(*tmp= lcc_mem_alloc(&result->memory, len + 1)))
          return lcc_set_error(&conn->error, LCC_ERROR_INFO, ER_OUT_OF_MEMORY, "HY000", NULL, len + 1);
        strncpy(*tmp, pos, len);
        pos+= len;
      }
    }

    if (conn->server.mariadb_capabilities & CAP_EXTENDED_METADATA)
    {
      char *attr_end;
      uint8_t type;
      len= p_to_lenc((u_char **)&pos, (u_char *)end, &error);
      if (error || pos + len > end)
        goto malformed_packet;

      attr_end= pos + len;
      while (pos < attr_end)
      {
        type= p_to_ui8(pos);
        pos++;
      }
      if (type >= LCC_MAX_FIELD_ATTRS)
        return lcc_set_error(&conn->error, LCC_ERROR_INFO, ER_UNKNOWN_FIELD_ATTR, "HY000", NULL, type);
      len= p_to_lenc((u_char **)&pos, (u_char *)attr_end, &error);
      if (error || pos + len > attr_end)
        goto malformed_packet;
      column.metadata[type]= lcc_mem_alloc(&result->memory, len + 1);
      memcpy((void *)column.metadata[type], pos, len);
      pos+= len;
    }

    len= p_to_lenc((u_char **)&pos, (u_char *)end, &error);
    if (error || pos + len > end)
      goto malformed_packet;

    if (pos + len != end || len != 0x0C)
      goto malformed_packet;

    column.charset_nr= p_to_ui16(pos);
    pos+= 2;
    column.column_size= p_to_ui32(pos);
    pos+= 4;
    column.type= p_to_ui8(pos);
    pos++;
    column.flags= p_to_ui16(pos);
    pos+= 2;
    column.decimals= p_to_ui8(pos);
    pos+= 3;
    memcpy(&result->columns[i], &column, sizeof(LCC_COLUMN));
  }
  conn->status= CONN_STATUS_RESULT;
  /* last packet should be EOF packet */
  return lcc_read_response(conn);

malformed_packet:
  return lcc_set_error(&conn->error, LCC_ERROR_INFO, ER_MALFORMED_PACKET, "HY000", NULL, pos - conn->io.read_pos);
}

LCC_ERRNO lcc_result_fetch_one(lcc_result *result, uint8_t *eof)
{
  LCC_ERRNO rc;
  size_t pkt_len;
  uint8_t error= 0;
  uint32_t i;
  char *pos, *end;
  
  if ((rc= lcc_io_read(result->conn, &pkt_len)))
    return rc;

  pos= result->conn->io.read_pos;
  end= pos + pkt_len;

  result->conn->io.read_pos= end;

  if (pkt_len <= 8 && (u_char)*pos == 0xFE)
  {
    *eof= 1;
    pos++;
    result->conn->server.warning_count= p_to_ui16(result->conn->io.read_pos);
    pos+= 2;
    result->conn->server.status = p_to_ui16(result->conn->io.read_pos + 3);
    pos+= 2;
    return ER_OK;
  }

  if (!result->conn->column_count)
    return ER_NO_RESULT_AVAILABLE;

  if (!result->data)
  {
    if (!(result->data= (LCC_STRING *)lcc_mem_alloc(&result->memory,
                     result->conn->column_count * sizeof(LCC_STRING))))
      return lcc_set_error(&result->conn->error, LCC_ERROR_INFO, ER_OUT_OF_MEMORY, "HY000", NULL,
                         sizeof(LCC_STRING) * result->conn->column_count);
  }

  for (i=0; i < result->conn->column_count; i++)
  {
    result->data[i].len= p_to_lenc((u_char **)&pos, (u_char *)end, &error);
    if (error || pos + result->data[i].len > end)
      goto malformed_packet;

    if (!result->data[i].len)
      result->data[i].str= NULL;
    else
    {
      result->data[i].str= pos;
    }
    if (result->data[i].len > result->columns[i].max_column_size)
    {
      result->columns[i].max_column_size= result->data[i].len;
    }
    pos+= result->data[i].len;
  }
  result->row_count++;
  return ER_OK;
    
malformed_packet:
  return lcc_set_error(&result->conn->error, LCC_ERROR_INFO, ER_MALFORMED_PACKET, "HY000", NULL, 
                       pos - result->conn->io.read_pos);
}
