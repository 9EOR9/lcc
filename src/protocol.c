#include <lcc.h>
#include <lcc_pack.h>
#include <lcc_priv.h>
#include <lcc_error.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK_IS_CONNECTED(hdl)\
if ((hdl)->type != LCC_CONNECTION)\
  return ER_INVALID_CONNECTION_HANDLE;\
if ((hdl)->socket < 0)\
  return ER_INVALID_SOCKET_DESCRIPTOR;

#define CHECK_NET_BUFFER(hdl)\
if (!(hdl)->net_buffer && \
    (!((hdl)->net_buffer= malloc(LCC_NET_BUFFER_SIZE))))\
      return ER_OUT_OF_MEMORY;\

#define MARIADB_RPL_HACK "5.5.5-"
#define packet_error (u_int32_t)-1

/* utf8mb4 character set number is 45 */
#define UTF8MB4 45

/* default values, they can be overwritten via options */
u_int32_t max_packet_size= 0x40000000;
u_int32_t net_buffer_length= 0x2000;

static u_int32_t
lcc_read(lcc_connection *conn);

static inline u_int16_t
err_malformed_packet(lcc_error *error, size_t offset)
{
  lcc_set_error(error, ER_MALFORMED_PACKET, "HY000", NULL, offset);
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

u_int16_t lcc_send_client_hello(lcc_connection *conn, lcc_scramble *scramble)
{
  char buffer[2048];
  char *p= buffer;
  u_int32_t client_flags= (u_int32_t)CLIENT_DEFAULT_CAPS;

  memset(p, 0, 255);

  /* client capabilities */
  if (conn->options.tls)
    client_flags|= CAP_TLS;
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
    ui32_to_p(p, (u_int32_t)MARIADB_CAPS);
  else
    ui32_to_p(p, 0);
  p+= 4;
 
  /* Todo: if TLS is active we stop here,
   * and perform TLS handshake
   */

  /* user: zero terminated string */
  if (conn->options.user && conn->options.user[0])
  {
    strncat(p, conn->options.user, LCC_MAX_USER_LEN * 4);
    p += strlen(conn->options.user);
  } else
  {
    *p= 0;
    p++;
  }

  if (conn->server.capabilities & CAP_PLUGIN_AUTH_LENENC_CLIENT_DATA)
  {
    /* length encoded authenticatio data */
  } else if (conn->server.capabilities & CAP_SECURE_CONNECTION)
    /* 1st byte: length of authentication data 
       string: authentication data */
  {
  } else
  {
    /* null terminated authentication data */
  }

  /* MariaDB specific client capabilties */
  if (!conn->server.capabilities & CAP_MYSQL)
  {
    conn->client.mariadb_capabilities= MARIADB_CAPS >> 32;
  }
}

u_int16_t lcc_read_server_hello(lcc_connection *conn, lcc_scramble *scramble)
{
  u_int32_t pkt_len;
  char *start, *pos, *end;

  CHECK_IS_CONNECTED(conn);
  CHECK_NET_BUFFER(conn);

  if (!scramble)
    return ER_NULL_VALUE_PARAMETER;

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

  pkt_len= lcc_read(conn);

  if (pkt_len == packet_error ||
      pkt_len == 0)
    return ER_NET_READ;

  start= pos= conn->net_pos= conn->net_buffer + 4;
  end= pos + pkt_len;

  /*
   * In very rare cases we can get an error packet
   * instead of server_hello packet, e.g. if number
   * of failing connection attempts reaches
   * max_connect_errors
   */

  if (*pos == 0xFF)
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
  memcpy(scramble->scramble, pos, 8);
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
  scramble->scramble_len= p_to_ui8(pos);
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
    memcpy(&scramble->scramble[8], pos, scramble->scramble_len - 9);
    pos+= scramble->scramble_len - 9;
    pos++;
  }

  if (conn->server.capabilities & CAP_PLUGIN_AUTH)
  {
    if (scramble->plugin)
      free(scramble->plugin);
    /* boundary checkpoint */
    if (!memchr(pos, 0, end - pos))
      return ER_MALFORMED_PACKET;
    scramble->plugin= strdup(pos);
    printf("plugin: %s\n", scramble->plugin);
  }
  return ER_OK;
}

static u_int32_t
lcc_read(lcc_connection *conn)
{
  u_int32_t pkt_len= 0;
  ssize_t b;
  CHECK_IS_CONNECTED(conn);

  if ((b= read(conn->socket, conn->net_buffer, 8192)))
  {
    pkt_len= p_to_ui24(conn->net_buffer);
    if (pkt_len > b)
      return packet_error;
    conn->net_pos= conn->net_buffer + 4;
  }
  return pkt_len;
}
