/* Bounded event race and Windows directory notification integration check */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static HANDLE quit_event, work_event;
static volatile LONG stopping, worker_errors;
static volatile LONG apc_calls;
static DWORD WINAPI producer(void *unused)
{
    unsigned int loops = 0;
    (void)unused;
    while (!InterlockedCompareExchange(&stopping, 0, 0)) {
        if (!SetEvent(work_event)) InterlockedIncrement(&worker_errors);
        if (!(++loops % 64)) SwitchToThread();
    }
    return 0;
}
static void CALLBACK apc(ULONG_PTR unused)
{
    (void)unused;
    InterlockedIncrement(&apc_calls);
}
static int directory_check(void)
{
    WCHAR temp[MAX_PATH], directory[MAX_PATH], file[MAX_PATH];
    BYTE buffer[4096];
    DWORD bytes = 0;
    OVERLAPPED overlapped = {0};
    HANDLE dir, created;
    int ok = 0;
    if (!GetTempPathW(MAX_PATH, temp)) return 0;
    swprintf(directory, MAX_PATH, L"%lsfh6-msync-%lu", temp, GetCurrentProcessId());
    swprintf(file, MAX_PATH, L"%ls\\notice.txt", directory);
    if (!CreateDirectoryW(directory, NULL)) return 0;
    dir = CreateFileW(directory, FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
    if (dir == INVALID_HANDLE_VALUE) goto cleanup_dir;
    overlapped.hEvent = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!overlapped.hEvent) goto cleanup_handle;
    if (!ReadDirectoryChangesW(dir, buffer, sizeof(buffer), FALSE, FILE_NOTIFY_CHANGE_FILE_NAME, NULL, &overlapped, NULL)) goto cleanup_event;
    created = CreateFileW(file, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (created == INVALID_HANDLE_VALUE) goto cleanup_event;
    CloseHandle(created);
    if (WaitForSingleObject(overlapped.hEvent, 4000) == WAIT_OBJECT_0 && GetOverlappedResult(dir, &overlapped, &bytes, FALSE)) {
        FILE_NOTIFY_INFORMATION *info = (FILE_NOTIFY_INFORMATION *)buffer;
        ok = bytes >= sizeof(DWORD)*3 && info->Action == FILE_ACTION_ADDED && info->FileNameLength == 10*sizeof(WCHAR) && !wmemcmp(info->FileName, L"notice.txt", 10);
    }
cleanup_event:
    CancelIo(dir);
    CloseHandle(overlapped.hEvent);
cleanup_handle:
    CloseHandle(dir);
cleanup_dir:
    DeleteFileW(file);
    RemoveDirectoryW(directory);
    return ok;
}
int main(int argc, char **argv)
{
    unsigned int seconds = argc > 1 ? (unsigned int)atoi(argv[1]) : 15;
    unsigned long long waits = 0, timeouts = 0, failures = 0;
    ULONGLONG start, next;
    HANDLE objects[2], worker;
    DWORD result;
    if (seconds < 1 || seconds > 30) return 2;
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("READY pid=%lu directory_notifications=%d\n", GetCurrentProcessId(), directory_check());
    quit_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    work_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (!quit_event || !work_event) return 3;
    objects[0] = quit_event;
    objects[1] = work_event;
    if (WaitForMultipleObjects(2, objects, FALSE, 5) != WAIT_TIMEOUT) return 4;
    if (!QueueUserAPC(apc, GetCurrentThread(), 0)) return 5;
    if (WaitForMultipleObjectsEx(2, objects, FALSE, 1000, TRUE) != WAIT_IO_COMPLETION || apc_calls != 1) return 6;
    SetEvent(quit_event);
    SetEvent(work_event);
    if (WaitForMultipleObjects(2, objects, TRUE, 1000) != WAIT_OBJECT_0) return 7;
    ResetEvent(quit_event);
    Sleep(1000);
    worker = CreateThread(NULL, 0, producer, NULL, 0, NULL);
    if (!worker) return 8;
    start = GetTickCount64();
    next = start + 1000;
    while (GetTickCount64() - start < seconds * 1000 && waits < 10000000) {
        result = WaitForMultipleObjectsEx(2, objects, FALSE, 1000, (waits & 1) != 0);
        if (result == WAIT_OBJECT_0 + 1) ++waits;
        else if (result == WAIT_TIMEOUT) ++timeouts;
        else { ++failures; break; }
        if (GetTickCount64() >= next) {
            printf("PROGRESS elapsed_ms=%llu waits=%llu timeouts=%llu\n", GetTickCount64()-start, waits, timeouts);
            next += 1000;
        }
    }
    InterlockedExchange(&stopping, 1);
    if (WaitForSingleObject(worker, 2000) != WAIT_OBJECT_0) ++failures;
    ResetEvent(work_event);
    SetEvent(quit_event);
    if (WaitForMultipleObjects(2, objects, FALSE, 1000) != WAIT_OBJECT_0) ++failures;
    printf("RESULT elapsed_ms=%llu waits=%llu timeouts=%llu failures=%llu worker_errors=%ld apc_calls=%ld\n", GetTickCount64()-start, waits, timeouts, failures, worker_errors, apc_calls);
    Sleep(1000);
    CloseHandle(worker);
    CloseHandle(work_event);
    CloseHandle(quit_event);
    return failures || worker_errors || waits < 1000;
}
