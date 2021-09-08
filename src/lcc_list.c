#include <lcc.h>
#include <lcc_priv.h>
#include <lcc_error.h>
#include <stdlib.h>

LCC_ERROR lcc_list_add(LCC_LIST **list, void *data)
{
  if (!*list)
  {
    if (!(*list= calloc(1, sizeof(LCC_LIST))))
      return ER_OUT_OF_MEMORY;
    (*list)->data= data;
    return ER_OK;
  }
  while ((*list)->next)
    *list= (*list)->next;
  if (!((*list)->next= calloc(1, sizeof(LCC_LIST))))
    return ER_OUT_OF_MEMORY;
  (*list)->next->data= data;
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
