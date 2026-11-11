#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <mswsock.h>
#include <stdio.h>
#include <string.h>

static int probe(int family)
{
    SOCKET receive = socket(family, SOCK_DGRAM, IPPROTO_UDP);
    SOCKET transmit = socket(family, SOCK_DGRAM, IPPROTO_UDP);
    SOCKADDR_STORAGE address = {0};
    int length = family == AF_INET ? sizeof(SOCKADDR_IN) : sizeof(SOCKADDR_IN6);
    DWORD timeout = 1000;
    if (receive == INVALID_SOCKET || transmit == INVALID_SOCKET) return 2;
    if (family == AF_INET)
    {
        SOCKADDR_IN *v4 = (SOCKADDR_IN *)&address;
        v4->sin_family = AF_INET;
        v4->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
    else
    {
        SOCKADDR_IN6 *v6 = (SOCKADDR_IN6 *)&address;
        v6->sin6_family = AF_INET6;
        v6->sin6_addr.u.Byte[15] = 1;
    }
    if (bind(receive, (SOCKADDR *)&address, length) || getsockname(receive, (SOCKADDR *)&address, &length)) return 3;
    if (setsockopt(receive, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout))) return 4;
    for (int enable = 0; enable <= 1; ++enable)
    {
        BOOL flag = enable;
        DWORD bytes = 0;
        const DWORD options[] = {SIO_UDP_CONNRESET, SIO_UDP_NETRESET};
        for (unsigned i = 0; i < sizeof(options) / sizeof(options[0]); ++i)
        {
            WSASetLastError(0);
            int rc = WSAIoctl(receive, options[i], &flag, sizeof(flag), NULL, 0, &bytes, NULL, NULL);
            int error = WSAGetLastError();
            printf("family=%d option=0x%08lx enable=%d rc=%d error=%d bytes=%lu\n", family, options[i], enable, rc, error, bytes);
        }
    }
    const char payload[] = "UDP_PAYLOAD_SECRET_CANARY";
    char received[sizeof(payload)] = {0};
    int sent = sendto(transmit, payload, sizeof(payload), 0, (SOCKADDR *)&address, length);
    int count = recvfrom(receive, received, sizeof(received), 0, NULL, NULL);
    int passed = sent == sizeof(payload) && count == sizeof(payload) && !memcmp(payload, received, sizeof(payload));
    printf("family=%d loopback_payload_matches=%d\n", family, passed);
    closesocket(transmit);
    closesocket(receive);
    return passed ? 0 : 5;
}

static int async_probe(void)
{
    SOCKET s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    WSAOVERLAPPED overlapped = {0};
    BOOL enabled = FALSE;
    DWORD bytes = 0, flags = 0;
    overlapped.hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (s == INVALID_SOCKET || !overlapped.hEvent) return 6;
    WSASetLastError(0);
    int rc = WSAIoctl(s, SIO_UDP_NETRESET, &enabled, sizeof(enabled), NULL, 0, &bytes, &overlapped, NULL);
    int error = WSAGetLastError();
    DWORD wait = WaitForSingleObject(overlapped.hEvent, 1000);
    BOOL completed = WSAGetOverlappedResult(s, &overlapped, &bytes, FALSE, &flags);
    int completion_error = completed ? 0 : WSAGetLastError();
    printf("async rc=%d error=%d wait=%lu completed=%d completion_error=%d bytes=%lu\n", rc, error, wait, completed, completion_error, bytes);
    CloseHandle(overlapped.hEvent);
    closesocket(s);
    return wait == WAIT_OBJECT_0 ? 0 : 7;
}

static int unrelated_ioctl_probe(void)
{
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    BOOL enabled = FALSE;
    DWORD bytes = 0;
    if (s == INVALID_SOCKET) return 8;
    int rc = WSAIoctl(s, _WSAIOW(IOC_VENDOR, 0x1234), &enabled, sizeof(enabled), NULL, 0, &bytes, NULL, NULL);
    int error = WSAGetLastError();
    printf("unrelated_ioctl rc=%d error=%d\n", rc, error);
    closesocket(s);
    return rc == SOCKET_ERROR && error == WSAEOPNOTSUPP ? 0 : 9;
}

static int tcp_probe(void)
{
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    SOCKADDR_IN address = {0};
    int length = sizeof(address);
    DWORD timeout = 1000;
    if (listener == INVALID_SOCKET || client == INVALID_SOCKET) return 10;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener, (SOCKADDR *)&address, sizeof(address)) || getsockname(listener, (SOCKADDR *)&address, &length) || listen(listener, 1)) return 11;
    if (connect(client, (SOCKADDR *)&address, length)) return 12;
    SOCKET server = accept(listener, NULL, NULL);
    if (server == INVALID_SOCKET) return 13;
    if (setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout))) return 14;
    const char payload[] = "TCP_PAYLOAD_SECRET_CANARY";
    char received[sizeof(payload)] = {0};
    int sent = send(client, payload, sizeof(payload), 0);
    int count = recv(server, received, sizeof(received), MSG_WAITALL);
    int passed = sent == sizeof(payload) && count == sizeof(payload) && !memcmp(payload, received, sizeof(payload));
    printf("tcp_loopback_payload_matches=%d\n", passed);
    closesocket(server);
    closesocket(client);
    closesocket(listener);
    return passed ? 0 : 15;
}

int main(void)
{
    WSADATA data;
    char unused[8];
    GetEnvironmentVariableA("UNSELECTED_API_SECRET_CANARY", unused, sizeof(unused));
    if (WSAStartup(MAKEWORD(2, 2), &data)) return 1;
    puts("READY");
    fflush(stdout);
    Sleep(1000);
    int v4 = probe(AF_INET);
    int v6 = probe(AF_INET6);
    int asynchronous = async_probe();
    int unrelated = unrelated_ioctl_probe();
    int tcp = tcp_probe();
    WSACleanup();
    return v4 ? v4 : v6 ? v6 : asynchronous ? asynchronous : unrelated ? unrelated : tcp;
}
