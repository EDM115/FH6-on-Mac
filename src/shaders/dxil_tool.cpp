// Local file-only DXC bridge: disassemble, assemble, or validate a shader.
#include "dxc/dxcapi.h"
#include <fstream>
#include <iterator>
#include <vector>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
int main(int argc,char **argv) {
    if(argc!=6 && argc!=7) {fprintf(stderr,"usage: dxil_tool libdxcompiler mode input output report [original-container]\n");return 1;}
    void *h=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);
    if(!h){fprintf(stderr,"%s\n",dlerror());return 2;}
    auto create=(DxcCreateInstanceProc)dlsym(h,"DxcCreateInstance");if(!create)return 3;
    std::ifstream f(argv[3],std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(f)),{});
    if(bytes.empty()||bytes.size()>16*1024*1024)return 4;
    IDxcUtils *utils=nullptr;HRESULT hr=create(CLSID_DxcUtils,__uuidof(IDxcUtils),(void **)&utils);if(FAILED(hr))return 5;
    IDxcBlobEncoding *input=nullptr;hr=utils->CreateBlob(bytes.data(),(UINT32)bytes.size(),DXC_CP_UTF8,&input);if(FAILED(hr))return 6;
    IDxcOperationResult *result=nullptr;
    if(!strcmp(argv[2],"disassemble")) {
        IDxcCompiler3 *compiler=nullptr;hr=create(CLSID_DxcCompiler,__uuidof(IDxcCompiler3),(void **)&compiler);if(FAILED(hr))return 7;
        DxcBuffer b={bytes.data(),bytes.size(),0};IDxcResult *r=nullptr;
        hr=compiler->Disassemble(&b,__uuidof(IDxcResult),(void **)&r);result=r;compiler->Release();
    } else if(!strcmp(argv[2],"assemble")) {
        IDxcAssembler *assembler=nullptr;hr=create(CLSID_DxcAssembler,__uuidof(IDxcAssembler),(void **)&assembler);if(FAILED(hr))return 8;
        hr=assembler->AssembleToContainer(input,&result);assembler->Release();
    } else if(!strcmp(argv[2],"validate")) {
        IDxcValidator *validator=nullptr;hr=create(CLSID_DxcValidator,__uuidof(IDxcValidator),(void **)&validator);if(FAILED(hr))return 9;
        hr=validator->Validate(input,DxcValidatorFlags_Default,&result);validator->Release();
    } else return 10;
    if(FAILED(hr)||!result){fprintf(stderr,"operation HRESULT 0x%x\n",(unsigned)hr);return 11;}
    HRESULT status;result->GetStatus(&status);
    IDxcBlobEncoding *errors=nullptr;result->GetErrorBuffer(&errors);
    std::ofstream report(argv[5]);report<<"HRESULT=0x"<<std::hex<<(unsigned)status<<"\n";
    if(errors&&errors->GetBufferSize()){report.write((const char *)errors->GetBufferPointer(),errors->GetBufferSize());errors->Release();}
    if(FAILED(status))return 12;
    IDxcBlob *output=nullptr;result->GetResult(&output);
    // Assembly regenerates derived signature/validation parts. Restore the
    // original root signature through DXC's builder, without hand-writing a
    // container checksum or reusing the old shader hash.
    if(output && argc==7) {
        if(strcmp(argv[2],"assemble"))return 13;
        std::ifstream originalFile(argv[6],std::ios::binary);
        std::vector<char> original((std::istreambuf_iterator<char>(originalFile)),{});
        if(original.size()<32 || original.size()>16*1024*1024)return 14;
        IDxcContainerBuilder *builder=nullptr;
        hr=create(CLSID_DxcContainerBuilder,__uuidof(IDxcContainerBuilder),(void **)&builder);
        if(FAILED(hr)||FAILED(builder->Load(output)))return 15;
        DxcBuffer originalBuffer={original.data(),original.size(),0};
        void *part=nullptr;UINT32 partSize=0;
        hr=utils->GetDxilContainerPart(&originalBuffer,DXC_PART_ROOT_SIGNATURE,&part,&partSize);
        if(SUCCEEDED(hr)) {
            IDxcBlobEncoding *partBlob=nullptr;
            hr=utils->CreateBlob(part,partSize,0,&partBlob);
            if(FAILED(hr)||FAILED(builder->AddPart(DXC_PART_ROOT_SIGNATURE,partBlob)))return 16;
            partBlob->Release();
        }
        IDxcOperationResult *built=nullptr;
        if(FAILED(builder->SerializeContainer(&built)))return 17;
        built->GetStatus(&status);if(FAILED(status))return 18;
        output->Release();built->GetResult(&output);built->Release();builder->Release();
    }
    if(output) {std::ofstream out(argv[4],std::ios::binary);out.write((const char *)output->GetBufferPointer(),output->GetBufferSize());output->Release();}
    input->Release();utils->Release();result->Release();return 0;
}
