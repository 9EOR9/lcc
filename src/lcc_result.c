#include <lcc.h>
#include <lcc_priv.h>
#include <lcc_error.h>

/**
 * @brief: reads metadata for the following result set
 *
 * @param: handle - a result set pointer, which was previously
           allocated by LCC_init_handle
 * 
 * @return: ERR_OK on success, otherwise error code.
 */
static LCC_ERRNO
lcc_result_get_metadata(LCC_HANDLE *handle)
{
  LCC_RESULT *result= (lcc_result *)handle;

  if (!handle || handle->type != LCC_RESULT)
    return ER_INVALID_HANDLE;

  if (!result->conn.column_count)
    return ER_NO_RESULT_AVAILABLE;

  return lcc_read_metadata(result);
}

 /* todo: mysql needs a list of pointers where 
    e.g. result->conn will be invalidated;

  Missing functions:

  LCC_result_columns() returns metadata information for fields
  LCC_result_skip(result, {CURRENT, ALL})
  LCC_result_options(handle, option, ..)

LCC_COLUMN LCC_result_columns(LCC_HANDLE *handle)
{
}
*/
