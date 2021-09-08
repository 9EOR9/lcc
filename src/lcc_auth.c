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
#include <lcc_pack.h>
#include <string.h>
#include <lcc_error.h>

#define MAX_SHA1 20

static void lcc_xor_buffer(u_char *xored, const u_char *s1, const u_char *s2, size_t len)
{
  const u_char *end= s1 + len;
  while (s1 < end)
    *xored++= *s1++ ^ *s2++;
}

static LCC_ERROR
lcc_native_password(lcc_connection *conn,
                    const char *password, 
                    u_char *buffer,
                    size_t *buflen)
{
  u_char sha1_1[MAX_SHA1];
  u_char sha1_2[MAX_SHA1];

  SHA1_CTX ctx;
  lcc_scramble *scramble= &conn->scramble;

  if (*buflen < SCRAMBLE_LEN)
    return ER_INVALID_BUFFER_SIZE;

  /* hash password */
  SHA1((char *)sha1_1, password, strlen(password));
  /* hash hashed password */
  SHA1((char *)sha1_2, (char *)sha1_1, 20);
  
  /* hash scramble + sha1_2 */
  SHA1Init(&ctx);
  SHA1Update(&ctx, (u_char *)scramble->scramble, SCRAMBLE_LEN);
  SHA1Update(&ctx, (u_char *)sha1_2, MAX_SHA1);
  SHA1Final((u_char *)buffer, &ctx);

  /* xor with sha1 */
  lcc_xor_buffer(buffer, buffer, sha1_1, MAX_SHA1); 
  *buflen= MAX_SHA1;
  return ER_OK;
}

LCC_ERROR 
lcc_auth(lcc_connection *conn,
         const char *password,
         u_char *buffer,
         size_t *buflen)
{
  /* mysql_native_password */
  if (!strcmp(conn->scramble.plugin, "mysql_native_password"))
    return lcc_native_password(conn, password, buffer, buflen);

  /* no matching plugin found */
  lcc_set_error(&conn->error, LCC_ERROR_INFO, ER_UNKNOWN_AUTH_METHOD, "HY000", NULL, conn->scramble.plugin);
  return 0;
}
