#include <lcc.h>
#include <lcc_priv.h>
#include <lcc_pack.h>
#include <lcc_error.h>
#include <float.h>

lcc_bin_type lcc_bin_types[LCC_COLTYPE_GEOMETRY +1];
uint8_t lcc_stmt_initialized= 0;

#define STMT_EXEC_HEADER_SIZE 10

void lcc_stmt_init_bin_types()
{
  uint32_t var_length_types[]= {
    LCC_COLTYPE_BLOB8, LCC_COLTYPE_BLOB24, LCC_COLTYPE_BLOB32,
    LCC_COLTYPE_BLOB64, LCC_COLTYPE_BIT, LCC_COLTYPE_JSON,
    LCC_COLTYPE_NEWDECIMAL, LCC_COLTYPE_ENUM, LCC_COLTYPE_SET,
    LCC_COLTYPE_VARSTR, LCC_COLTYPE_STR, LCC_COLTYPE_GEOMETRY,
  };
  uint8_t i;

  if (lcc_stmt_initialized)
    return;

  memset(lcc_bin_types, 0, sizeof(lcc_bin_types));

  /* numeric types */
  lcc_bin_types[LCC_COLTYPE_INT8].display_len= 4;
  lcc_bin_types[LCC_COLTYPE_INT8].store_len= 1;

  lcc_bin_types[LCC_COLTYPE_INT16].display_len= 6;
  lcc_bin_types[LCC_COLTYPE_INT16].store_len= 2;

  lcc_bin_types[LCC_COLTYPE_YEAR].display_len= 4;
  lcc_bin_types[LCC_COLTYPE_YEAR].store_len= 2;

  lcc_bin_types[LCC_COLTYPE_INT24].display_len= 8;
  lcc_bin_types[LCC_COLTYPE_INT24].store_len= 4;

  lcc_bin_types[LCC_COLTYPE_INT32].display_len= 11;
  lcc_bin_types[LCC_COLTYPE_INT32].store_len= 4;

  lcc_bin_types[LCC_COLTYPE_INT64].display_len= 20;
  lcc_bin_types[LCC_COLTYPE_INT64].store_len= 8;

  lcc_bin_types[LCC_COLTYPE_FLOAT].display_len= 3 + FLT_MANT_DIG + FLT_MIN_EXP;
  lcc_bin_types[LCC_COLTYPE_FLOAT].store_len= 4;

  lcc_bin_types[LCC_COLTYPE_DOUBLE].display_len= 3 + DBL_MANT_DIG + DBL_MIN_EXP;
  lcc_bin_types[LCC_COLTYPE_DOUBLE].store_len= 8;

  /* date and time types */
  lcc_bin_types[LCC_COLTYPE_TIME].display_len= 17;
  lcc_bin_types[LCC_COLTYPE_TIME].store_len= -1;

  lcc_bin_types[LCC_COLTYPE_DATE].display_len= 10;
  lcc_bin_types[LCC_COLTYPE_DATE].store_len= -1;

  lcc_bin_types[LCC_COLTYPE_DATETIME].display_len= 30;
  lcc_bin_types[LCC_COLTYPE_DATETIME].store_len= -1;

  lcc_bin_types[LCC_COLTYPE_TIMESTAMP].display_len= 10;
  lcc_bin_types[LCC_COLTYPE_TIMESTAMP].store_len= -1;

  /* All other column types have variable lengths */
  for (i=0; i < sizeof(var_length_types) / sizeof(uint32_t); i++)
  {
    lcc_bin_types[var_length_types[i]].display_len =
    lcc_bin_types[var_length_types[i]].store_len = -1;
  }
  lcc_stmt_initialized= 1;
}

LCC_ERRNO
lcc_stmt_close(LCC_HANDLE *handle)
{
  LCC_ERRNO rc;
  lcc_stmt *stmt= (lcc_stmt *)handle;

  if ((rc= lcc_validate_handle(handle, LCC_STATEMENT)))
    goto error;

  return ER_OK;
error:
  return lcc_set_error(&stmt->error, LCC_ERROR_INFO, rc, "HY000", NULL);
}

