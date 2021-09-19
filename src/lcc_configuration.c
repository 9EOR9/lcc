/* lcc_configuration */
#include <lcc.h>
#include <lcc_priv.h>
#include <lcc_error.h>
#include <lcc_pack.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <ini.h>

#define MAX_RECURSION 32

lcc_key_val default_conn_attr[]=
{
  {"_client_name", "lcc"},
  {"_client_version", LCC_VERSION_STR},
  {"_os", LCC_SYSTEM_TYPE},
  {"_platform", LCC_MACHINE_TYPE},
  {NULL, NULL}
};

char **lcc_configuration_dirs= NULL;

typedef struct {
  lcc_connection *conn;
  const char *section;
  uint8_t recursion;
} config_info;

static int 
lcc_parse_ini(config_info *info,
              const char *filename);

lcc_configuration_options lcc_conf_options[]=
{
  {
    LCC_OPT_CURRENT_DB,
    offsetof(lcc_connection, configuration.current_db),
    LCC_CONF_STR,
    (const char *[]){"database", "db", NULL}
  },
  {
    LCC_OPT_AUTH_PLUGIN,
    offsetof(lcc_connection, configuration.auth_plugin),
    LCC_CONF_STR,
    (const char *[]){"auth_plugin", NULL}
  },
  {
    LCC_OPT_TLS_CERT,
    offsetof(lcc_connection, configuration.tls_cert),
    LCC_CONF_STR,
    (const char *[]){"tls_cert", "ssl_cert", NULL}
  },
  {
    LCC_OPT_TLS_KEY,
    offsetof(lcc_connection, configuration.tls_key),
    LCC_CONF_STR,
    (const char *[]){"tls_key", "ssl_key", NULL}
  },
  {
    LCC_OPT_TLS_CIPHER,
    offsetof(lcc_connection, configuration.tls_cipher),
    LCC_CONF_STR,
    (const char *[]){"tls_cipher", "ssl_cipher", NULL}
  },
  {
    LCC_OPT_TLS_CA,
    offsetof(lcc_connection, configuration.tls_ca),
    LCC_CONF_STR,
    (const char *[]){"tls_ca", "ssl_ca", NULL}
  },
  {
    LCC_OPT_TLS_CA_PATH,
    offsetof(lcc_connection, configuration.tls_ca_path),
    LCC_CONF_STR,
    (const char *[]){"tls_capath", "ssl_capath", NULL}
  },
  {
    LCC_OPT_TLS_CRL,
    offsetof(lcc_connection, configuration.tls_crl),
    LCC_CONF_STR,
    (const char *[]){"tls_crl", "ssl_crl", NULL}
  },
  {
    LCC_OPT_TLS_CRL_PATH,
    offsetof(lcc_connection, configuration.tls_crl_path),
    LCC_CONF_STR,
    (const char *[]){"tls_crlpath", "ssl_crlpath", NULL}
  },
  {
    LCC_OPT_TLS_CRL_PATH,
    offsetof(lcc_connection, configuration.tls_crl_path),
    LCC_CONF_STR,
    (const char *[]){"tls_crlpath", "ssl_crlpath", NULL}
  },
  {
    LCC_OPT_TLS_VERIFY_PEER,
    offsetof(lcc_connection, configuration.tls_verify_peer),
    LCC_CONF_INT8,
    (const char *[]){"tls_verify_peer", "ssl_verify", NULL}
  },
  {
    LCC_OPT_USER,
    offsetof(lcc_connection, configuration.user),
    LCC_CONF_STR,
    (const char *[]){"user", NULL}
  },
  {
    LCC_OPT_PASSWORD,
    offsetof(lcc_connection, configuration.password),
    LCC_CONF_STR,
    (const char *[]){"password", "passwd", NULL}
  },
  {
    LCC_OPT_SOCKET_NO,
    offsetof(lcc_connection, socket),
    LCC_CONF_INT32,
    (const char *[]){"socket_no", NULL}
  },
  {
    LCC_OPT_REMEMBER_CONFIG,
    offsetof(lcc_connection, configuration.remember),
    LCC_CONF_INT8,
    (const char *[]){"remember_config", NULL}
  },
};

/*
 *  lcc_get_configuration(key, option)
 *
 *  returns a lcc_configuration option
 */
static lcc_configuration_options *
lcc_get_configuration(const char *key,
                      LCC_OPTION option)
{
  uint16_t i;

  for (i=0; i < sizeof(lcc_conf_options) / sizeof(lcc_configuration_options); i++)
  {
    if (key && key[0])
    {
      uint8_t j= 0;
      while (lcc_conf_options[i].keys[j])
      {
        if (!strcmp(key, lcc_conf_options[i].keys[j]))
          return &lcc_conf_options[i];
        j++;
      }
    }
    /* todo: order lcc_conf_options, so we can use option as index,
     * there should be a short test to check right order
     */
    else if (option && lcc_conf_options[i].option == option)
    {
      return &lcc_conf_options[i];
    }
  }
  return NULL;
}

