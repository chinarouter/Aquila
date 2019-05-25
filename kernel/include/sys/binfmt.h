#ifndef _SYS_BINFMT_H
#define _SYS_BINFMT_H

#include <core/system.h>
#include <fs/vfs.h>
#include <sys/proc.h>

struct binfmt {
    int (*check)(struct inode *inode);
    int (*load)(struct proc *proc, const char *path, struct inode *inode);
};

int binfmt_load(struct proc *proc, const char *path, struct proc **ref);

#endif /* ! _SYS_BINFMT_H */
