#include <lcc.h>
#include <lcc_priv.h>
#include <lcc_error.h>
#include <lcc_pack.h>
#include <stdlib.h>
#include <errno.h>
#include <libsocket/libunixsocket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>

#define MAX_COMM_PACKET_SIZE 0xFFFFFF
#define COMM_HEADER_SIZE 4
#define COMM_CACHE_SIZE 0x4000

#ifdef _WIN32
#define socket_error() WinGetLastError()
#else
#define socket_error() errno
#endif

extern uint32_t comm_buffer_length;

void 
lcc_io_close(lcc_connection *conn)
{
  if (conn)
  {
    (void)lcc_io_write(conn, CMD_CLOSE, NULL, 0);

    free(conn->io.readbuf);
    free(conn->io.writebuf);
    free(conn->scramble.plugin);
    memset(&conn->io, 0, sizeof(lcc_io));
  }
}

/**
 * @brief: initializes communication buffers for read/write operations
 **/
LCC_ERRNO
lcc_io_init(lcc_connection *connection)
{
  if ((connection->io.readbuf= calloc(1, comm_buffer_length)) &&
      (connection->io.writebuf= calloc(1, comm_buffer_length)))
  {
    /* set initial size */
    connection->io.read_size=
    connection->io.write_size= comm_buffer_length;

    connection->io.read_pos= connection->io.read_end= connection->io.readbuf;

    return ER_OK;
  }
  lcc_io_close(connection);
  return ER_OUT_OF_MEMORY;
}

/**
 * @brief: reallocates communication buffer
 @ @return: ER_OK on success, otherwise error number
 **/
LCC_ERRNO 
lcc_io_realloc(lcc_connection *conn, size_t size)
{
  char *tmp;
  size_t new_size= lcc_align_size(MIN_COM_BUFFER_SIZE, size);

  if (!(tmp= (char *)realloc(conn->io.readbuf, new_size)))
    return ER_OUT_OF_MEMORY;
  conn->io.read_pos= tmp + (conn->io.read_pos - conn->io.readbuf);
  conn->io.readbuf= tmp;
  conn->io.read_size= new_size;
  return ER_OK;
}


uint32_t lcc_buffered_packets(lcc_connection *conn)
{
  uint32_t packets= 0;
  char *pos= conn->io.read_pos;

  while (pos < conn->io.read_end)
  {
    size_t pkt_len= p_to_ui24(pos);
    pos+= pkt_len + COMM_HEADER_SIZE;
    packets++;
  }
  return packets;
}

uint32_t lcc_buffered_error_packet(lcc_connection *conn)
{
  uint32_t packet_nr= 0;
  char *pos= conn->io.read_pos;

  while (pos < conn->io.read_end)
  {
    size_t pkt_len= p_to_ui24(pos);
    pos+= COMM_HEADER_SIZE;
    packet_nr++;
    if (pkt_len > 2 && (u_char)*pos == 0xFF)
    {
      if (p_to_ui16((pos +1)) != 0xFFFF)
        break;
    }
    pos+= pkt_len;
  }
  return packet_nr;
}

static int
lcc_io_wait(lcc_connection *conn, int32_t timeout, uint8_t type)
{
  int rc;

#ifndef _WIN32
  struct pollfd p_fd;
#else
  struct timeval tv= {0,0};
  fd_set fds, exc_fds;
#endif

#ifndef _WIN32
  memset(&p_fd, 0, sizeof(p_fd));
  p_fd.fd= conn->socket;
  if (type == 0) /* read */
    p_fd.events= POLLIN;
  else
    p_fd.events= POLLOUT;

  /* if no timeout was specified, we set it to infinitive */
  if (timeout == 0)
      timeout= -1;

  do {
    rc= poll(&p_fd, 1, timeout);
  } while (rc == -1 && errno == EINTR);

  if (rc == 0)
    errno= ETIMEDOUT;
#else
  FD_ZERO(&fds);
  FD_ZERO(&exc_fds);

  FD_SET(conn->socket, &fds);
  FD_SET(conn->socket, &exc_fds);

  if (timeout >= 0)
  {
    tv.tv_sec= timeout / 1000;
    tv.tv_usec= (timeout % 1000) * 1000;
  }

  if (type == 0)
    rc= select(0, &fds, &exc_fds, (timeout >= 0) ? &tv : NULL);
  else
    rc= select(0, &fds, &exc_fds, (timeout >= 0) ? &tv : NULL);

  if (rc == SOCKET_ERROR)
  {
    errno= WSAGetLastError();
  }
  else if (rc == 0)
  {
    rc= SOCKET_ERROR;
    WSASetLastError(WSAETIMEDOUT);
    errno= ETIMEDOUT;
  }
  else if (FD_ISSET(conn->socket, &exc_fds))
  {
    int err;
    int len = sizeof(int);
    if (getsockopt(conn->socket, SOL_SOCKET, SO_ERROR, (char *)&err, &len) != SOCKET_ERROR)
    {
      WSASetLastError(err);
      errno= err;
    }
    rc= SOCKET_ERROR;
  }
#endif
  return rc;
}

