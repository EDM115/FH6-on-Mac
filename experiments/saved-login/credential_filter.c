/* Exercise only a uniquely named synthetic generic credential in a scratch bottle */
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <wincred.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    WCHAR target[128], filter[128], missing[128];
    BYTE fixture[] = {0x46, 0x48, 0x36, 0x19, 0x2a};
    CREDENTIALW credential = {0}, *readback = NULL, **items = NULL;
    DWORD count = 0, error, failures = 0;
    BOOL ok;
    unsigned long pid = GetCurrentProcessId();
    swprintf(target, 128, L"FH6-0019-Synthetic-%lu/entry", pid);
    swprintf(filter, 128, L"FH6-0019-Synthetic-%lu/*", pid);
    swprintf(missing, 128, L"FH6-0019-Synthetic-%lu/missing*", pid);
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = target;
    credential.CredentialBlobSize = sizeof fixture;
    credential.CredentialBlob = fixture;
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = L"synthetic-probe";
    SetLastError(0);
    ok = CredWriteW(&credential, 0);
    error = GetLastError();
    printf("write fixture: success=%u error=%lu\n", !!ok, (unsigned long)error);
    if (!ok) return 2;
    SetLastError(0);
    ok = CredReadW(target, CRED_TYPE_GENERIC, 0, &readback);
    error = GetLastError();
    BOOL same = ok && readback && readback->CredentialBlobSize == sizeof fixture && !memcmp(readback->CredentialBlob, fixture, sizeof fixture);
    printf("exact read: success=%u error=%lu fixture_matches=%u\n", !!ok, (unsigned long)error, !!same);
    if (!same) failures++;
    if (readback) CredFree(readback);
    SetLastError(0);
    ok = CredEnumerateW(filter, 0, &count, &items);
    error = GetLastError();
    same = ok && count == 1 && items && items[0]->Type == CRED_TYPE_GENERIC && !wcscmp(items[0]->TargetName, target) && items[0]->CredentialBlobSize == sizeof fixture && !memcmp(items[0]->CredentialBlob, fixture, sizeof fixture);
    printf("matching prefix: success=%u error=%lu count=%lu fixture_matches=%u\n", !!ok, (unsigned long)error, (unsigned long)count, !!same);
    if (!same) failures++;
    if (items) CredFree(items);
    items = NULL;
    count = 0;
    SetLastError(0);
    ok = CredEnumerateW(missing, 0, &count, &items);
    error = GetLastError();
    printf("missing prefix: success=%u error=%lu count=%lu\n", !!ok, (unsigned long)error, (unsigned long)count);
    if (ok || error != ERROR_NOT_FOUND || count) failures++;
    if (items) CredFree(items);
    SetLastError(0);
    ok = CredDeleteW(target, CRED_TYPE_GENERIC, 0);
    error = GetLastError();
    printf("delete fixture: success=%u error=%lu\n", !!ok, (unsigned long)error);
    if (!ok) failures++;
    printf("RESULT: %s (%lu failures)\n", failures ? "FAIL" : "PASS", (unsigned long)failures);
    fflush(stdout);
    Sleep(3000); /* Keep the process alive briefly for mapped-DLL verification */
    return failures ? 1 : 0;
}
