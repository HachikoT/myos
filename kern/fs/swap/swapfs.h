#ifndef __KERN_FS_SWAP_SWAPFS_H__
#define __KERN_FS_SWAP_SWAPFS_H__

#include "kern/mm/mem_layout.h"

void swapfs_init(void);
int swapfs_read(swap_entry_t entry, struct page_desc *page);
int swapfs_write(swap_entry_t entry, struct page_desc *page);

#endif /* !__KERN_FS_SWAP_SWAPFS_H__ */
