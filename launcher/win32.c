/* The Windows side of the launcher (win32.h): what main.c, cfg.c and cloud.c do with fork/exec, pipes, zenity and curl
 * on Linux. Only the Windows build compiles this file. */
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commdlg.h>
#include <winhttp.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "win32.h"

/* ---- UTF-8 <-> UTF-16 ---- */
static wchar_t *wide(const char *s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) return NULL;
    wchar_t *w = malloc((size_t)n * sizeof(wchar_t));
    if (w && MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n) <= 0) {
        free(w);
        return NULL;
    }
    return w;
}

static int narrow(const wchar_t *w, char *out, size_t n) {
    if (n == 0) return -1;
    int r = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)n, NULL, NULL);
    if (r <= 0) {
        out[0] = 0;
        return -1;
    }
    return 0;
}

int w32_rename_over(const char *from, const char *to) {
    wchar_t *wf = wide(from), *wt = wide(to);
    int ok = wf && wt && MoveFileExW(wf, wt, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    free(wf);
    free(wt);
    return ok ? 0 : -1;
}

int w32_exe_path(char *out, size_t n) {
    wchar_t w[MAX_PATH * 4];
    DWORD r = GetModuleFileNameW(NULL, w, (DWORD)(sizeof(w) / sizeof(w[0])));
    if (r == 0 || r >= sizeof(w) / sizeof(w[0])) return -1;
    return narrow(w, out, n);
}

int w32_pick_disc(const char *start_dir, char *out, size_t n) {
    wchar_t file[MAX_PATH * 4] = L"";
    wchar_t *dir = start_dir && start_dir[0] ? wide(start_dir) : NULL;
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = L"PS1 disc image (*.cue, *.bin)\0*.cue;*.bin\0All files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = (DWORD)(sizeof(file) / sizeof(file[0]));
    ofn.lpstrInitialDir = dir;
    ofn.lpstrTitle = L"Select Digimon World 3 (.cue / .bin)";
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    BOOL ok = GetOpenFileNameW(&ofn);
    free(dir);
    if (!ok || !file[0]) return -1;
    return narrow(file, out, n);
}

/* ---- the game's process ---- */

/* One argument as the UCRT's command-line parser reads it back (CommandLineToArgvW's rules): quoted when it has a
 * space, a tab or a quote; backslashes doubled only before a quote. */
static void append_arg(char **buf, size_t *len, size_t *cap, const char *a) {
    size_t need = *len + strlen(a) * 2 + 4;
    if (need > *cap) {
        while (*cap < need) *cap = *cap ? *cap * 2 : 256;
        *buf = realloc(*buf, *cap);
    }
    char *o = *buf + *len;
    if (*len) *o++ = ' ';
    if (a[0] && !strpbrk(a, " \t\"")) {
        size_t l = strlen(a);
        memcpy(o, a, l);
        o += l;
    } else {
        *o++ = '"';
        for (const char *s = a;; s++) {
            size_t bs = 0;
            while (*s == '\\') { bs++; s++; }
            if (!*s) {
                for (size_t i = 0; i < bs * 2; i++) *o++ = '\\';
                break;
            }
            if (*s == '"') {
                for (size_t i = 0; i < bs * 2 + 1; i++) *o++ = '\\';
            } else {
                for (size_t i = 0; i < bs; i++) *o++ = '\\';
            }
            *o++ = *s;
        }
        *o++ = '"';
    }
    *o = 0;
    *len = (size_t)(o - *buf);
}

/* The CRT's own way of handing open file descriptors to a child (what _spawnv does, and libuv's process-stdio.c):
 * STARTUPINFO.lpReserved2 holds the count, one flag byte per descriptor, then one HANDLE per descriptor (unaligned).
 * The child's UCRT opens its descriptors 0..count-1 from it at start, so the runtime's write(DW3_STATUS_FD) reaches the
 * status pipe as on Linux. */
#define CRT_FOPEN 0x01
#define CRT_FPIPE 0x08
#define CRT_FDEV 0x40

int w32_spawn(W32Proc *p, const char *const *argv, const char *cwd, int status_fd) {
    memset(p, 0, sizeof(*p));
    if (status_fd < 3 || status_fd > 32) return -1;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE out_rd = NULL, out_wr = NULL, st_rd = NULL, st_wr = NULL, nul = INVALID_HANDLE_VALUE;
    int rc = -1;
    char *cmd = NULL;
    size_t len = 0, cap = 0;
    wchar_t *wapp = NULL, *wcmd = NULL, *wcwd = NULL;
    unsigned char *crt = NULL;

    if (!CreatePipe(&out_rd, &out_wr, &sa, 1 << 20) || !CreatePipe(&st_rd, &st_wr, &sa, 0)) goto done;
    /* The game's output pipe is 1 MB, not the default 4 KB: the game writes its log unbuffered (HEAVY logs are KBs per
     * frame) and the launcher reads every 50 ms; a full pipe blocks the game's WriteFile, which is a stalled frame. */
    /* Only the write ends go to the child: the launcher keeps the read ends. */
    SetHandleInformation(out_rd, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(st_rd, HANDLE_FLAG_INHERIT, 0);
    nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);

    int count = status_fd + 1;
    size_t crt_size = sizeof(int) + (size_t)count + (size_t)count * sizeof(HANDLE);
    crt = calloc(1, crt_size);
    if (!crt) goto done;
    memcpy(crt, &count, sizeof(int));
    unsigned char *flags = crt + sizeof(int);
    unsigned char *handles = flags + count;
    for (int fd = 0; fd < count; fd++) {
        HANDLE h = INVALID_HANDLE_VALUE;
        unsigned char f = 0;
        if (fd == 0 && nul != INVALID_HANDLE_VALUE) { h = nul; f = CRT_FOPEN | CRT_FDEV; }
        else if (fd == 1 || fd == 2) { h = out_wr; f = CRT_FOPEN | CRT_FPIPE; }
        else if (fd == status_fd) { h = st_wr; f = CRT_FOPEN | CRT_FPIPE; }
        flags[fd] = f;
        memcpy(handles + (size_t)fd * sizeof(HANDLE), &h, sizeof(HANDLE));
    }

    for (int i = 0; argv[i]; i++) append_arg(&cmd, &len, &cap, argv[i]);
    if (!cmd || !(wapp = wide(argv[0])) || !(wcmd = wide(cmd)) || (cwd && !(wcwd = wide(cwd)))) goto done;

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul;
    si.hStdOutput = out_wr;
    si.hStdError = out_wr;
    si.cbReserved2 = (WORD)crt_size;
    si.lpReserved2 = crt;
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(wapp, wcmd, NULL, NULL, TRUE, 0, NULL, wcwd, &si, &pi)) goto done;
    CloseHandle(pi.hThread);
    p->process = pi.hProcess;
    p->pid = pi.dwProcessId;
    p->out = out_rd;
    p->status = st_rd;
    out_rd = st_rd = NULL;
    rc = 0;
done:
    /* The child holds its own copies of the write ends now: closing ours makes its end the pipes' end. */
    if (out_wr) CloseHandle(out_wr);
    if (st_wr) CloseHandle(st_wr);
    if (out_rd) CloseHandle(out_rd);
    if (st_rd) CloseHandle(st_rd);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    free(crt);
    free(cmd);
    free(wapp);
    free(wcmd);
    free(wcwd);
    return rc;
}

