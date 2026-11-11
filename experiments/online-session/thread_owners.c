#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>

/* Enumerate numeric thread ownership without opening or suspending game threads */
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    DWORD pid = strtoul(argv[1], NULL, 0);
    if (!pid) return 2;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 3;
    THREADENTRY32 entry = {0};
    entry.dwSize = sizeof(entry);
    BOOL found = Thread32First(snapshot, &entry);
    while (found)
    {
        if (entry.th32OwnerProcessID == pid)
            printf("pid=%lu tid=%lu tid_hex=%04lx\n", pid, entry.th32ThreadID, entry.th32ThreadID);
        entry.dwSize = sizeof(entry);
        found = Thread32Next(snapshot, &entry);
    }
    DWORD error = GetLastError();
    CloseHandle(snapshot);
    return error == ERROR_NO_MORE_FILES ? 0 : 4;
}
