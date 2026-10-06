/* Diagnostic only: mirror one D3DMetal pipeline error before os_log redaction.
 * No game-memory writes, graphics changes, account logs, or system preferences.
 */
#include <os/log.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
extern void _os_log_impl(void *, os_log_t, os_log_type_t, const char *, uint8_t *, uint32_t);
extern void _os_log_error_impl(void *, os_log_t, os_log_type_t, const char *, uint8_t *, uint32_t);
static _Atomic unsigned captured;
static void mirror_pipeline_log(const char *format, uint8_t *buffer, uint32_t size) {
    if (format && strstr(format, "Failed to create pipeline state:") &&
        strstr(format, "marking PSO(") && buffer && size >= 2 &&
        atomic_fetch_add_explicit(&captured, 1, memory_order_relaxed) < 160) {
        /* Apple os_log packs a two-byte header followed by typed arguments.
         * This exact format contains only function/error/shader strings and
         * scalar line/PSO identifiers. Check all bounds before decoding.
         */
        size_t offset = 2;
        unsigned count = buffer[1];
        flockfile(stderr);
        fputs("FH6_PIPELINE_DIAGNOSTIC", stderr);
        for (unsigned i = 0; i < count && offset + 2 <= size; ++i) {
            unsigned descriptor = buffer[offset++];
            unsigned length = buffer[offset++];
            if (offset + length > size) break;
            if ((descriptor >> 4) == 2 && length == sizeof(char *)) {
                const char *value = NULL;
                memcpy(&value, buffer + offset, sizeof(value));
                fprintf(stderr, " arg%u=%.*s", i, 1800, value ? value : "(null)");
            } else if ((descriptor >> 4) == 0 && length <= sizeof(uint64_t)) {
                uint64_t value = 0;
                memcpy(&value, buffer + offset, length);
                fprintf(stderr, " arg%u=%llu", i, (unsigned long long)value);
            }
            offset += length;
        }
        fputc('\n', stderr);
        fflush(stderr);
        funlockfile(stderr);
    }
}
static void capture_pipeline_log(void *dso, os_log_t log, os_log_type_t type,
                                 const char *format, uint8_t *buffer, uint32_t size) {
    mirror_pipeline_log(format, buffer, size);
    _os_log_impl(dso, log, type, format, buffer, size);
}
static void capture_pipeline_error(void *dso, os_log_t log, os_log_type_t type,
                                 const char *format, uint8_t *buffer, uint32_t size) {
    mirror_pipeline_log(format, buffer, size);
    _os_log_error_impl(dso, log, type, format, buffer, size);
}
__attribute__((used, section("__DATA,__interpose")))
static const struct { const void *replacement; const void *original; } interpose_pipeline_log = {
    (const void *)&capture_pipeline_log, (const void *)&_os_log_impl
};

__attribute__((used, section("__DATA,__interpose")))
static const struct { const void *replacement; const void *original; } interpose_pipeline_error = {
    (const void *)&capture_pipeline_error, (const void *)&_os_log_error_impl
};

__attribute__((constructor)) static void capture_loaded(void) {
    fputs("FH6_PIPELINE_CAPTURE_LOADED\n", stderr);
}
