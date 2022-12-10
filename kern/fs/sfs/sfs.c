#include "kern/fs/sfs/sfs.h"
#include "kern/debug/assert.h"

/*
 * sfs_init - mount sfs on disk0
 *
 * CALL GRAPH:
 *   kern_init-->fs_init-->sfs_init
 */
void sfs_init(void)
{
    int ret;
    if ((ret = sfs_mount("disk0")) != 0)
    {
        panic("failed: sfs: sfs_mount: %e.\n", ret);
    }
}