LCC_ERRNO
lcc_io_read_socket(lcc_connection *conn, char *buffer, size_t size, ssize_t *bytes_read)
{
  conn->configuration.read_timeout= 1000;

  while ((*bytes_read= recv(conn->socket, buffer, size, MSG_DONTWAIT)) <= 0L)
  {
    if ((socket_error() != EAGAIN) || conn->configuration.read_timeout == 0)
      return ER_COMM_READ;

    if (lcc_io_wait(conn, conn->configuration.read_timeout, 0) < 0)
    {
      lcc_set_error(&conn->error, LCC_ERROR_INFO, ER_COMM_READ, "08001", NULL, errno);
      return ER_COMM_READ;
    }
  }
  return ER_OK;
}

static LCC_ERRNO
lcc_io_write_socket(lcc_connection *conn,
                      char *buffer,
                      size_t len)
{
  int flags= MSG_DONTWAIT | MSG_NOSIGNAL;
  conn->configuration.write_timeout= 1000;
  while (send(conn->socket, buffer, len, flags) < -1)
  {
    if ((socket_error() != EAGAIN) || conn->configuration.write_timeout == 0)
      return ER_COMM_WRITE;

    if (lcc_io_wait(conn, conn->configuration.write_timeout, 1) < 0)
    {
      lcc_set_error(&conn->error, LCC_ERROR_INFO, ER_COMM_WRITE, "08001", NULL, errno);
      return ER_COMM_WRITE;
    }
  }
  return ER_OK;
}

LCC_ERRNO
lcc_io_flush(lcc_connection *conn)
{
  LCC_ERRNO rc= ER_OK;
  if (conn->io.write_pos > conn->io.writebuf)
    rc= lcc_io_write_socket(conn, conn->io.writebuf, conn->io.write_pos - conn->io.writebuf);
  conn->io.write_pos= conn->io.writebuf;
  return rc;
}

LCC_ERRNO
lcc_io_write_buffer(lcc_connection *conn,
                      char *buffer,
                      uint32_t len)
{
  LCC_ERRNO rc;
  char *end= conn->io.writebuf + conn->io.write_size;
  uint32_t free_bytes= end - conn->io.write_pos;

  if (!buffer || !len)
    return ER_OK;

  if (free_bytes > 0 && 
      len > free_bytes)
  {
    memcpy(conn->io.write_pos, buffer, free_bytes);
    if ((rc= lcc_io_write_socket(conn, conn->io.writebuf, end - conn->io.writebuf)))
      return rc;
    conn->io.write_pos= conn->io.writebuf;
    buffer+= free_bytes;
    len-= free_bytes;
  }
  memcpy(conn->io.write_pos, buffer, len);
  conn->io.write_pos+= len;
  return ER_OK;
}

