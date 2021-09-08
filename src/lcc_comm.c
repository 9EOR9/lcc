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

#ifdef _WIN32
#define socket_error() WinGetLastError()
#else
#define socket_error() errno
#endif

extern u_int32_t comm_buffer_length;

void 
lcc_comm_close(lcc_connection *connection)
{
  if (connection)
  {
    free(connection->comm.readbuf);
    free(connection->comm.writebuf);
    free(connection->scramble.plugin);
  }
  memset(&connection->comm, 0, sizeof(lcc_communication));
}

/**
 * @brief: initializes communication buffers for read/write operations
 **/
LCC_ERROR
lcc_comm_init(lcc_connection *connection)
{
  if ((connection->comm.readbuf= calloc(1, comm_buffer_length)) &&
      (connection->comm.writebuf= calloc(1, comm_buffer_length)))
  {
    /* set initial size */
    connection->comm.read_size=
    connection->comm.write_size= comm_buffer_length;

    return ER_OK;
  }
  lcc_comm_close(connection);
  return ER_OUT_OF_MEMORY;
}

/**
 * @brief: reallocates communication buffer
 @ @return: ER_OK on success, otherwise error number
 **/
LCC_ERROR 
lcc_comm_realloc(lcc_connection *conn, size_t new_size)
{
  char *tmp;
  new_size= (new_size + MIN_COM_BUFFER_SIZE - 1) & ~(MIN_COM_BUFFER_SIZE - 1);

  if (!(tmp= (char *)realloc(conn->comm.readbuf, new_size)))
    return ER_OUT_OF_MEMORY;
  conn->comm.readbuf= tmp;
  conn->comm.read_size= new_size;
  return ER_OK;
}


int
lcc_com_wait(lcc_connection *conn, int32_t timeout, u_int8_t type)
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

LCC_ERROR
lcc_comm_read_socket(lcc_connection *conn, char *buffer, size_t size, ssize_t *bytes_read)
{
  conn->configuration.read_timeout= 1000;
  while ((*bytes_read= recv(conn->socket, buffer, size, MSG_DONTWAIT)) <= 0L)
  {
    if ((socket_error() != EAGAIN) || conn->configuration.read_timeout == 0)
      return ER_COMM_READ;

    if (lcc_com_wait(conn, conn->configuration.read_timeout, 0) < 0)
    {
      lcc_set_error(&conn->error, LCC_ERROR_INFO, ER_COMM_READ, "08001", NULL, errno);
      return ER_COMM_READ;
    }
  }
  return ER_OK;
}

static LCC_ERROR
lcc_comm_write_socket(lcc_connection *conn,
                      char *buffer,
                      size_t len)
{
  int flags= MSG_DONTWAIT | MSG_NOSIGNAL;
  conn->configuration.write_timeout= 1000;
  while (send(conn->socket, buffer, len, flags) < -1)
  {
    if ((socket_error() != EAGAIN) || conn->configuration.write_timeout == 0)
      return ER_COMM_WRITE;

    if (lcc_com_wait(conn, conn->configuration.write_timeout, 1) < 0)
    {
      lcc_set_error(&conn->error, LCC_ERROR_INFO, ER_COMM_WRITE, "08001", NULL, errno);
      return ER_COMM_WRITE;
    }
  }
  return ER_OK;
}

LCC_ERROR
lcc_comm_flush(lcc_connection *conn)
{
  LCC_ERROR rc= ER_OK;
  if (conn->comm.write_pos > conn->comm.writebuf)
    rc= lcc_comm_write_socket(conn, conn->comm.writebuf, conn->comm.write_pos - conn->comm.writebuf);
  conn->comm.write_pos= conn->comm.writebuf;
  return rc;
}

LCC_ERROR
lcc_comm_write_buffer(lcc_connection *conn,
                      char *buffer,
                      u_int32_t len)
{
  LCC_ERROR rc;
  char *end= conn->comm.writebuf + conn->comm.write_size;
  u_int32_t free_bytes= end - conn->comm.write_pos;

  if (free_bytes > 0 && 
      len > free_bytes)
  {
    memcpy(conn->comm.write_pos, buffer, free_bytes);
    if ((rc= lcc_comm_write_socket(conn, conn->comm.writebuf, end - conn->comm.writebuf)))
      return rc;
    conn->comm.write_pos= conn->comm.writebuf;
    buffer+= free_bytes;
    len-= free_bytes;
  }
  memcpy(conn->comm.write_pos, buffer, len);
  conn->comm.write_pos+= len;
  return ER_OK;
}

/**
  * @brief: stores a logical packet into write buffer
  * format: pkt_len (3 bytes) packet_number (1 byte) [command (1 byte)] data
**/
LCC_ERROR
lcc_comm_write(lcc_connection *conn, u_char command, char *buffer, size_t len)
{
  char header[COMM_HEADER_SIZE + 1];
  u_int8_t pkt_nr= 1;
  LCC_ERROR rc;

  if (command)
    len+= 1;
  conn->comm.write_pos= conn->comm.writebuf;

  /* if buffer exceeds MAX_PACKET_SIZE, we need to split package */
  while (len >= MAX_COMM_PACKET_SIZE)
  {
    ui24_to_p(header, MAX_COMM_PACKET_SIZE);
    header[3]= pkt_nr++;
    if (command)
    {
      ui8_to_p(&header[4], command);
      command= 0;
      if ((rc= lcc_comm_write_buffer(conn, header, COMM_HEADER_SIZE + 1)) ||
          (rc= lcc_comm_write_buffer(conn, buffer, MAX_COMM_PACKET_SIZE - 1)))
        return rc;
      buffer+= MAX_COMM_PACKET_SIZE - 1;
    } else
    {
      if ((rc= lcc_comm_write_buffer(conn, header, COMM_HEADER_SIZE)) ||
          (rc= lcc_comm_write_buffer(conn, buffer, MAX_COMM_PACKET_SIZE)))
        return rc;
      buffer+= MAX_COMM_PACKET_SIZE;
    }
    len-= MAX_COMM_PACKET_SIZE;
  }
  /* Write remaining bytes, which might be also zero 
     (see https://mariadb.com/kb/en/0-packet/) */
  ui24_to_p(header, len);
  header[3]= pkt_nr++;
  if ((rc= lcc_comm_write_buffer(conn, header, COMM_HEADER_SIZE)) ||
      (rc= lcc_comm_write_buffer(conn, buffer, (u_int32_t)len)) ||
      (rc= lcc_comm_flush(conn)))
      return rc;
  return ER_OK;
}

LCC_ERROR
lcc_comm_read(lcc_connection *conn,
              size_t *pkt_len)
{
  LCC_ERROR rc;
  char header[COMM_HEADER_SIZE];
  ssize_t bytes_read= 0;
  u_int32_t len= 0;
  
  *pkt_len= 0;
  conn->comm.read_end= conn->comm.readbuf;

  do {
    rc= lcc_comm_read_socket(conn, header, COMM_HEADER_SIZE, &bytes_read);
    if (rc)
      return rc;
    len= p_to_ui24(header);
    *pkt_len+= len;    
    if (*pkt_len > conn->comm.read_size)
    {
      rc= lcc_comm_realloc(conn, *pkt_len);
      if (rc)
        return rc;
    }
    rc= lcc_comm_read_socket(conn, conn->comm.read_end, len, &bytes_read);
    if (rc)
      return rc;
    conn->comm.read_end+= len;
  }
  while (len == MAX_COMM_PACKET_SIZE);

  return ER_OK;
}
