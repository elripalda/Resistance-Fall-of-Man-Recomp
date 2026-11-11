/* Seamless level switch (included by stubs.cpp, Windows only).
 *
 * exitspawn ends this process and starts the other lifted executable, so the
 * game window would close and a new one open on every level load. When
 * RFoMSwitch.exe is present, the exiting process starts it to hold a black
 * cover over the window; the new process signals a named event once its own
 * window has presented frames, and the cover hands it the foreground.
 *
 *   RFOM_SEAMLESS=0   old behaviour (window closes and reopens)
 *   RFOM_SEAMLESS=1   use the cover also under Wine/Proton (off there by default)
 */
#ifdef _WIN32
extern "C" void* g_kbm_hwnd;                 /* cellPad.c: the game window */
extern "C" unsigned cellGcm_flip_request_count(void);

static bool rfom_under_wine()
{
    HMODULE nt = GetModuleHandleA("ntdll.dll");
    return nt && GetProcAddress(nt, "wine_get_version") != NULL;
}

static std::string rfom_switch_helper(const std::string& root)
{
    char exe[MAX_PATH]; GetModuleFileNameA(NULL, exe, MAX_PATH);
    std::string dir(exe); dir = dir.substr(0, dir.find_last_of("\\/"));
    const std::string cands[] = { dir + "\\RFoMSwitch.exe", root + "\\build\\RFoMSwitch.exe",
                                  root + "\\RFoMSwitch.exe" };
    for (const auto& c : cands)
        if (GetFileAttributesA(c.c_str()) != INVALID_FILE_ATTRIBUTES) return c;
    return std::string();
}

/* Starts target with the cover helper. Returns false (nothing started) when
 * the cover is off or unavailable, so the caller falls back to a plain start. */
static bool rfom_seamless_spawn(const std::string& target_exe, std::vector<char>& cmdline,
                                const std::string& root, STARTUPINFOA* si)
{
    const char* se = getenv("RFOM_SEAMLESS");
    if (se && *se == '0') return false;
    if (!(se && *se == '1') && rfom_under_wine()) return false;
    HWND old_hwnd = (HWND)g_kbm_hwnd;
    if (!old_hwnd || !IsWindowVisible(old_hwnd)) return false;
    std::string helper = rfom_switch_helper(root);
    if (helper.empty()) return false;

    char ev_name[96];
    snprintf(ev_name, sizeof ev_name, "Local\\RFoMSwitch_%lu_%llu", GetCurrentProcessId(),
             (unsigned long long)GetTickCount64());
    HANDLE ev = CreateEventA(NULL, TRUE, FALSE, ev_name);
    if (!ev) return false;

    RECT wr;
    if (GetWindowRect(old_hwnd, &wr)) {
        char pos[48]; snprintf(pos, sizeof pos, "%ld,%ld", wr.left, wr.top);
        SetEnvironmentVariableA("RFOM_PREV_WINDOW", pos);
    }
    SetEnvironmentVariableA("RFOM_COVER_EVENT", ev_name);

    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof pi);
    if (!CreateProcessA(target_exe.c_str(), cmdline.data(), NULL, NULL, TRUE, CREATE_SUSPENDED, NULL,
                        root.c_str(), si, &pi)) {
        SetEnvironmentVariableA("RFOM_COVER_EVENT", NULL);
        SetEnvironmentVariableA("RFOM_PREV_WINDOW", NULL);
        CloseHandle(ev);
        return false;
    }

    char hcmd[MAX_PATH + 128];
    snprintf(hcmd, sizeof hcmd, "\"%s\" 0x%llX %lu %s", helper.c_str(),
             (unsigned long long)(ULONG_PTR)old_hwnd, pi.dwProcessId, ev_name);
    STARTUPINFOA hsi; ZeroMemory(&hsi, sizeof hsi); hsi.cb = sizeof hsi;
    PROCESS_INFORMATION hpi; ZeroMemory(&hpi, sizeof hpi);
    if (CreateProcessA(helper.c_str(), hcmd, NULL, NULL, FALSE, 0, NULL, root.c_str(), &hsi, &hpi)) {
        AllowSetForegroundWindow(hpi.dwProcessId);
        WaitForInputIdle(hpi.hProcess, 3000);   /* the cover is up before this window goes */
        CloseHandle(hpi.hThread); CloseHandle(hpi.hProcess);
        fprintf(stderr, "[exitspawn] cover window up (%s)\n", helper.c_str());
    } else {
        fprintf(stderr, "[exitspawn] cover helper failed to start (error %lu)\n", GetLastError());
    }
    AllowSetForegroundWindow(pi.dwProcessId);
    ResumeThread(pi.hThread);
    fprintf(stderr, "[exitspawn] started %s\n", target_exe.c_str());
    fflush(stderr);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    /* ev stays open until ExitProcess; the helper holds it after that */
    return true;
}

/* New process side: once the window exists and has presented a couple of
 * frames, release the cover. In windowed mode the window first moves to where
 * the previous one was. */
static DWORD WINAPI rfom_cover_release(LPVOID)
{
    char ev_name[96] = {};
    if (!GetEnvironmentVariableA("RFOM_COVER_EVENT", ev_name, sizeof ev_name)) return 0;
    char pos[48] = {};
    GetEnvironmentVariableA("RFOM_PREV_WINDOW", pos, sizeof pos);
    /* not passed on to a later exitspawn */
    SetEnvironmentVariableA("RFOM_COVER_EVENT", NULL);
    SetEnvironmentVariableA("RFOM_PREV_WINDOW", NULL);

    const ULONGLONG start = GetTickCount64();
    while (!g_kbm_hwnd && GetTickCount64() - start < 90000) Sleep(10);
    HWND h = (HWND)g_kbm_hwnd;
    if (h && pos[0] && (GetWindowLongPtrA(h, GWL_STYLE) & WS_CAPTION)) {
        long x = 0, y = 0;
        if (sscanf(pos, "%ld,%ld", &x, &y) == 2)
            SetWindowPos(h, NULL, (int)x, (int)y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    const unsigned f0 = cellGcm_flip_request_count();
    while (cellGcm_flip_request_count() < f0 + 2 && GetTickCount64() - start < 90000) Sleep(10);
    Sleep(100);
    if (HANDLE ev = CreateEventA(NULL, TRUE, FALSE, ev_name)) {   /* opens the helper's event */
        SetEvent(ev);
        CloseHandle(ev);
    }
    fprintf(stderr, "[exitspawn] cover released after %llu ms\n",
            (unsigned long long)(GetTickCount64() - start));
    return 0;
}

static void rfom_cover_release_start()
{
    if (GetEnvironmentVariableA("RFOM_COVER_EVENT", NULL, 0))
        CreateThread(NULL, 0, rfom_cover_release, NULL, 0, NULL);
}
#endif