LCC_ERRNO API_FUNC
LCC_statement_prepare(LCC_HANDLE *handle,
                      const char *stmt_str,
                      size_t len)
{
  LCC_ERRNO rc;
  lcc_stmt *stmt= (lcc_stmt *)handle;

  if ((rc= lcc_validate_handle(handle, LCC_STATEMENT)))
    return rc;

  if (!stmt_str || stmt_str[0] == 0 || len == 0)
  {
    rc= ER_INVALID_POINTER;
    goto error;
  }

  if ((ssize_t)len == -1)
    len= strlen(stmt_str);

  if((rc= lcc_io_write(stmt->conn, CMD_STMT_PREPARE, (char *)stmt_str, len)))
    goto error;

  return ER_OK;
error:
  return lcc_set_error(&stmt->error, LCC_ERROR_INFO, rc, "HY000", NULL);
}

LCC_ERRNO API_FUNC
LCC_statement_read_prepare_response(LCC_HANDLE *handle)
{
  if (lcc_validate_handle(handle, LCC_STATEMENT))
    return ER_INVALID_HANDLE;
  return lcc_read_prepare_response((lcc_stmt *)handle);
}

LCC_ERRNO API_FUNC
LCC_statement_set_params(LCC_HANDLE *handle,
                         void *params,
                         stmt_param_callback cb)
{
  LCC_ERRNO rc;
  lcc_stmt *stmt;

  if (lcc_validate_handle(handle, LCC_STATEMENT))
    return ER_INVALID_HANDLE;

  stmt= (lcc_stmt *)handle;

  if (!params)
  {
    rc= ER_INVALID_POINTER;
    goto error;
  }

  if (!stmt->param_count)
  {
    rc= ER_STMT_WITHOUT_PARAMETERS;
    goto error;
  }

  stmt->params= params;
  stmt->param_callback= cb;

  return ER_OK;
error:
  return lcc_set_error(&stmt->error, LCC_ERROR_INFO, rc, "HY000", NULL);
}

LCC_ERRNO API_FUNC
LCC_stmt_set_param(LCC_HANDLE *handle, LCC_BIND *bind)
{
  LCC_ERRNO rc;
  lcc_stmt *stmt= (lcc_stmt *)handle;

  if (lcc_validate_handle(handle, LCC_STATEMENT))
  {
    rc= ER_INVALID_HANDLE;
    goto error;  
  }

  if (!bind)
  {
    rc= ER_INVALID_POINTER;
    goto error;
  }

  stmt->params= bind;
  return ER_OK;

error:
  return lcc_set_error(&stmt->error, LCC_ERROR_INFO, rc, "HY000", NULL);
}

size_t lcc_store_param(u_char *buffer, LCC_BIND *param)
{
  switch(param->buffer_type)
  {
    case LCC_COLTYPE_INT8:
      ui8_to_p(buffer, *((uint8_t *)param->buffer.buf));
      return 1;
    case LCC_COLTYPE_INT16:
      ui16_to_p(buffer, *((uint16_t *)param->buffer.buf));
      return 2;
    case LCC_COLTYPE_INT24:
    case LCC_COLTYPE_INT32:
      ui32_to_p(buffer, *((uint32_t *)param->buffer.buf));
      return 4;
    case LCC_COLTYPE_INT64:
      ui64_to_p(buffer, *((uint64_t *)param->buffer.buf));
      return 8;
    case LCC_COLTYPE_FLOAT:
      f4_to_p(buffer, *((float *)param->buffer.buf));
      return 4;
    case LCC_COLTYPE_DOUBLE:
      d_to_p(buffer, *((double *)param->buffer.buf));
      return 4;
    case LCC_COLTYPE_TIME:
    case LCC_COLTYPE_DATE:
    case LCC_COLTYPE_DATETIME:
      return time_to_p((LCC_TIME *)param->buffer.buf, buffer);
    default:
      return strlenc_to_p(buffer, param->buffer.buf, param->buffer.len);
      break;
  }
}

