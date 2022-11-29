#include "kern/mm/default_pmm_manager.h"
#include "kern/mm/memlayout.h"
#include "libs/list.h"

free_area_t free_area;

#define free_list (free_area.free_list)
#define n_free (free_area.n_free)

static void default_init(void)
{
  list_init(&free_list);
  n_free = 0;
}

static void default_mem_map_init(struct Page *base, size_t n)
{
  struct Page *p = base;
  for (; p != base + n; p++)
  {
    p->flags = 0;
    SET_PAGE_PROPERTY(p);
    p->property = 0;
    set_page_ref(p, 0);
    list_add_before(&free_list, &(p->page_link));
  }
  n_free += n;
  // first block
  base->property = n;
}

static struct Page *default_alloc_pages(size_t n)
{
  if (n > n_free)
  {
    return NULL;
  }
  list_entry_t *le, *len;
  le = &free_list;

  while ((le = list_next(le)) != &free_list)
  {
    struct Page *p = le2page(le, page_link);
    if (p->property >= n)
    {
      int i;
      for (i = 0; i < n; i++)
      {
        len = list_next(le);
        struct Page *pp = le2page(le, page_link);
        SET_PAGE_RESERVED(pp);
        CLEAR_PAGE_PROPERTY(pp);
        list_del(le);
        le = len;
      }
      if (p->property > n)
      {
        (le2page(le, page_link))->property = p->property - n;
      }
      CLEAR_PAGE_PROPERTY(p);
      SET_PAGE_RESERVED(p);
      n_free -= n;
      return p;
    }
  }
  return NULL;
}

static void default_free_pages(struct Page *base, size_t n)
{
  list_entry_t *le = &free_list;
  struct Page *p;
  while ((le = list_next(le)) != &free_list)
  {
    p = le2page(le, page_link);
    if (p > base)
    {
      break;
    }
  }
  // list_add_before(le, base->page_link);
  for (p = base; p < base + n; p++)
  {
    list_add_before(le, &(p->page_link));
  }
  base->flags = 0;
  set_page_ref(base, 0);
  CLEAR_PAGE_PROPERTY(base);
  SET_PAGE_PROPERTY(base);
  base->property = n;

  p = le2page(le, page_link);
  if (base + n == p)
  {
    base->property += p->property;
    p->property = 0;
  }
  le = list_prev(&(base->page_link));
  p = le2page(le, page_link);
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
      p = le2page(le, page_link);
    }
  }

  n_free += n;
  return;
}

static size_t default_n_free_pages(void)
{
  return n_free;
}

// 默认物理内存管理器
struct pmm_manager default_pmm_mgr = {
    .name = "default_pmm_manager",
    .init = default_init,
    .mem_map_init = default_mem_map_init,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .n_free_pages = default_n_free_pages,
};
