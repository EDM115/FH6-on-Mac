#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int transmit(SOCKET s, SOCKADDR_IN6 *destination, const char *tag)
{
    int count = sendto(s, tag, strlen(tag), 0, (SOCKADDR *)destination, sizeof(*destination));
    return count == (int)strlen(tag) ? 0 : 1;
}

int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    unsigned port4 = strtoul(argv[1], NULL, 10), port6 = strtoul(argv[2], NULL, 10);
    if (!port4 || port4 > 65535 || !port6 || port6 > 65535) return 2;
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data)) return 3;
    puts("READY");
    fflush(stdout);
    Sleep(1000);
    SOCKET s = WSASocketW(AF_INET6, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
    if (s == INVALID_SOCKET) return 4;
    int value = 0, rc, error;
    if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&value, sizeof(value))) return 5;
    SOCKADDR_IN6 local = {0}, v4 = {0}, v6 = {0};
    local.sin6_family = AF_INET6;
    if (bind(s, (SOCKADDR *)&local, sizeof(local))) return 6;
    v4.sin6_family = v6.sin6_family = AF_INET6;
    v4.sin6_port = htons(port4);
    v6.sin6_port = htons(port6);
    v4.sin6_addr.u.Byte[10] = v4.sin6_addr.u.Byte[11] = 0xff;
    v4.sin6_addr.u.Byte[12] = 127;
    v4.sin6_addr.u.Byte[15] = v6.sin6_addr.u.Byte[15] = 1;
    value = 37;
    if (setsockopt(s, IPPROTO_IPV6, IPV6_UNICAST_HOPS, (const char *)&value, sizeof(value))) return 7;
    if (transmit(s, &v4, "before-v4") || transmit(s, &v6, "before-v6")) return 8;
    value = 255;
    WSASetLastError(0);
    rc = setsockopt(s, IPPROTO_IP, IP_TTL, (const char *)&value, sizeof(value));
    error = WSAGetLastError();
    printf("ttl255_rc=%d error=%d\n", rc, error);
    if (transmit(s, &v4, "after-v4") || transmit(s, &v6, "after-v6")) return 9;
    closesocket(s);
    WSACleanup();
    return 0;
}