/**
 * @brief: LCC_stmt_set_params
 *
 *
 *
 *
 */
LCC_ERRNO API_FUNC
LCC_stmt_fill_exec_buffer(LCC_HANDLE *handle)
{
  LCC_ERRNO rc;
  uint32_t i;
  lcc_stmt *stmt= (lcc_stmt *)handle;
  u_char *start, *pos, *null_pos;
  size_t null_size, total_length;

  if (lcc_validate_handle(handle, LCC_STATEMENT))
    return ER_INVALID_HANDLE;

  /* Calculate length */
  null_size= (stmt->param_count + 7) / 8;
  total_length= STMT_EXEC_HEADER_SIZE + null_size + 1 + stmt->param_count * 2;

  for (i=0; i < stmt->param_count; i++)
  {
    size_t len= lcc_bin_types[stmt->params[i].buffer_type].store_len;

    if (len == (size_t)-1)
    {
      len= stmt->params[i].buffer.len + 5;
    }
    total_length+= len;
  }

  if (total_length > stmt->execbuf.len)
  {
    void *tmp= stmt->execbuf.buf;
    total_length= lcc_align_size(LCC_MEM_ALIGN_SIZE, total_length);
    if (!(stmt->execbuf.buf= realloc(stmt->execbuf.buf, total_length)))
    {
      rc= ER_OUT_OF_MEMORY;
      stmt->execbuf.buf= tmp;
      goto error;
    }
    stmt->execbuf.len= total_length;
  }

  pos= start= (u_char *)stmt->execbuf.buf;

  /* Execute header */
  ui32_to_p(pos, stmt->id);
  pos+= 4;
  *pos++= 0;
  ui32_to_p(pos, (uint32_t)1);
  pos+= 4;

  stmt->exec_len= pos - start;

  /* if statement has no parameters, we just quit */
  if (!stmt->param_count)
    return ER_OK;

  null_pos= pos;
  pos+= null_size;
  memset(null_pos, 0, null_size);
  *pos++= 1; /* send types to server */
  stmt->exec_len= pos - start;

  /* types */
  for (i=0; i < stmt->param_count; i++)
  {
    LCC_BIND *bind= &stmt->params[i];

    /* Null values will be stored in null bitmap */
    if (bind->buffer_type == LCC_COLTYPE_NULL ||
        !bind->buffer.buf ||
        bind->indicator == LCC_INDICATOR_NULL)
    {
      bind->has_data= 0;
      null_pos[i/8]= (u_char)(1 << (i & 7));
    } else
    {
      bind->has_data= 1;
      ui8_to_p(pos, bind->buffer_type);
      pos++;
      ui8_to_p(pos, bind->is_unsigned ? 128 : 0);
      pos++;
    }
  }
  for (i=0; i < stmt->param_count; i++)
  {
    if (stmt->params[i].has_data)
    {
      pos+= lcc_store_param(pos, &stmt->params[i]);
    }
  }
  stmt->exec_len= pos - start;

  return ER_OK;
error:
  return lcc_set_error(&stmt->error, LCC_ERROR_INFO, rc, "HY000", NULL);
}

LCC_ERRNO API_FUNC
LCC_stmt_execute(LCC_HANDLE *handle)
{
  LCC_ERRNO rc;
  lcc_stmt *stmt= (lcc_stmt *)handle;

  if (lcc_validate_handle(handle, LCC_STATEMENT))
    return ER_INVALID_HANDLE;

  if (!stmt->execbuf.buf ||
      !stmt->exec_len)
  {
    rc= ER_STMT_NOT_READY;
    goto error;
  }

  if((rc= lcc_io_write(stmt->conn, CMD_STMT_EXECUTE, (char *)stmt->execbuf.buf, stmt->exec_len)))
    goto error;

  return ER_OK;
error:
  return lcc_set_error(&stmt->error, LCC_ERROR_INFO, rc, "HY000", NULL);
}
