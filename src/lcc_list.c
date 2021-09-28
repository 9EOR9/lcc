#include <lcc.h>
#include <lcc_priv.h>
#include <lcc_error.h>
#include <stdlib.h>

LCC_ERRNO lcc_list_add(LCC_LIST **list, void *data)
{
  LCC_LIST *last= *list;

  if (!*list)
  {
    if (!(*list= calloc(1, sizeof(LCC_LIST))))
      return ER_OUT_OF_MEMORY;
    (*list)->data= data;
    return ER_OK;
  }

  /* check if we have an empty slot */
  while (last)
  {
    if (!last->data)
    {
      last->data= data;
      return ER_OK;
    }
    if (!last->next)
      break;
    last= last->next;
  }

  if (!(last->next= (LCC_LIST *)calloc(1, sizeof(LCC_LIST))))
    return ER_OUT_OF_MEMORY;
  last->next->data= data;
  return ER_OK;
}

LCC_LIST *lcc_list_find(LCC_LIST *list, void *search, lcc_find_callback func)
{
  if (!func)
    return NULL;

  while (list)
  {
    if (func(list->data, search))
      return list;
    list= list->next;
  }
  return NULL;
}

/**
 * @brief: clears an element from list without deleting it,
 *         next lcc_list_add call will reuse the cleared element
 *
 */
void lcc_list_clear_element(LCC_LIST *list, void *search)
{
  while (list)
  {
    if (list->data == search)
    {
      list->data= NULL;
      return;
    }
  }
}

void lcc_list_delete(LCC_LIST *list, lcc_delete_callback func)
{
  while (list)
  {
    LCC_LIST *tmp= list;
    list= list->next;
    if (func)
      func(tmp->data);
    free(tmp);
  }
}