LCC_ERRNO API_FUNC
LCC_configuration_set(LCC_HANDLE *handle,
                      const char *option_str,
                      LCC_OPTION option,
                      void *buffer)
{
#define GET_INTVAL(addr, type, str, val)\
if (str)\
  *(addr)= (type)atoll((char *)val);\
else\
  *(addr)= *(type *)val;

  lcc_connection *conn= (lcc_connection *)handle;
  lcc_configuration_options *conf= lcc_get_configuration(option_str, option);

  if (!conf || !conn)
    return 0;

  switch (conf->type) {
    case LCC_CONF_STR:
    {
      char **address= LCC_FIELD_PTR(conn, conf->offset, char *);

      if (*address)
        free((char *)*address);
      *address= strdup((char *)buffer);
    }
    break;
    case LCC_CONF_INT32:
    {
      uint32_t *address= LCC_FIELD_PTR(conn, conf->offset, uint32_t);
      GET_INTVAL(address, uint32_t, option_str, buffer);
    }
    break;
    case LCC_CONF_INT8:
    {
      uint8_t *address= LCC_FIELD_PTR(conn, conf->offset, uint8_t);
      GET_INTVAL(address, uint8_t, option_str, buffer);
    }
    break;
    default:
      break;
  }
  return 0;
}

/*
 * release configuration memory
 */
void lcc_configuration_close(lcc_connection *conn)
{
  uint32_t i;
  for (i=0; i < sizeof(lcc_conf_options) / sizeof(lcc_configuration_options); i++)
  {
    if (lcc_conf_options[i].type == LCC_CONF_STR)
    {
      char **address= LCC_FIELD_PTR(conn, lcc_conf_options[i].offset, char *);
      if (*address)
        free(*address);
    }
  }
  memset(&conn->configuration, 0, sizeof(lcc_configuration));
}

/* MariaDB/MySQL related */
static const char *client_sections[] = {
  "client", "client-server", "client-mariadb", "lcc-client", NULL
};

static uint8_t lcc_is_client_section(const char *section)
{
  uint8_t i;

  for (i=0; client_sections[i]; i++)
    if (!strcmp(client_sections[i], section))
      return 1;
  return 0;
}

/* callback function for ini parser */
static int
lcc_configuration_handler(void* data, 
                          const char* section,
                          const char* name,
                          const char* value)
{
    config_info *info= (config_info *)data;
    uint8_t valid_section= 0;

    if (info->section)
      valid_section= (strcmp(section, info->section) == 0);
    else 
      valid_section= lcc_is_client_section(section);

    if (valid_section)
    {
      if (name)
      { 
        if (name[0] != '!')
        {
          LCC_configuration_set((LCC_HANDLE *)info->conn, name, 0, (void *)value);
        }
        else {
          char *start= (char *)name;
          char *end= start + strlen(name);

          if (end - start < 9)
            return 1;
          if (strncmp(start + 1, "include", 7))
            return 1;
          start+= 8;
          while (*start == ' ' && start < end)
            start++;
          lcc_parse_ini(info, start);
        }
      }
    }
    return 1;
}

static int 
lcc_parse_ini(config_info *info,
              const char *filename)
{
  if (info->recursion > MAX_RECURSION)
    return 0;
  info->recursion++;
  if (ini_parse(filename, lcc_configuration_handler, info) < 0)
    return 1;
  info->recursion--;
  return 0;
}

uint8_t
LCC_configuration_load_file(LCC_HANDLE *handle, const char *filenames[], const char *section)
{
  uint8_t i= 0;
  config_info info;

  memset(&info, 0, sizeof(config_info));
  info.conn= (lcc_connection *)handle;
  info.section= section;
  info.recursion= 0;

  while (filenames[i])
  {
    if (!access(filenames[i], R_OK))
      if (lcc_parse_ini(&info, filenames[i]))
        return 1;
    i++;
  }
  return 0;
}

LCC_ERRNO 
LCC_set_option(LCC_HANDLE *hdl, LCC_OPTION option, ...)
{
  va_list ap;
  void *opt1, *opt2;
  uint16_t error_code= ER_OK;
  lcc_connection *conn= NULL;

  va_start(ap, option);
  opt1= va_arg(ap, void *);

  if (hdl && hdl->type == LCC_CONNECTION)
    conn= (lcc_connection *)hdl;

  switch(option) {
    /* callback function for server status */
    case LCC_OPT_STATUS_CALLBACK:
      opt2= va_arg(ap, void *);
      conn->configuration.callbacks.status_change= opt1;
      conn->configuration.callbacks.status_flags= opt2 ? *((uint16_t *)opt2) : 0;
      break;
    case LCC_OPT_PROGRESS_REPORT_CALLBACK:
      conn->configuration.callbacks.report_progress= opt1;
      break;
    default:
      error_code= ER_INVALID_OPTION;
  }
  va_end(ap);
  return error_code;
}
