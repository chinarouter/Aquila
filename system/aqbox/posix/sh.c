#include <aqbox.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <glob.h>
#include <termios.h>
#include <signal.h>

#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/utsname.h>

#include <sys/ioctl.h>
#include <termios.h>

int flags = 0;

#define F_DEBUG 1

/*
 * Built-in commands
 */

int builtin_cd(int argc, char **argv)
{
    char *path = NULL;
    if (argc == 1) {
        if (!(path = getenv("HOME")))
            path = "/";
    } else if (argc == 2) {
        path = argv[1];
    } else {
        fprintf(stderr, "aqsh: cd: too many arguments\n");
        return -1;
    }

    if (chdir(path)) {
        fprintf(stderr, "aqsh: cd: %s: ", path);
        perror("");
        return errno;
    }

    /* Update environment */
    char buf[512], cwd[512];
    getcwd(cwd, 512);
    snprintf(buf, 512, "PWD=%s", cwd);
    putenv(buf);
    return 0;
}

int builtin_export(int argc, char **argv)
{
    for (int i = 0; i < argc; ++i) {
        putenv(argv[i]);    /* FIXME */
    }

    return 0;
}

int builtin_exit(int argc, char **argv)
{
    exit(0);
}

int builtin_true(int argc, char **argv)
{
    return 0;
}

int builtin_set(int argc, char **argv)
{
    for (int i = 0; i < argc; ++i) {
        if (!strcmp(argv[i], "-x"))
            flags |= F_DEBUG;
    }

    return 0;
}

#if 0
int builtin_glob(int argc, char **argv)
{
    if (argc > 1) {
        glob_t pglob = {.gl_offs = 0};
        int ret = glob(argv[1], 0, NULL, &pglob);
        if (ret) {
            perror("glob");
        }

        for (size_t i = 0; i < pglob.gl_pathc; ++i) {
            printf("%s\n", pglob.gl_pathv[i]);
        }
    }

    return 0;
}
#endif

struct aqsh_command {
    char *name;
    int (*f)(int, char**);
} builtin_commands[] = {    /* Commands must be sorted */
    {"cd", builtin_cd},
    {"exit", builtin_exit},
    {"export", builtin_export},
    {"set", builtin_set},
    {"true", builtin_true},
    //{"glob", builtin_glob},
};

#define BUILTIN_COMMANDS_NR ((sizeof(builtin_commands)/sizeof(builtin_commands[0])))

static int bsearch_helper(const void *a, const void *b)
{
    struct aqsh_command *_a = (struct aqsh_command *) a;
    struct aqsh_command *_b = (struct aqsh_command *) b;
    return strcmp(_a->name, _b->name);
}

int (*aqsh_get_command(char *name))()
{
    /* Perform binary search on commands */
    struct aqsh_command *cmd = (struct aqsh_command *) 
        bsearch(&(struct aqsh_command){.name=name}, builtin_commands, BUILTIN_COMMANDS_NR, sizeof(builtin_commands[0]), bsearch_helper);

    return cmd? cmd->f : NULL;
}

/*
 * Global variables 
 */

extern char **environ;
char *hostname = "aquila";
char *user = "root";
char *pwd = "/";

/***********/
void print_prompt()
{
    int root_prompt = getuid() == 0;
    printf("[%s@%s %s]%c ", user, hostname, pwd, root_prompt? '#' : '$');
    fflush(stdout);
}

int run_prog(char *name, char **argv)
{
    int cld;
    if (cld = fork()) {
        int s, pid;
        do {
            pid = waitpid(cld, &s, 0);
        } while (pid != cld);

        if (WIFSIGNALED(s)) {   /* Terminated due to signal */
            switch (WTERMSIG(s)) {
                case SIGINT:    /* Ignore */
                    break;
                case SIGSEGV:
                    fprintf(stderr, "Segmentation fault\n");
                    break;
                default:
                    fprintf(stderr, "Terminated due to signal %d\n", WTERMSIG(s));
                    break;
            }
        }

    } else {
        int x = execve(name, argv, environ);
        fprintf(stderr, "sh: execve: %s\n", strerror(errno));
        exit(x);
    }
}

