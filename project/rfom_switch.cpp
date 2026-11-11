/* RFoMSwitch.exe: covers the game window while the port switches between
 * ResistanceFoM.exe (front end) and ResistanceFoMGame.exe (levels).
 *
 * The two programs are lifted separately, so a level load ends one process
 * and starts the other. The exiting process starts this helper first:
 *
 *   RFoMSwitch.exe <old hwnd> <new pid> <ready event name>
 *
 * It puts a black window over the old game window, keeps it there until the
 * new process signals the event (its window has presented a few frames) or
 * exits, hands the foreground to the new window and quits.
 *
 * Build: clang-cl /O2 project\rfom_switch.cpp /Fe:build\RFoMSwitch.exe
 *        /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib
 */
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

static DWORD g_new_pid;
static HWND g_found;

static BOOL CALLBACK find_game_window(HWND h, LPARAM)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    if (pid != g_new_pid || !IsWindowVisible(h)) return TRUE;
    char cls[64] = {};
    GetClassNameA(h, cls, sizeof cls);
    if (strcmp(cls, "ps3recomp_d3d12") && strcmp(cls, "ps3recomp_vulkan")) return TRUE;
    g_found = h;
    return FALSE;
}

static LRESULT CALLBACK cover_proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_SETCURSOR: SetCursor(NULL); return TRUE;
    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(h, &rc);
        FillRect((HDC)w, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
        return 1;
    }
    case WM_CLOSE: return 0;           /* closes only when the switch is done */
    }
    return DefWindowProcA(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int)
{
    if (__argc < 4) return 1;
    HWND old_hwnd = (HWND)(ULONG_PTR)_strtoui64(__argv[1], NULL, 0);
    g_new_pid = (DWORD)strtoul(__argv[2], NULL, 0);
    const char* ev_name = __argv[3];

    typedef BOOL(WINAPI * SetDpiCtx)(HANDLE);
    if (SetDpiCtx f = (SetDpiCtx)GetProcAddress(GetModuleHandleA("user32.dll"), "SetProcessDpiAwarenessContext"))
        f((HANDLE)-4);                 /* per-monitor v2: cover rect in real pixels */
    else
        SetProcessDPIAware();

    RECT rc;
    if (!old_hwnd || !IsWindow(old_hwnd) || !GetWindowRect(old_hwnd, &rc) || rc.right <= rc.left) {
        HMONITOR mon = MonitorFromWindow(old_hwnd, MONITOR_DEFAULTTOPRIMARY);
        MONITORINFO mi = { sizeof mi };
        GetMonitorInfoA(mon, &mi);
        rc = mi.rcMonitor;
    }

    /* open-or-create: the event survives even if the old process is already gone */
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, ev_name);
    HANDLE proc = OpenProcess(SYNCHRONIZE, FALSE, g_new_pid);

    WNDCLASSA wc = {};
    wc.lpfnWndProc = cover_proc;
    wc.hInstance = inst;
    wc.lpszClassName = "rfom_switch_cover";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hIcon = LoadIconA(GetModuleHandleA(NULL), MAKEINTRESOURCEA(1));
    RegisterClassA(&wc);
    HWND cover = CreateWindowExA(WS_EX_TOPMOST, "rfom_switch_cover", "Resistance: Fall of Man", WS_POPUP,
                                 rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, inst, NULL);
    if (!cover) return 1;
    ShowWindow(cover, SW_SHOW);
    UpdateWindow(cover);
    SetForegroundWindow(cover);
    AllowSetForegroundWindow(ASFW_ANY);

    HANDLE waits[2]; DWORD nw = 0;
    if (ev) waits[nw++] = ev;
    if (proc) waits[nw++] = proc;
    const ULONGLONG deadline = GetTickCount64() + 120000;
    for (;;) {
        ULONGLONG now = GetTickCount64();
        if (now >= deadline) break;
        DWORD left = (DWORD)(deadline - now);
        DWORD r = nw ? MsgWaitForMultipleObjects(nw, waits, FALSE, left, QS_ALLINPUT)
                     : MsgWaitForMultipleObjects(0, NULL, FALSE, left > 200 ? 200 : left, QS_ALLINPUT);
        if (nw && r < WAIT_OBJECT_0 + nw) break;
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    }

    EnumWindows(find_game_window, 0);
    if (g_found) {
        SetForegroundWindow(g_found);
        SetWindowPos(g_found, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }
    DestroyWindow(cover);
    if (ev) CloseHandle(ev);
    if (proc) CloseHandle(proc);
    return 0;
}
