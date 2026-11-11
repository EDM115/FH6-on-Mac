#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>

static void probe(int family, int dual_stack, int bind_socket, int type)
{
    SOCKET s = WSASocketW(family, type, type == SOCK_DGRAM ? IPPROTO_UDP : IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
    int rc, error, value = 0, length = sizeof(value);
    if (s == INVALID_SOCKET) { printf("create family=%d error=%d\n", family, WSAGetLastError()); return; }
    if (family == AF_INET6)
    {
        rc = getsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&value, &length);
        printf("family=%d default_v6only_rc=%d value=%d\n", family, rc, value);
        if (dual_stack)
        {
            value = 0;
            rc = setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&value, sizeof(value));
            printf("family=%d set_dualstack_rc=%d error=%d\n", family, rc, rc ? WSAGetLastError() : 0);
        }
    }
    if (bind_socket)
    {
        SOCKADDR_IN6 address = {0};
        address.sin6_family = AF_INET6;
        if (bind(s, (SOCKADDR *)&address, sizeof(address)))
        {
            printf("bind_failed family=%d error=%d\n", family, WSAGetLastError());
            closesocket(s);
            return;
        }
    }
    value = 1;
    rc = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&value, sizeof(value));
    printf("family=%d reuse_rc=%d error=%d\n", family, rc, rc ? WSAGetLastError() : 0);
    const int values[] = {1, 64, 255};
    for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
    {
        value = values[i];
        WSASetLastError(0);
        rc = setsockopt(s, IPPROTO_IP, IP_TTL, (char *)&value, sizeof(value));
        error = WSAGetLastError();
        printf("family=%d type=%d dual_stack=%d bound=%d IP_TTL=%d rc=%d error=%d\n", family, type, dual_stack, bind_socket, value, rc, error);
    }
    if (family == AF_INET6)
    {
        value = 255;
        WSASetLastError(0);
        rc = setsockopt(s, IPPROTO_IPV6, IPV6_UNICAST_HOPS, (char *)&value, sizeof(value));
        error = WSAGetLastError();
        printf("family=%d dual_stack=%d IPV6_UNICAST_HOPS=%d rc=%d error=%d\n", family, dual_stack, value, rc, error);
    }
    closesocket(s);
}

int main(void)
{
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data)) return 1;
    probe(AF_INET, 0, 0, SOCK_DGRAM);
    probe(AF_INET6, 0, 0, SOCK_DGRAM);
    probe(AF_INET6, 1, 0, SOCK_DGRAM);
    probe(AF_INET6, 0, 1, SOCK_DGRAM);
    probe(AF_INET6, 1, 1, SOCK_DGRAM);
    probe(AF_INET6, 1, 1, SOCK_STREAM);
    WSACleanup();
    return 0;
}
