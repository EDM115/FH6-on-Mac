// Check diagnostic coverage and unchanged forwarding on a synthetic texture.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdio.h>
int main(int argc, char **) {
    @autoreleasepool {
        id<MTLDevice> device=MTLCreateSystemDefaultDevice();
        if (!device) return 2;
        MTLTextureDescriptor *desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
                                   MTLPixelFormatDepth32Float width:32 height:24 mipmapped:NO];
        desc.storageMode=MTLStorageModePrivate;
        desc.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead|MTLTextureUsagePixelFormatView;
        id<MTLTexture> texture=[device newTextureWithDescriptor:desc];
        if (!texture) return 3;
        MTLPixelFormat format=argc>1 ? MTLPixelFormatR32Float : MTLPixelFormatDepth32Float;
        id<MTLTexture> view=[texture newTextureViewWithPixelFormat:format textureType:MTLTextureType2D
                            levels:NSMakeRange(0,1) slices:NSMakeRange(0,1)];
        if (!view || view.pixelFormat!=format || view.width!=32 || view.height!=24) return 4;
        puts("VALID_TEXTURE_VIEW_PASSED");
        [view release]; [texture release]; [device release];
        return 0;
    }
}
