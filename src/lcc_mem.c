#include <lcc.h>
#include <lcc_priv.h>
#include <lcc_error.h>

static lcc_mem_block
*lcc_mem_new_block(size_t size)
{
  void *p, *end;
  lcc_mem_block *mem_block= NULL;
  size_t total_size;
  
  total_size= size + sizeof(lcc_mem_block);
  total_size= lcc_align_size(LCC_MEM_ALIGN_SIZE, total_size);

  if (!(p= mem_block= (lcc_mem_block *)malloc(total_size)))
    return NULL;
  end= p + total_size;

  p+= sizeof(lcc_mem_block);
  mem_block->buffer= p;
  mem_block->used_size= 0;
  mem_block->total_size= end - p;
  memset(mem_block->buffer, 0, mem_block->total_size);
  mem_block->next= NULL;

  return mem_block;
}

/**
 * @brief: initializes am internal memory pool
 * @param: prealloc   size of preallocated buffer.
 */
LCC_ERRNO
lcc_mem_init(lcc_mem *mem,
              size_t prealloc)
{
  prealloc= lcc_align_size(LCC_MEM_ALIGN_SIZE, prealloc);
  if (!(mem->block= lcc_mem_new_block(prealloc)))
    return ER_OUT_OF_MEMORY;
  mem->prealloc_size= prealloc;
  mem->in_use= 1;
  return ER_OK;
}

/**
 * @brief: allocates memory from given memory pool
 *
 * @param: mem - memory pool
 * @param: size - size of requested memory
 */
void *lcc_mem_alloc(lcc_mem *mem,
                    size_t size)
{
#define free_size(c) (c)->total_size - (c)->used_size
  lcc_mem_block *found_block= NULL, *current, *last= NULL;
  size_t min= 0xFFFF;

  for (current= mem->block; current; current= current->next)
  {
    if (free_size(current) >= size && free_size(current) - size < min)
    {
      found_block= current;
      min= free_size(current) - size;
    }
    if (!current->next)
      last= current;
  }

  if (!found_block)
  {
    size_t new_size= lcc_align_size(LCC_MEM_ALIGN_SIZE, lcc_MAX(size, mem->prealloc_size));
    found_block= last->next= lcc_mem_new_block(new_size);
  }

  if (found_block)
  {
    void *p= found_block->buffer + found_block->used_size;
    found_block->used_size+= size;
    return p;
  }
  return NULL;
}

/**
 * @brief: closes memory pool and releases all memory
 *
 * @param: mem - memory pool
 */
void lcc_mem_close(lcc_mem *mem)
{
  while (mem->block)
  {
    lcc_mem_block *free_block= mem->block;
    mem->block= mem->block->next;
    free(free_block);
  }
  memset(mem, 0, sizeof(lcc_mem));
  return;
}
