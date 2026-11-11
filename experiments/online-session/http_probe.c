/* Exercise real WinHTTP sync, async and timeout paths with synthetic secret canaries */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static HANDLE completed;
static volatile LONG callback_error, callback_status;

static void CALLBACK callback(HINTERNET request, DWORD_PTR context, DWORD status, void *info, DWORD length)
{
    (void)context;
    if (status == WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE) {
        if (!WinHttpReceiveResponse(request, NULL)) {
            InterlockedExchange(&callback_error, GetLastError());
            SetEvent(completed);
        }
    } else if (status == WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE) {
        DWORD code = 0, size = sizeof(code);
        if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &code, &size, NULL))
            InterlockedExchange(&callback_error, GetLastError());
        InterlockedExchange(&callback_status, code);
        SetEvent(completed);
    } else if (status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR && length == sizeof(WINHTTP_ASYNC_RESULT)) {
        InterlockedExchange(&callback_error, ((WINHTTP_ASYNC_RESULT *)info)->dwError);
        SetEvent(completed);
    }
}

static int test(INTERNET_PORT port, const WCHAR *path, BOOL async, DWORD expected_status, DWORD expected_error)
{
    HINTERNET session = NULL, connection = NULL, request = NULL;
    DWORD status = 0, error = 0, size = sizeof(status), bytes = 0;
    DWORD response_timeout = expected_error ? 200 : 2000;
    char buffer[128] = {0};
    const char body[] = "BODY_SECRET_CANARY";
    int pass = 0;
    callback_error = callback_status = 0;
    ResetEvent(completed);
    session = WinHttpOpen(L"FH6-local-fixture", WINHTTP_ACCESS_TYPE_NO_PROXY, NULL, NULL, async ? WINHTTP_FLAG_ASYNC : 0);
    if (!session) goto done;
    if (!WinHttpSetTimeouts(session, 1000, 1000, 1000, expected_error ? 200 : 2000)) goto done;
    if (!WinHttpSetOption(session, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &response_timeout, sizeof(response_timeout))) goto done;
    connection = WinHttpConnect(session, L"127.0.0.1", port, 0);
    if (!connection) goto done;
    request = WinHttpOpenRequest(connection, L"POST", path, NULL, NULL, NULL, 0);
    if (!request) goto done;
    if (!WinHttpAddRequestHeaders(request, L"Authorization: Bearer HEADER_SECRET_CANARY\r\n", (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD)) goto done;
    if (async && WinHttpSetStatusCallback(request, callback, WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS, 0) == WINHTTP_INVALID_STATUS_CALLBACK) goto done;
    if (!WinHttpSendRequest(request, NULL, 0, (void *)body, sizeof(body)-1, sizeof(body)-1, 0)) {
        error = GetLastError();
        goto evaluate;
    }
    if (async) {
        if (WaitForSingleObject(completed, 6000) != WAIT_OBJECT_0) goto done;
        error = callback_error;
        status = callback_status;
    } else {
        if (!WinHttpReceiveResponse(request, NULL)) { error = GetLastError(); goto evaluate; }
        if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, NULL, &status, &size, NULL)) goto done;
        if (!WinHttpReadData(request, buffer, sizeof(buffer)-1, &bytes)) goto done;
        if (bytes != strlen("RESPONSE_SECRET_CANARY") || strcmp(buffer, "RESPONSE_SECRET_CANARY")) goto done;
    }
evaluate:
    pass = error == expected_error && (error || status == expected_status);
    printf("CASE async=%d expected_status=%lu actual_status=%lu expected_error=%lu actual_error=%lu pass=%d\n", async, expected_status, status, expected_error, error, pass);
done:
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    if (session) WinHttpCloseHandle(session);
    Sleep(100);
    return pass;
}

int main(int argc, char **argv)
{
    int passed = 0;
    long port = argc > 1 ? strtol(argv[1], NULL, 10) : 0;
    if (port < 1 || port > 65535) return 2;
    setvbuf(stdout, NULL, _IONBF, 0);
    completed = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!completed) return 3;
    puts("READY");
    Sleep(1000);
    passed += test(port, L"/Match/CreateMatchmakingTicket?token=QUERY_SECRET_CANARY", FALSE, 200, 0);
    passed += test(port, L"/Match/GetMatchmakingTicket?token=QUERY_SECRET_CANARY", TRUE, 401, 0);
    passed += test(port, L"/UNKNOWN_PATH_SECRET_CANARY", TRUE, 503, 0);
    /* Preserve the measured vendor behavior, which returns the Winsock timeout code */
    passed += test(port, L"/timeout", TRUE, 0, WSAETIMEDOUT);
    printf("RESULT passed=%d total=4\n", passed);
    CloseHandle(completed);
    Sleep(1000);
    return passed == 4 ? 0 : 1;
}
