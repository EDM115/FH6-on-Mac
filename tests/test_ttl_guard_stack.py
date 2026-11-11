"""Exercise the actual TTL helper with nonzero automatic storage and a one-byte V6ONLY result"""
import json
from pathlib import Path
import subprocess
import sys

import tempfile
import unittest
import re

ROOT = Path(__file__).resolve().parents[1]

scaffold = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
typedef uintptr_t SOCKET;
typedef int32_t LONG;
typedef uint32_t DWORD;
typedef unsigned char BYTE;
typedef wchar_t WCHAR;
struct sockaddr { unsigned short sa_family; };
struct sockaddr_storage { unsigned short ss_family; char padding[126]; };
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define WSAEINVAL 10022
#define AF_INET6 23
#define SOCK_DGRAM 2
#define SOL_SOCKET 65535
#define SO_TYPE 4104
#define IPPROTO_IPV6 41
#define IPV6_V6ONLY 27
#define IOCTL_AFD_WINE_SET_IP_TTL 1
#define IOCTL_AFD_WINE_SET_IPV6_UNICAST_HOPS 2
static DWORD last_error;
static unsigned enabled, only_v6, fallback_calls, observed_width;
static uint32_t observed_after_write;
static DWORD GetLastError(void) { return last_error; }
static void SetLastError(DWORD error) { last_error = error; }
static DWORD GetCurrentProcessId(void) { return 1; }
static DWORD GetCurrentThreadId(void) { return 1; }
static LONG InterlockedIncrement(LONG *value) { return ++*value; }
static DWORD GetEnvironmentVariableW(const WCHAR *name, WCHAR *value, DWORD count)
{
    (void)name;
    if (count != 2) abort();
    value[0] = enabled ? L'1' : L'0'; value[1] = 0;
    return 1;
}
static int server_setsockopt(SOCKET socket, unsigned code, const char *value, int length)
{
    (void)socket; (void)value;
    if (length != 4) abort();
    if (code == IOCTL_AFD_WINE_SET_IP_TTL) { last_error = WSAEINVAL; return -1; }
    if (code != IOCTL_AFD_WINE_SET_IPV6_UNICAST_HOPS) abort();
    fallback_calls++; last_error = 0;
    return 0;
}
static int getsockname(SOCKET socket, struct sockaddr *address, int *length)
{
    (void)socket; (void)length;
    address->sa_family = AF_INET6;
    return 0;
}
static int getsockopt(SOCKET socket, int level, int option, char *value, int *length)
{
    (void)socket;
    if (level == SOL_SOCKET && option == SO_TYPE)
    {
        int kind = SOCK_DGRAM;
        if (*length != sizeof(kind)) abort();
        memcpy(value, &kind, sizeof(kind));
        return 0;
    }
    if (level != IPPROTO_IPV6 || option != IPV6_V6ONLY || *length < 1) abort();
    observed_width = *length;
    *value = only_v6;
    observed_after_write = 0;
    memcpy(&observed_after_write, value, observed_width);
    *length = 1;
    return 0;
}
static int discard_diagnostic(FILE *stream, const char *format, ...)
{
    (void)stream; (void)format;
    return 0;
}
#define fprintf discard_diagnostic
HELPER
#undef fprintf
int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    enabled = atoi(argv[1]); only_v6 = atoi(argv[2]);
    DWORD value = 255;
    int result = fh6_set_ip_ttl(7, (const char *)&value, sizeof(value));
    unsigned expected = enabled && !only_v6;
    printf("result=%d error=%u fallback=%u getter_capacity=%u bytes_after_getter=%08x\n", result, last_error, fallback_calls, observed_width, observed_after_write);
    return result == (expected ? 0 : -1) && last_error == (expected ? 0 : WSAEINVAL) && fallback_calls == expected ? 0 : 7;
}
'''
def helper_from_patch():
    patch = (ROOT / 'patches/wine/0025-fh6-dualstack-ttl255.patch').read_text()
    additions = '\n'.join(line[1:] for line in patch.splitlines() if line.startswith('+') and not line.startswith('+++'))
    start = additions.index('static int fh6_set_ip_ttl(')
    opened = additions.index('{', start)
    depth = 0
    for index in range(opened, len(additions)):
        depth += (additions[index] == '{') - (additions[index] == '}')
        if depth == 0:
            return additions[start:index + 1]
    raise AssertionError('Incomplete TTL helper in exported patch')


class TTLGuardTests(unittest.TestCase):
    def test_one_byte_getter_rejects_the_original_and_admits_the_fixed_helper(self):
        fixed = helper_from_patch()
        self.assertIn('BYTE v6only = 1', fixed)
        original, count = re.subn(r'    BYTE v6only = 1;[^\n]*\n    int ret, type, size;', '    int ret, type, v6only, size;', fixed)
        self.assertEqual(count, 1)
        with tempfile.TemporaryDirectory() as directory:
            for label, helper in [('original', original), ('fixed', fixed)]:
                source = Path(directory) / (label + '.c')
                executable = source.with_suffix('')
                source.write_text(scaffold.replace('HELPER', helper))
                subprocess.run(['/usr/bin/clang', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-sign-compare', '-O1', '-fno-inline', '-ftrivial-auto-var-init=pattern', str(source), '-o', str(executable)], check=True, capture_output=True)
                for gate, v6only in ((1, 0), (0, 0), (1, 1)):
                    result = subprocess.run([str(executable), str(gate), str(v6only)], capture_output=True, text=True)
                    expected = 7 if label == 'original' and gate and not v6only else 0
                    self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
                    if expected:
                        self.assertIn('fallback=0 getter_capacity=4 bytes_after_getter=aaaaaa00', result.stdout)


if __name__ == '__main__':
    unittest.main()
