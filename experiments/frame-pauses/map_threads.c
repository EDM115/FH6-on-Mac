#define _WIN32_WINNT 0x0a00
#include <windows.h>
#include <tlhelp32.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef LONG (WINAPI *query_thread_fn)(HANDLE, ULONG, void *, ULONG, ULONG *);
typedef HRESULT (WINAPI *thread_description_fn)(HANDLE, PWSTR *);
typedef struct {
    LONG status;
    void *teb;
    HANDLE process_id;
    HANDLE thread_id;
    ULONG_PTR affinity;
    LONG priority;
    LONG base_priority;
} BasicThread;

static void quoted(const char *text)
{
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        if (*p == '"' || *p == '\\') printf("\\%c", *p);
        else if (*p < 32) printf("\\u%04x", *p);
        else putchar(*p);
    }
    putchar('"');
}

static int read_exact(HANDLE process, uintptr_t address, void *buffer, SIZE_T size)
{
    SIZE_T count = 0;
    return ReadProcessMemory(process, (void *)address, buffer, size, &count) && count == size;
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    DWORD pid = strtoul(argv[1], NULL, 0);
    if (!pid) return 2;
    HANDLE process = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return 3;
    char image[32768];
    DWORD length = sizeof(image);
    if (!QueryFullProcessImageNameA(process, 0, image, &length)) return 4;
    const char *base = strrchr(image, '\\');
    if (!base || _stricmp(base + 1, "forzahorizon6.exe")) return 5;
    query_thread_fn query = (query_thread_fn)(void *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread");
    thread_description_fn description = (thread_description_fn)(void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetThreadDescription");
    if (!query) return 6;
    printf("{\"wine_pid\":%lu,\"modules\":[", (unsigned long)pid);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) return 7;
    MODULEENTRY32 module = {0};
    module.dwSize = sizeof(module);
    int first = 1;
    if (Module32First(snapshot, &module)) do {
        if (!first) putchar(',');
        first = 0;
        printf("{\"base\":\"0x%llx\",\"size\":%lu,\"name\":", (unsigned long long)(uintptr_t)module.modBaseAddr, (unsigned long)module.modBaseSize);
        quoted(module.szModule);
        putchar('}');
    } while (Module32Next(snapshot, &module));
    CloseHandle(snapshot);
    printf("],\"threads\":[");
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 8;
    THREADENTRY32 thread = {0};
    thread.dwSize = sizeof(thread);
    first = 1;
    unsigned seen = 0;
    if (Thread32First(snapshot, &thread)) do {
        if (thread.th32OwnerProcessID != pid) continue;
        if (++seen > 512) return 9;
        HANDLE handle = OpenThread(THREAD_QUERY_INFORMATION, FALSE, thread.th32ThreadID);
        if (!handle) continue;
        BasicThread info = {0};
        void *entry = NULL;
        LONG status = query(handle, 0, &info, sizeof(info), NULL);
        LONG entry_status = query(handle, 9, &entry, sizeof(entry), NULL);
        unsigned char tib[0x50] = {0};
        uint64_t data[12] = {0};
        uintptr_t teb = (uintptr_t)info.teb;
        uint64_t self = 0, owner = 0, tid = 0;
        int valid = status >= 0 && read_exact(process, teb, tib, sizeof(tib));
        if (valid) {
            memcpy(&self, tib + 0x30, 8);
            memcpy(&owner, tib + 0x40, 8);
            memcpy(&tid, tib + 0x48, 8);
            valid = self == teb && owner == pid && tid == thread.th32ThreadID;
        }
        // Wine 11 ntdll_thread_data at TEB+0x2f0, pthread_id at +0x3a0
        if (valid) valid = read_exact(process, teb + 0x378, data, sizeof(data));
        char name[1024] = {0};
        PWSTR wide = NULL;
        if (description && SUCCEEDED(description(handle, &wide)) && wide) {
            WideCharToMultiByte(CP_UTF8, 0, wide, -1, name, sizeof(name), NULL, NULL);
            LocalFree(wide);
        }
        if (!first) putchar(',');
        first = 0;
        printf("{\"tid\":%lu,\"query_status\":%ld,\"entry_status\":%ld,\"teb_valid\":%s,\"teb\":\"0x%llx\",\"pthread\":\"%llu\",\"syscall_frame\":\"0x%llx\",\"wine_start\":\"0x%llx\",\"win32_start\":\"0x%llx\",\"name\":", (unsigned long)thread.th32ThreadID, (long)status, (long)entry_status, valid ? "true" : "false", (unsigned long long)teb, (unsigned long long)data[5], (unsigned long long)data[0], (unsigned long long)data[9], (unsigned long long)(uintptr_t)entry);
        quoted(name);
        putchar('}');
        CloseHandle(handle);
    } while (Thread32Next(snapshot, &thread));
    CloseHandle(snapshot);
    CloseHandle(process);
    puts("]}");
    return 0;
}
