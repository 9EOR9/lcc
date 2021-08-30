/* authentication methods */

/* mysql native password:
 *
 * Password is calculated as:
 * sha1(password) ^ sha1(scramble + sha1(sha1(password)))
 */

#include <stdint.h>
#include <sha1.h>
#include <lcc.h>
#include <lcc_priv.h>
#include <string.h>
#include <lcc_error.h>

#define MAX_SHA1 20
#define SCRAMBLE_LENGTH 20


static u_int16_t
lcc_native_password(lcc_connection *conn,
                    const char *password, 
                    unsigned char *buffer,
                    size_t buflen)
{
  unsigned char sha1_1[MAX_SHA1];
  unsigned char sha1_2[MAX_SHA1];

  unsigned char *start= buffer,
                *end= start + buflen;
  SHA1_CTX ctx;
  lcc_scramble *scramble= &conn->scramble;

  if (buflen < MAX_SHA1)
    return 0;

  /* hash password */
  SHA1(sha1_1, password, strlen(password));
  /* hash hashed password */
  SHA1(sha1_2, sha1_1, 20);
  
  /* hash scramble + sha1_2 */
  SHA1Init(&ctx);
  SHA1Update(&ctx, scramble->scramble, SCRAMBLE_LENGTH);
  SHA1Update(&ctx, sha1_2, MAX_SHA1);
  SHA1Final(buffer, &ctx);

  /* xor with sha1 */
  while (start < end)
  {
    *start= *start ^ sha1_1[20 - (end - start)];
    start++;
  }

  return MAX_SHA1;
}

u_int16_t 
lcc_auth(lcc_connection *conn,
         const char *password,
         unsigned char *buffer,
         size_t buflen)
{
  /* mysql_native_password */
  if (!strcmp(conn->scramble.plugin, "mysql_native_password"))
    return lcc_native_password(conn, password, buffer, buflen);

  /* no matching plugin found */
  lcc_set_error(&conn->error, ER_UNKNOWN_AUTH_METHOD, "HY000", NULL, conn->scramble.plugin);
  return 0;
}
