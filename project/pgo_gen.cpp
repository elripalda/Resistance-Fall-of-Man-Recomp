/* PGO training build only (PORT_PGO=gen; not compiled otherwise).
 * The profile counters live in memory and reach LLVM_PROFILE_FILE only on an
 * orderly exit, which the game does not always take (window close ->
 * TerminateProcess paths). Write them every PORT_PGO_DUMP_S seconds
 * (default 30) and at exit; each write replaces the file with the counters
 * so far. Self-starting, so main.cpp is unchanged. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

extern "C" int __llvm_profile_write_file(void);
extern "C" void __llvm_profile_set_filename(const char*);

static DWORD WINAPI port_pgo_dumper(LPVOID p)
{
    SetThreadDescription(GetCurrentThread(), L"pgo monitor");
    const DWORD ms = (DWORD)(uintptr_t)p * 1000;
    for (;;) { Sleep(ms); __llvm_profile_write_file(); }
}

static void port_pgo_atexit(void) { __llvm_profile_write_file(); }

static int port_pgo_start(void)
{
    /* the profile runtime read the variable before main(): set the name directly */
    if (!getenv("LLVM_PROFILE_FILE")) { _putenv_s("LLVM_PROFILE_FILE", "pgo\\game-%p.profraw");
                                        __llvm_profile_set_filename("pgo\\game-%p.profraw"); }
    CreateDirectoryA("pgo", NULL);
    const char* e = getenv("PORT_PGO_DUMP_S");
    const int s = e && atoi(e) > 0 ? atoi(e) : 30;
    atexit(port_pgo_atexit);
    HANDLE h = CreateThread(NULL, 0, port_pgo_dumper, (LPVOID)(uintptr_t)s, 0, NULL);
    if (h) CloseHandle(h);
    fprintf(stderr, "[pgo] instrumented build: profile -> %s every %d s and at exit\n", getenv("LLVM_PROFILE_FILE"), s);
    return 0;
}

static const int s_port_pgo_started = port_pgo_start();
