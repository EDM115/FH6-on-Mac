// Diagnostic capture only. Accept validated DXIL pixel-shader containers whose
// input signature explicitly names SV_ShadingRate. No graphics or bytecode edits.
#include <CommonCrypto/CommonDigest.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <strings.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef FH6_SHADER_CAPTURE_DIR
#error Specify a workspace-only capture directory at build time.
#endif
struct IRObject;
extern IRObject *IRObjectCreateFromDXIL(const char *, size_t);
static std::atomic<size_t> capturedBytes{0};
static uint32_t u32(const char *p) { uint32_t n; memcpy(&n,p,4); return n; }
static bool relevant(const char *bytes, size_t n) {
    if (!bytes || n < 44 || n > 8*1024*1024 || memcmp(bytes,"DXBC",4) || u32(bytes+24)!=n) return false;
    uint32_t count=u32(bytes+28);
    if (count>64 || 32+size_t(count)*4>n) return false;
    bool pixel=false, input=false;
    for (uint32_t i=0;i<count;i++) {
        size_t off=u32(bytes+32+4*i);
        if (off>n-8) return false;
        size_t length=u32(bytes+off+4);
        if (length>n-off-8) return false;
        const char *p=bytes+off+8;
        if (!memcmp(bytes+off,"DXIL",4) && length>=8) pixel=(u32(p)>>16)==0;
        bool sig1=!memcmp(bytes+off,"ISG1",4), sig0=!memcmp(bytes+off,"ISGN",4);
        if ((!sig1 && !sig0) || length<8) continue;
        uint32_t elements=u32(p), first=u32(p+4);
        size_t stride=sig1?32:24;
        if (elements>128 || first>length || elements*stride>length-first) return false;
        for (uint32_t j=0;j<elements;j++) {
            size_t name=u32(p+first+j*stride+(sig1?4:0));
            if (name>=length) return false;
            const char *end=(const char *)memchr(p+name,0,length-name);
            if (!end) return false;
            if (size_t(end-(p+name))==14 && !strncasecmp(p+name,"SV_ShadingRate",14)) input=true;
        }
    }
    return pixel && input;
}
static void capture(const char *bytes, size_t n) {
    if (!relevant(bytes,n)) return;
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(bytes,(CC_LONG)n,digest);
    char hex[65];for(unsigned i=0;i<32;i++) snprintf(hex+2*i,3,"%02x",digest[i]);
    char path[2048];snprintf(path,sizeof(path),"%s/%s.dxil",FH6_SHADER_CAPTURE_DIR,hex);
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
    if(fd<0)return;
    if(capturedBytes.fetch_add(n)+n>32*1024*1024) {close(fd);unlink(path);return;}
    size_t done=0;while(done<n){ssize_t r=write(fd,bytes+done,n-done);if(r<=0)break;done+=r;}
    close(fd);if(done!=n){unlink(path);return;}
    fprintf(stderr,"FH6_SHADER_CAPTURE pid=%d sha256=%s bytes=%zu\n",getpid(),hex,n);
}
static IRObject *captureCreate(const char *bytes,size_t n) {
    capture(bytes,n);
    return IRObjectCreateFromDXIL(bytes,n);
}
__attribute__((used,section("__DATA,__interpose")))
static const struct {const void *replacement,*original;} captureInterpose={
    (const void *)&captureCreate,(const void *)&IRObjectCreateFromDXIL
};
