#include <lcc.h>
#include <lcc_priv.h>
#include <lcc_error.h>

 /* todo: mysql needs a list of pointers where 
    e.g. result->conn will be invalidated;

  Missing functions:

  LCC_result_skip(result, {CURRENT, ALL})
  LCC_result_options(handle, option, ..)
*/

const LCC_COLUMN API_FUNC
*LCC_result_columns(LCC_HANDLE *handle)
{
  if (!handle || handle->type != LCC_RESULT)
    return NULL;
  if (!((lcc_result *)handle)->conn->column_count)
    return NULL;
  return ((lcc_result *)handle)->columns;
}