enum KEY_ACTION {
    KEY_NULL = 0,       /* NULL */
    CTRL_C = 3,         /* Ctrl-c */
    CTRL_D = 4,         /* Ctrl-d */
    CTRL_F = 6,         /* Ctrl-f */
    CTRL_H = 8,         /* Ctrl-h */
    TAB = 9,            /* Tab */
    CTRL_L = 12,        /* Ctrl+l */
    ENTER = 13,         /* Enter */
    CTRL_Q = 17,        /* Ctrl-q */
    CTRL_S = 19,        /* Ctrl-s */
    CTRL_U = 21,        /* Ctrl-u */
    ESC = 27,           /* Escape */
    BACKSPACE =  127,   /* Backspace */
    /* The following are just soft codes, not really reported by the
     * terminal directly. */
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

int read_key(int fd)
{
    int nread;
    char c, seq[3];
    while ((nread = read(fd,&c,1)) == 0);
    if (nread == -1) exit(1);

    while (1) {
        switch(c) {
        case ESC:    /* escape sequence */
            /* If this is just an ESC, we'll timeout here. */
            if (read(fd,seq,1) == 0) return ESC;
            if (read(fd,seq+1,1) == 0) return ESC;

            /* ESC [ sequences. */
            if (seq[0] == '[') {
                if (seq[1] >= '0' && seq[1] <= '9') {
                    /* Extended escape, read additional byte. */
                    if (read(fd,seq+2,1) == 0) return ESC;
                    if (seq[2] == '~') {
                        switch(seq[1]) {
                        case '3': return DEL_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        }
                    }
                } else {
                    switch(seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                    }
                }
            }

            /* ESC O sequences. */
            else if (seq[0] == 'O') {
                switch(seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
                }
            }
            break;
        default:
            return c;
        }
    }
}

struct termios orig;
char read_buf[1024];

char *read_input()
{
    tcgetattr(0, &orig);
    struct termios new = orig;
    new.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(0, TCSAFLUSH, &new);
    memset(read_buf, 0, 1024);

    size_t off = 0, len = 0;
    char newline = 0;
    while (!newline) {
        int c = read_key(0);
        switch(c) {
        case '\t':  /* Tab character */
            goto skip_echo;
        case '\n':
        case ENTER:
            read_buf[len] = '\0';
            write(1, "\n", 1);
            goto done;
        case CTRL_C:
        case CTRL_Q:
        case CTRL_S:
        case CTRL_F:
            goto skip_echo;
        case BACKSPACE:
        case CTRL_H:
        case DEL_KEY:
            if (off > 0) {
                read_buf[--off] = '\0';
                write(1, "\b \b", 3);
                --len;

                if (off < len) {
                    char seq[20];
                    memmove(read_buf + off, read_buf + off + 1, len - off);
                    printf("\r\033[0J");
                    print_prompt();
                    read_buf[len] = '\0';
                    printf("%s", read_buf);

                    /* XXX */
                    snprintf(seq, 20, "\x1b[%dD", len-off);
                    printf("%s", seq);

                    fflush(stdout);
                }
            }

            goto skip_echo;
        case PAGE_UP:
        case PAGE_DOWN:
            goto skip_echo;
        case ARROW_UP:
        case ARROW_DOWN:
            goto skip_echo;
        case ARROW_LEFT:
            if (off > 0) {
                --off;
                printf("\x1b[D");
                fflush(stdout);
            }
            goto skip_echo;
        case ARROW_RIGHT:
            if (off < len) {
                ++off;
                printf("\x1b[C");
                fflush(stdout);
            }
            goto skip_echo;
        case CTRL_L:
            printf("\033[H\033[2J");    /* VT100/ANSI Clear sequence */
            print_prompt();
            read_buf[len] = '\0';
            printf("%s", read_buf);
            fflush(stdout);
            goto skip_echo;
        case ESC:
            goto skip_echo;
        default:
            if (c < ' ')    /* Non-printable */
                break;

            if (off < len) {
                char seq[20];
                memmove(read_buf + off + 1, read_buf + off, len - off);
                read_buf[off++] = c;
                ++len;

                printf("\r\033[0J");
                print_prompt();
                read_buf[len] = '\0';
                printf("%s", read_buf);

                snprintf(seq, 20, "\x1b[%dD", len-off);
                printf("%s", seq);

                fflush(stdout);
                goto skip_echo;
            } else {
                read_buf[off++] = c;
                ++len;
            }
            break;
        }

        write(1, &c, 1);
skip_echo:
        ;
    }
done:
    tcsetattr(0, TCSAFLUSH, &orig);
    return read_buf;
}

int eval(char *buf)
{
    //fprintf(stderr, "eval(\"%s\")\n", buf);
    if (!strlen(buf))
        return 0;

    char *argv[50];
    int args_i = 0;

    /* replace # with NULL */
    char *hash;
    if ((hash = strchr(buf, '#')))
        *hash = 0;

    char *tok = strtok(buf, " \t\n");

    while (tok) {
        argv[args_i++] = tok;
        tok = strtok(NULL, " \t\n");
    }

    /* return if line is empty */
    if (!args_i)
        return 0;

    argv[args_i] = NULL;

    if (flags & F_DEBUG) {
        fprintf(stderr, "+ ");
        for (int i = 0; i < args_i; ++i) {
            fprintf(stderr, "%s ", argv[i]);
        }
        fprintf(stderr, "\n");
    }

    /* path? */
    if (args_i && (argv[0][0] == '/' || argv[0][0] == '.')) {
        int fd = open(argv[0], O_RDONLY);
        if (fd > 0) {
            close(fd);
            return run_prog(argv[0], argv);
        } else {
            fprintf(stderr, "aqsh: %s: ", argv[0]);
            perror("");
            return errno;
        }
    }

    /* Check if command is built-in */
    int (*f)() = aqsh_get_command(argv[0]);
    if (f) return f(args_i, argv);

    /* Check commands from PATH */
    char *env_path = getenv("PATH");
    if (env_path) {
        char *path = strdup(env_path);
        char *comp = strtok(path, ":");
        char buf[1024];
        do {
            sprintf(buf, "%s/%s", comp, argv[0]);
            int fd = open(buf, O_RDONLY);
            if (fd > 0) {
                close(fd);
                int ret = run_prog(buf, argv);
                free(path);
                return ret;
            }
        } while (comp = strtok(NULL, ":"));
    }

    fprintf(stderr, "aqsh: %s: command not found\n", argv[0]);
    return -1;
}

void sigpass(int s)
{

}

void shell()
{
    signal(2, sigpass);
    setpgid(0, 0);
    pid_t pid = getpid();
    ioctl(0, TIOCSPGRP, &pid);
    char buf[1024];

    for (;;) {
        user = getenv("USER");
        pwd = getenv("PWD");
        print_prompt();
        memset(buf, 0, 1024);
        char *input = read_input();
        //char *input = fgets(buf, 1024, stdin);
        eval(input);
    }
}

void shell_batch(const char *path)
{
    FILE *file = fopen(path, "r");

    if (!file) {
        fprintf(stderr, "aqsh: can not execute %s: ", path);
        perror("");
        return;
    }

    char buf[1024];
    memset(buf, 0, sizeof(buf));

    while (fgets(buf, sizeof(buf), file)) {
        //printf("aqsh: buf: %s\n", buf);
        //write(2, ">> ", 3);
        //write(2, buf, sizeof(buf));
        eval(buf);
    }
}

AQBOX_APPLET(sh)(int argc, char **argv)
{
    if (argc > 1) {
        /* batch */
        for (int i = 1; i < argc; ++i)
            shell_batch(argv[i]);
    } else {
        /* interactive */
        shell();
    }

    return 0;
}