long w32_read(void **pipe, char *buf, size_t n) {
    if (!*pipe) return -1;
    DWORD avail = 0, got = 0;
    if (!PeekNamedPipe((HANDLE)*pipe, NULL, 0, NULL, &avail, NULL)) {
        CloseHandle((HANDLE)*pipe);
        *pipe = NULL;
        return -1;
    }
    if (avail == 0) return 0;
    if (avail > n) avail = (DWORD)n;
    if (!ReadFile((HANDLE)*pipe, buf, avail, &got, NULL)) {
        CloseHandle((HANDLE)*pipe);
        *pipe = NULL;
        return -1;
    }
    return (long)got;
}

void w32_wait(const W32Proc *p, int timeout_ms) {
    /* An anonymous pipe cannot be waited on: look at it every 50 ms, and stop at the process's end. */
    for (int waited = 0; waited < timeout_ms; waited += 50) {
        DWORD avail = 0;
        if (p->out && PeekNamedPipe((HANDLE)p->out, NULL, 0, NULL, &avail, NULL) && avail > 0) return;
        if (!p->process || WaitForSingleObject((HANDLE)p->process, 50) == WAIT_OBJECT_0) return;
    }
}

int w32_exited(const W32Proc *p, unsigned long *code) {
    if (!p->process) return 1;
    if (WaitForSingleObject((HANDLE)p->process, 0) != WAIT_OBJECT_0) return 0;
    DWORD c = 1;
    GetExitCodeProcess((HANDLE)p->process, &c);
    if (code) *code = c;
    return 1;
}

static BOOL CALLBACK window_of(HWND w, LPARAM lp) {
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (pid == ((DWORD *)lp)[0] && IsWindowVisible(w)) {
        ((DWORD *)lp)[1] = 1;
        return FALSE;
    }
    return TRUE;
}

int w32_window_up(const W32Proc *p) {
    DWORD state[2] = {p->pid, 0};
    EnumWindows(window_of, (LPARAM)state);
    return state[1] != 0;
}

void w32_close_pipe(void **pipe) {
    if (*pipe) CloseHandle((HANDLE)*pipe);
    *pipe = NULL;
}

typedef struct {
    int (*fn)(void *);
    void *arg;
} ThreadStart;

static DWORD WINAPI thread_main(LPVOID p) {
    ThreadStart ts = *(ThreadStart *)p;
    free(p);
    /* Background work (the periodic cloud push) must never take the game's CPU: on a one-core PC it ran at the game's
     * own priority, a TLS handshake and a hash in the middle of a frame. */
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    return (DWORD)ts.fn(ts.arg);
}

