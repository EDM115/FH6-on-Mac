// Check diagnostic coverage and unchanged forwarding on a synthetic texture.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <stdio.h>
#include <string.h>
int main(int argc, char **argv) {
    @autoreleasepool {
        id<MTLDevice> device=MTLCreateSystemDefaultDevice();
        if (!device) return 2;
        MTLTextureDescriptor *desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
                                   MTLPixelFormatDepth32Float width:32 height:24 mipmapped:NO];
        desc.storageMode=MTLStorageModePrivate;
        desc.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead|MTLTextureUsagePixelFormatView;
        id<MTLTexture> texture=[device newTextureWithDescriptor:desc];
        if (!texture) return 3;
        MTLPixelFormat format=argc>2 && !strcmp(argv[2],"invalid") ? MTLPixelFormatR32Float : MTLPixelFormatDepth32Float;
        id<MTLTexture> view=nil;
        if (argc>1 && !strcmp(argv[1],"swizzle")) {
            view=[texture newTextureViewWithPixelFormat:format textureType:MTLTextureType2D
                            levels:NSMakeRange(0,1) slices:NSMakeRange(0,1) swizzle:MTLTextureSwizzleChannelsDefault];
        } else if (argc>1 && !strcmp(argv[1],"simple")) {
            view=[texture newTextureViewWithPixelFormat:format];
        } else {
            view=[texture newTextureViewWithPixelFormat:format textureType:MTLTextureType2D
                            levels:NSMakeRange(0,1) slices:NSMakeRange(0,1)];
        }
        if (!view || view.pixelFormat!=format || view.width!=32 || view.height!=24) return 4;
        puts("VALID_TEXTURE_VIEW_PASSED");
        [view release]; [texture release]; [device release];
        return 0;
    }
}
