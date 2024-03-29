#ifndef __KERN_FS_FS_H__
#define __KERN_FS_FS_H__

#include "kern/sync/sem.h"
#include "kern/fs/file.h"

#define SECT_SIZE 512
#define PAGE_NSECT (PG_SIZE / SECT_SIZE)

#define SWAP_DEV_NO 1
#define DISK0_DEV_NO 2
#define DISK1_DEV_NO 3

void fs_init(void);
void fs_cleanup(void);

struct inode;
struct file;

// 进程的打开文件信息
struct files_struct
{
    struct inode *pwd;     // inode of present working directory
    struct file *fd_array; // opened files array
    int files_count;       // the number of opened files
    semaphore_t files_sem; // lock protect sem
};

#define FILES_STRUCT_BUFSIZE (PG_SIZE - sizeof(struct files_struct))
#define FILES_STRUCT_NENTRY (FILES_STRUCT_BUFSIZE / sizeof(struct file))

void lock_files(struct files_struct *filesp);
void unlock_files(struct files_struct *filesp);

struct files_struct *files_create(void);
void files_destroy(struct files_struct *filesp);
void files_closeall(struct files_struct *filesp);
int dup_fs(struct files_struct *to, struct files_struct *from);

static inline int
files_count(struct files_struct *filesp)
{
    return filesp->files_count;
}

static inline int
files_count_inc(struct files_struct *filesp)
{
    filesp->files_count += 1;
    return filesp->files_count;
}

static inline int
files_count_dec(struct files_struct *filesp)
{
    filesp->files_count -= 1;
    return filesp->files_count;
}

#endif // __KERN_FS_FS_H__