/**
  * @brief: stores a logical packet into write buffer
  * format: pkt_len (3 bytes) packet_number (1 byte) [command (1 byte)] data
**/
LCC_ERRNO
lcc_io_write(lcc_connection *conn, lcc_ioand command, char *buffer, size_t len)
{
  char header[COMM_HEADER_SIZE + 1];
  uint8_t pkt_nr= 0;
  LCC_ERRNO rc;

  if (command)
    len+= 1;
  conn->io.write_pos= conn->io.writebuf;

  if (command == CMD_NONE)
    pkt_nr= 1;

  /* if buffer exceeds MAX_PACKET_SIZE, we need to split package */
  while (len >= MAX_COMM_PACKET_SIZE)
  {
    ui24_to_p(header, MAX_COMM_PACKET_SIZE);
    header[3]= pkt_nr++;
    if (command != CMD_NONE)
    {
      ui8_to_p(&header[4], command);
      command= CMD_NONE;
      if ((rc= lcc_io_write_buffer(conn, header, COMM_HEADER_SIZE + 1)) ||
          (rc= lcc_io_write_buffer(conn, buffer, MAX_COMM_PACKET_SIZE - 1)))
        return rc;
      buffer+= MAX_COMM_PACKET_SIZE - 1;
    } else
    {
      if ((rc= lcc_io_write_buffer(conn, header, COMM_HEADER_SIZE)) ||
          (rc= lcc_io_write_buffer(conn, buffer, MAX_COMM_PACKET_SIZE)))
        return rc;
      buffer+= MAX_COMM_PACKET_SIZE;
    }
    len-= MAX_COMM_PACKET_SIZE;
  }
  /* Write remaining bytes, which might be also zero 
     (see https://mariadb.com/kb/en/0-packet/) */
  ui24_to_p(header, len);
  header[3]= pkt_nr++;
  if (command != CMD_NONE)
    header[4]= (uint8_t)command;
  if ((rc= lcc_io_write_buffer(conn, header, COMM_HEADER_SIZE + (command != CMD_NONE))) ||
      (rc= lcc_io_write_buffer(conn, buffer, (uint32_t)len)) ||
      (rc= lcc_io_flush(conn)))
      return rc;
  return ER_OK;
}

static LCC_ERRNO
lcc_io_read_buffer(lcc_connection *conn, size_t *length)
{
  lcc_io *io= &conn->io;
  size_t cached_bytes= io->read_end - io->read_pos;
  ssize_t bytes_read;
  LCC_ERRNO rc;

  /* check if read_pos is ok */
  if (io->read_pos == io->read_end)
  {
    io->read_pos= io->read_end= io->readbuf;
  }

  /* check if required data is cached */
  if (cached_bytes >= *length)
  {
    return ER_OK;
  }

  /* move block to the beginning */
  if (cached_bytes && io->read_pos > io->readbuf)
  {
    memmove(io->readbuf, io->read_pos, cached_bytes);
    io->read_pos= io->readbuf;
    io->read_end= io->readbuf + cached_bytes;
  }

  if ((rc= lcc_io_read_socket(conn, io->read_end, io->read_size - cached_bytes, &bytes_read)))
    return rc;

  /* mark end of readbuf */
  io->read_end+= bytes_read;
  return ER_OK;
}

/**
 * @brief: reads a server packet
 *
 * @param: conn - a connection handle
 * @param: pkt_len[inout] - a pointer which contains the packet length
           of the packet
 * 
 * @return: ER_OK or error code
 */
LCC_ERRNO
lcc_io_read(lcc_connection *conn,
              size_t *pkt_len)
{
  LCC_ERRNO rc;
  size_t bytes_read;
  uint32_t len= 0;
  
  *pkt_len= 0;

  do {
    bytes_read= COMM_HEADER_SIZE;
    if ((rc= lcc_io_read_buffer(conn, &bytes_read)))
      return rc;
    if (bytes_read < COMM_HEADER_SIZE)
      return ER_COMM_READ;
    len= p_to_ui24(conn->io.read_pos);
    conn->io.read_pos+= COMM_HEADER_SIZE;
    *pkt_len= len;
    if (*pkt_len > conn->io.read_size)
    {
      rc= lcc_io_realloc(conn, *pkt_len);
      if (rc)
        return rc;
    }
    bytes_read= len;
    rc= lcc_io_read_buffer(conn, &bytes_read);
    if (rc)
      return rc;
  }
  while (len == MAX_COMM_PACKET_SIZE);

  return ER_OK;
}
