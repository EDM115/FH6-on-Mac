#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winternl.h>
#include <psapi.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    SIZE_T PeakVirtualSize, VirtualSize;
    ULONG PageFaultCount;
    SIZE_T PeakWorkingSetSize, WorkingSetSize;
    SIZE_T QuotaPeakPagedPoolUsage, QuotaPagedPoolUsage;
    SIZE_T QuotaPeakNonPagedPoolUsage, QuotaNonPagedPoolUsage;
    SIZE_T PagefileUsage, PeakPagefileUsage, PrivateUsage;
} ProbeVmCountersEx;
typedef NTSTATUS (NTAPI *QueryProcess)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);

static int snapshot(const char *stage)
{
    PROCESS_MEMORY_COUNTERS_EX p = {.cb = sizeof(p)};
    MEMORYSTATUSEX m = {.dwLength = sizeof(m)};
    ProbeVmCountersEx vm = {0};
    QueryProcess query = (QueryProcess)(uintptr_t)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationProcess");
    if (!query || query(GetCurrentProcess(), ProcessVmCounters, &vm, sizeof(vm), NULL) != 0) return 0;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&p, sizeof(p)) || !GlobalMemoryStatusEx(&m)) return 0;
    printf("SNAPSHOT stage=%s private=%llu pagefile=%llu peak_pagefile=%llu virtual_size=%llu working_set=%llu psapi_pagefile=%llu psapi_cb=%lu physical_total=%llu physical_available=%llu\n", stage, (unsigned long long)vm.PrivateUsage, (unsigned long long)vm.PagefileUsage, (unsigned long long)vm.PeakPagefileUsage, (unsigned long long)vm.VirtualSize, (unsigned long long)vm.WorkingSetSize, (unsigned long long)p.PagefileUsage, (unsigned long)p.cb, (unsigned long long)m.ullTotalPhys, (unsigned long long)m.ullAvailPhys);
    fflush(stdout);
    return 1;
}

int main(void)
{
    const SIZE_T reserve_size = (SIZE_T)2 * 1024 * 1024 * 1024;
    const SIZE_T commit_size = (SIZE_T)256 * 1024 * 1024;
    if (!snapshot("baseline")) return 1;
    unsigned char *p = VirtualAlloc(NULL, reserve_size, MEM_RESERVE, PAGE_READWRITE);
    if (!p || !snapshot("reserved_2g")) return 2;
    if (!VirtualAlloc(p, commit_size, MEM_COMMIT, PAGE_READWRITE)) return 3;
    uint32_t state = 0x1a2b3c4d;
    volatile uint32_t *words = (volatile uint32_t *)p;
    for (SIZE_T i = 0; i < commit_size / sizeof(*words); i++) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        words[i] = state;
    }
    if (!snapshot("touched_256m")) return 4;
    if (!VirtualFree(p, commit_size, MEM_DECOMMIT) || !snapshot("decommitted")) return 5;
    if (!VirtualFree(p, 0, MEM_RELEASE) || !snapshot("released")) return 6;
    puts("RESULT complete");
    fflush(stdout);
    Sleep(3000);
    return 0;
}