void *w32_thread(int (*fn)(void *), void *arg) {
    ThreadStart *ts = malloc(sizeof(*ts));
    if (!ts) return NULL;
    ts->fn = fn;
    ts->arg = arg;
    HANDLE t = CreateThread(NULL, 0, thread_main, ts, 0, NULL);
    if (!t) free(ts);
    return t;
}

int w32_thread_end(void *thread, int wait, int *rc) {
    if (WaitForSingleObject((HANDLE)thread, wait ? INFINITE : 0) != WAIT_OBJECT_0) return 0;
    DWORD code = 1;
    GetExitCodeThread((HANDLE)thread, &code);
    CloseHandle((HANDLE)thread);
    *rc = (int)code;
    return 1;
}

void w32_proc_close(W32Proc *p) {
    if (p->out) CloseHandle((HANDLE)p->out);
    if (p->status) CloseHandle((HANDLE)p->status);
    if (p->process) CloseHandle((HANDLE)p->process);
    memset(p, 0, sizeof(*p));
}

/* ---- HTTP ---- */
int w32_http_request(const char *method, const char *url, const char *headers, const void *body, size_t body_len,
                     int *status_out, char **body_out, size_t *body_len_out, char *resp_headers, size_t resp_headers_n) {
    *status_out = 0;
    *body_out = NULL;
    *body_len_out = 0;
    if (resp_headers_n) resp_headers[0] = 0;
    int rc = -1;
    HINTERNET ses = NULL, con = NULL, req = NULL;
    wchar_t *wurl = wide(url), *wmethod = wide(method), *whdr = NULL;
    wchar_t host[256], path[2048];
    URL_COMPONENTSW uc;
    memset(&uc, 0, sizeof(uc));
    uc.dwStructSize = sizeof(uc);
    uc.lpszHostName = host;
    uc.dwHostNameLength = (DWORD)(sizeof(host) / sizeof(host[0]));
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = (DWORD)(sizeof(path) / sizeof(path[0]));
    if (!wurl || !wmethod || !WinHttpCrackUrl(wurl, 0, 0, &uc)) goto done;
    /* WinHttpCrackUrl, asked only for the path, already returns it with the query ("/pull?meta=1"): the request wants
     * exactly that. Appending the query again made "?meta=1?meta=1", which the worker did not read as the metadata
     * request, so it sent the whole card and the sync never saw the cloud's rev or sha. */

    ses = WinHttpOpen(L"dw3-launcher", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) goto done;
    /* curl's limits on Linux (cloud.c): 4 s to connect, 12 s in all. */
    WinHttpSetTimeouts(ses, 4000, 4000, 12000, 12000);
    con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) goto done;
    req = WinHttpOpenRequest(con, wmethod, path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                             uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!req) goto done;
    if (headers && headers[0]) {
        /* "Name: value\n" lines as WinHTTP wants them: CRLF between. */
        size_t hl = strlen(headers);
        char *crlf = malloc(hl * 2 + 1);
        if (!crlf) goto done;
        size_t j = 0;
        for (size_t i = 0; i < hl; i++) {
            if (headers[i] == '\n') crlf[j++] = '\r';
            crlf[j++] = headers[i];
        }
        crlf[j] = 0;
        whdr = wide(crlf);
        free(crlf);
        if (!whdr || !WinHttpAddRequestHeaders(req, whdr, (DWORD)-1L, WINHTTP_ADDREQ_FLAG_ADD)) goto done;
    }
    if (!WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)body, (DWORD)body_len, (DWORD)body_len, 0) ||
        !WinHttpReceiveResponse(req, NULL)) {
        goto done;
    }
    DWORD status = 0, sz = sizeof(status);
    if (!WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                             &status, &sz, WINHTTP_NO_HEADER_INDEX)) {
        goto done;
    }
    if (resp_headers_n) {
        DWORD hsz = 0;
        WinHttpQueryHeaders(req, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, NULL, &hsz,
                            WINHTTP_NO_HEADER_INDEX);
        wchar_t *wh = hsz ? malloc(hsz + sizeof(wchar_t)) : NULL;
        if (wh && WinHttpQueryHeaders(req, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, wh, &hsz,
                                      WINHTTP_NO_HEADER_INDEX)) {
            narrow(wh, resp_headers, resp_headers_n);
        }
        free(wh);
    }
    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    if (!buf) goto done;
    for (;;) {
        DWORD avail = 0, got = 0;
        if (!WinHttpQueryDataAvailable(req, &avail)) { free(buf); goto done; }
        if (avail == 0) break;
        if (len + avail + 1 > cap) {
            if (len + avail + 1 > 4u * 1024 * 1024) { free(buf); goto done; }
            while (len + avail + 1 > cap) cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); goto done; }
            buf = nb;
        }
        if (!WinHttpReadData(req, buf + len, avail, &got)) { free(buf); goto done; }
        if (got == 0) break;
        len += got;
    }
    buf[len] = 0;
    *status_out = (int)status;
    *body_out = buf;
    *body_len_out = len;
    rc = 0;
done:
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    if (ses) WinHttpCloseHandle(ses);
    free(wurl);
    free(wmethod);
    free(whdr);
    return rc;
}
#endif
