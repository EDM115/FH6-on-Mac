#include <os/log.h>
int main(void) {
 os_log_t log = os_log_create("D3DMetal", "");
 os_log_error(log, "[%s:%d][ERROR] Failed to create pipeline state: %s, vs=%s,fs=%s,gs=%s,hs=%s,ds=%s, marking PSO(%llu) as no-op", "Probe", 123, "synthetic diagnostic error", "vertex-test", "fragment-test", "", "", "", 7ull);
 os_log_error(log, "Unrelated message: %s", "do-not-capture");
 return 0;
}
