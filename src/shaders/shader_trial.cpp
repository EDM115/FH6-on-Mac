// Opt-in diagnostic overlay. Only exact prevalidated shader hashes are replaced.
// Original FH6 files and the normal D3DMetal cache are never modified.
#include <CommonCrypto/CommonDigest.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <unistd.h>
#ifndef FH6_TRIAL_ROOT
#error Define private workspace root
#endif
struct IRObject;
extern IRObject *IRObjectCreateFromDXIL(const char *, size_t);
static std::atomic<unsigned> logCount{0};
static std::mutex shaderMutex;
static std::unordered_map<std::string,std::vector<char>> shaders;
static bool enabled() {
    const char *mode=getenv("FH6_SHADER_TRIAL");
    return mode && (!strcmp(mode,"control") || !strcmp(mode,"fallback"));
}
static bool fallback() {const char *mode=getenv("FH6_SHADER_TRIAL");return mode && !strcmp(mode,"fallback");}
static size_t trialConfstr(int name,char *buffer,size_t length) {
    if(enabled() && name==_CS_DARWIN_USER_CACHE_DIR) {
        Dl_info caller={};
        if(dladdr(__builtin_return_address(0),&caller) && caller.dli_fname && strstr(caller.dli_fname,"/D3DMetal.framework/")) {
            const char *root=fallback()?FH6_TRIAL_ROOT "/fallback-cache/":FH6_TRIAL_ROOT "/control-cache/";
            if(buffer && length)snprintf(buffer,length,"%s",root);
            return strlen(root)+1;
        }
    }
    return confstr(name,buffer,length);
}
static IRObject *trialCreate(const char *bytes,size_t n) {
    if(!enabled() || !bytes || n<32 || n>8*1024*1024 || memcmp(bytes,"DXBC",4))return IRObjectCreateFromDXIL(bytes,n);
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];CC_SHA256(bytes,(CC_LONG)n,digest);
    char hex[65];for(unsigned i=0;i<32;i++)snprintf(hex+2*i,3,"%02x",digest[i]);
    const std::vector<char> *replacement=nullptr;
    {
        // Keep bytecode storage alive for the entire process even if the
        // converter retains the supplied data instead of copying it.
        std::lock_guard<std::mutex> guard(shaderMutex);
        auto found=shaders.find(hex);
        if(found!=shaders.end())replacement=&found->second;
        else {
            std::string path=FH6_TRIAL_ROOT "/prepared-fallbacks/replacements/";
            path+=hex;path+=".dxil";
            std::ifstream f(path,std::ios::binary|std::ios::ate);
            if(f) {
                auto size=f.tellg();
                if(size>=32 && size<=8*1024*1024 && shaders.size()<128) {
                    f.seekg(0);std::vector<char> data((size_t)size);
                    if(f.read(data.data(),data.size()))replacement=&shaders.emplace(hex,std::move(data)).first->second;
                }
            }
        }
    }
    if(!replacement)return IRObjectCreateFromDXIL(bytes,n);
    unsigned count=logCount.fetch_add(1);
    if(count<64)fprintf(stderr,"FH6_SHADER_TRIAL pid=%d mode=%s original=%s bytes=%zu replacement=%zu\n",getpid(),fallback()?"fallback":"control",hex,n,replacement->size());
    return fallback()?IRObjectCreateFromDXIL(replacement->data(),replacement->size()):IRObjectCreateFromDXIL(bytes,n);
}
__attribute__((constructor))static void loaded() {
    if(enabled())fprintf(stderr,"FH6_SHADER_TRIAL_LOADED pid=%d mode=%s\n",getpid(),getenv("FH6_SHADER_TRIAL"));
}
__attribute__((used,section("__DATA,__interpose")))
static const struct {const void *replacement,*original;} trialInterpose[]={
    {(const void *)&trialCreate,(const void *)&IRObjectCreateFromDXIL},
    {(const void *)&trialConfstr,(const void *)&confstr}
};
