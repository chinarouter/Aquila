#ifndef _SYS_SIGNAL_H
#define _SYS_SIGNAL_H

#include <core/system.h>

struct sigaction;

#include <sys/proc.h>

/* Signal numbers */
#define SIGHUP  1   /* hangup */
#define SIGINT  2   /* interrupt */
#define SIGQUIT 3   /* quit */
#define SIGILL  4   /* illegal instruction (not reset when caught) */
#define SIGTRAP 5   /* trace trap (not reset when caught) */
#define SIGIOT  6   /* IOT instruction */
#define SIGABRT 6   /* used by abort, replace SIGIOT in the future */
#define SIGEMT  7   /* EMT instruction */
#define SIGFPE  8   /* floating point exception */
#define SIGKILL 9   /* kill (cannot be caught or ignored) */
#define SIGBUS  10  /* bus error */
#define SIGSEGV 11  /* segmentation violation */
#define SIGSYS  12  /* bad argument to system call */
#define SIGPIPE 13  /* write on a pipe with no one to read it */
#define SIGALRM 14  /* alarm clock */
#define SIGTERM 15  /* software termination signal from kill */
#define SIGURG  16  /* urgent condition on IO channel */
#define SIGSTOP 17  /* sendable stop signal not from tty */
#define SIGTSTP 18  /* stop signal from tty */
#define SIGCONT 19  /* continue a stopped process */
#define SIGCHLD 20  /* to parent on child stop or exit */
#define SIGCLD  20  /* System V name for SIGCHLD */
#define SIGTTIN 21  /* to readers pgrp upon background tty read */
#define SIGTTOU 22  /* like TTIN for output if (tp->t_local&LTOSTOP) */
#define SIGIO   23  /* input/output possible signal */
#define SIGPOLL SIGIO   /* System V name for SIGIO */
#define SIGWINCH 24 /* window changed */
#define SIGUSR1 25  /* user defined signal 1 */
#define SIGUSR2 26  /* user defined signal 2 */

#define SIG_MAX 26

#define SIGACT_ABORT        1
#define SIGACT_TERMINATE    2
#define SIGACT_IGNORE       3
#define SIGACT_STOP         4
#define SIGACT_CONTINUE     5

/* libc bindings */
#define SIG_DFL ((uintptr_t) 0)  /* Default action */
#define SIG_IGN ((uintptr_t) 1)  /* Ignore action */
#define SIG_ERR ((uintptr_t) -1) /* Error return */

struct sigaction {
    uintptr_t sa_handler;
    sigset_t  sa_mask;
    int       sa_flags;
};

extern int sig_default_action[];

int signal_send(int pid, int sig);
int signal_proc_send(struct proc *proc, int signal);
int signal_pgrp_send(struct pgroup *pg, int signal);

#endif /* ! _SYS_SIGNAL_H */
