#include "kern/mm/ff_pmm_manager.h"
#include "kern/mm/mem_layout.h"
#include "kern/debug/assert.h"
#include "libs/list.h"

free_area_t free_area;

#define free_list (free_area.free_list)
#define n_free (free_area.n_free)

static void ff_init(void)
{
  // 初始化空的页链表
  list_init(&free_list);
  n_free = 0;
}

// 初始化n个页描述符到内存管理器
static void ff_mem_map_init(struct page_desc *base, size_t n)
{
  // 将这n个页描述挨个添加到空闲链表末尾
  struct page_desc *p = base;
  for (; p != base + n; p++)
  {
    p->ref = 0;
    p->flags = 0;
    SET_PG_FLAG_BIT(p, PG_PROPERTY);
    p->property = 0;
    list_add_before(&free_list, &(p->page_link));
  }
  n_free += n;
  // 给每一段的第一个页描述符记录上大小
  base->property = n;
}

// 分配连续的n个页描述符出来
static struct page_desc *ff_alloc_pages(size_t n)
{
  assert(n > 0);
  if (n > n_free)
  {
    return NULL;
  }

  list_entry_t *le, *len;
  le = &free_list;

  while ((le = list_next(le)) != &free_list)
  {
    struct page_desc *p = le2page(le);
    if (p->property >= n)
    {
      for (int i = 0; i < n; i++)
      {
        struct page_desc *pp = le2page(le);
        SET_PG_FLAG_BIT(pp, PG_RESERVED);
        CLEAR_PG_FLAG_BIT(pp, PG_PROPERTY);
        len = list_next(le);
        list_del(le);
        le = len;
      }
      if (p->property > n)
      {
        (le2page(le))->property = p->property - n;
      }
      n_free -= n;
      return p;
    }
  }
  return NULL;
}

// 释放n个连续页描述符
static void ff_free_pages(struct page_desc *base, size_t n)
{
  assert(n > 0);
  assert(TEST_PG_FLAG_BIT(base, PG_RESERVED));

  list_entry_t *le = &free_list;
  struct page_desc *p;

  // 找到合适的位置插入
  while ((le = list_next(le)) != &free_list)
  {
    p = le2page(le);
    if (p > base)
    {
      break;
    }
  }

  for (p = base; p < base + n; p++)
  {
    list_add_before(le, &(p->page_link));
  }

  base->flags = 0;
  base->ref = 0;
  CLEAR_PG_FLAG_BIT(base, PG_RESERVED);
  SET_PG_FLAG_BIT(base, PG_PROPERTY);
  base->property = n;

  // 和后面的页描述符连续，那么合并
  p = le2page(le);
  if (base + n == p)
  {
    base->property += p->property;
    p->property = 0;
  }

  // 和前面的页描述符连续，合并
  le = list_prev(&(base->page_link));
  p = le2page(le);
  if (le != &free_list && p == base - 1)
  {
    while (le != &free_list)
    {
      if (p->property)
      {
        p->property += base->property;
        base->property = 0;
        break;
      }
      le = list_prev(le);
      p = le2page(le);
    }
  }

  n_free += n;
  return;
}

static size_t ff_n_free_pages(void)
{
  return n_free;
}

// 默认物理内存管理器
struct pmm_manager g_ff_pmm_mgr = {
    .name = "ff_pmm_manager",
    .init = ff_init,
    .mem_map_init = ff_mem_map_init,
    .alloc_pages = ff_alloc_pages,
    .free_pages = ff_free_pages,
    .n_free_pages = ff_n_free_pages,
};
