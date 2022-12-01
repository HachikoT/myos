#include "kern/fs/swapfs.h"
#include "kern/fs/fs.h"
#include "kern/mm/mem_layout.h"
#include "kern/driver/ide.h"
#include "kern/debug/assert.h"
#include "kern/mm/swap.h"
#include "kern/mm/pmm.h"

void swapfs_init(void)
{
    static_assert((PG_SIZE % SECT_SIZE) == 0);
    if (!ide_device_valid(SWAP_DEV_NO))
    {
        panic("swap fs isn't available.\n");
    }
    max_swap_offset = ide_device_size(SWAP_DEV_NO) / (PG_SIZE / SECT_SIZE);
}

int swapfs_read(swap_entry_t entry, struct page_desc *page)
{
    return ide_read_secs(SWAP_DEV_NO, swap_offset(entry) * PAGE_NSECT, page2kva(page), PAGE_NSECT);
}

int swapfs_write(swap_entry_t entry, struct page_desc *page)
{
    return ide_write_secs(SWAP_DEV_NO, swap_offset(entry) * PAGE_NSECT, page2kva(page), PAGE_NSECT);
}
