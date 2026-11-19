#ifndef DW3_WIN32_H
#define DW3_WIN32_H
/* The Windows side of what the launcher does with POSIX calls on Linux (win32.c): the game as a child process, the
 * system's file dialog, HTTP for the cloud sync. Strings are UTF-8 throughout. */
#ifdef _WIN32
#include <stddef.h>

/* This program's own path. 0 on success. */
int w32_exe_path(char *out, size_t n);

/* The system's Open dialog for a .cue / .bin, starting in start_dir (or NULL). 0 when a file was picked. */
int w32_pick_disc(const char *start_dir, char *out, size_t n);

/* The game as a child process: its stdout and stderr on one pipe (out), and the loading status the runtime writes to
 * its CRT file descriptor `status_fd` (psxstack/runtime/main.c status_emit: DW3_STATUS_FD) on another. */
typedef struct W32Proc {
    void *process;
    void *out;    /* read end of stdout + stderr, NULL once closed */
    void *status; /* read end of the status descriptor, NULL once closed */
    unsigned long pid;
} W32Proc;

/* Starts argv[0] with argv in `cwd` (the environment is this process's). 0 on success. */
int w32_spawn(W32Proc *p, const char *const *argv, const char *cwd, int status_fd);
/* What a pipe has now, without waiting: >0 bytes read, 0 nothing yet, -1 the other end closed (the pipe is closed and
 * *pipe set to NULL). */
long w32_read(void **pipe, char *buf, size_t n);
/* Waits up to timeout_ms for data on the pipe or the process's end. */
void w32_wait(const W32Proc *p, int timeout_ms);
/* 1 when the process has ended (*code its exit code), 0 while it runs. */
int w32_exited(const W32Proc *p, unsigned long *code);
/* 1 when the process shows a visible top-level window. */
int w32_window_up(const W32Proc *p);
void w32_proc_close(W32Proc *p);
void w32_close_pipe(void **pipe);

/* A thread running fn(arg): its handle, or NULL. w32_thread_end: 1 once it has returned (*rc its value; the handle is
 * closed then), 0 while it runs; `wait` waits for it. */
void *w32_thread(int (*fn)(void *), void *arg);
int w32_thread_end(void *thread, int wait, int *rc);

/* One HTTP request (WinHTTP), in the shape of android_http_request (cloud.c): headers as "Name: value\n" lines; the
 * response's body malloc'ed into *body_out (the caller frees it) and its header lines into resp_headers. 0 when a
 * response arrived, whatever its status; -1 when none did. */
int w32_http_request(const char *method, const char *url, const char *headers, const void *body, size_t body_len,
                     int *status_out, char **body_out, size_t *body_len_out, char *resp_headers, size_t resp_headers_n);
#endif
#endif
