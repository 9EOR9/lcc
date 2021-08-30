#include <protocol.h>
#include <lcc_priv.h>
#include <lcc_errmsg.h>

ssize_t
lcc_read(int socket, char *buffer, size_t length)
{
  ssize_t rc;

  do {
#ifdef WIN32
    rc= recv(socket, buffer, (int) length, 0);
#else
    rc= recv(socket, buffer, length, MSG_DONTWAIT);            
#ifdef WIN32
  } while (rc == -1 && WSAGetLastError() == 0)
#else
  } while (rc == -1 && errno == SOCKET_EINTR);
#endif
  return rc;
}

ssize_t
lcc_write(int socket, char *buffer, size_t length)
{
  ssize_t rc;
#ifndef WIN32
  int32_t flags= MSG_DONTWAIT;
#endif

#ifdef MSG_NOSIGNAL
  flags|= MSG_NOSIGNAL;
#endif

  do {
#ifdef WIN32
    rc= send(socket, buffer, (int) length, 0);
#else
    rc= send(socket, buffer, length, flags);            
#ifdef WIN32
  } while (rc == -1 && WSAGetLastError() == 0)
#else
  } while (rc == -1 && errno == SOCKET_EINTR);
#endif
  return rc;
}
