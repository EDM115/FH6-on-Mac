/* SPDX-License-Identifier: GPL-3.0-only
 * Exported from the October 2026 FH6 experiments, see THIRD_PARTY_NOTICES.md
 */
/* Pure layout tests, no D3D device, renderer or game is created */
#include "../../third_party/highball-tsshim/d3d12.c"
#include <stddef.h>
#include "../../third_party/directx/sdk_abi_excerpt.h"

static int failures;
#define CHECK(condition) do { if (!(condition)) { printf("FAIL line %d: %s\n", __LINE__, #condition); failures++; } } while (0)
#define STREAM_ABI(number, type) do { SIZE_T a = 0; CHECK(stream_inner_size(number, &a) == sizeof(type)); CHECK(a == __alignof__(type)); } while (0)
#define ABI(type, size, alignment) _Static_assert(sizeof(type) == size && __alignof__(type) == alignment, #type " stream ABI changed")
ABI(D3D12_SHADER_BYTECODE, 16, 8);
ABI(D3D12_STREAM_OUTPUT_DESC, 32, 8);
ABI(D3D12_BLEND_DESC, 328, 4);
ABI(D3D12_RASTERIZER_DESC, 44, 4);
ABI(D3D12_DEPTH_STENCIL_DESC, 52, 4);
ABI(D3D12_INPUT_LAYOUT_DESC, 16, 8);
ABI(D3D12_RT_FORMAT_ARRAY, 36, 4);
ABI(DXGI_SAMPLE_DESC, 8, 4);
ABI(D3D12_CACHED_PIPELINE_STATE, 16, 8);
ABI(D3D12_DEPTH_STENCIL_DESC1, 56, 4);
ABI(D3D12_VIEW_INSTANCING_DESC, 24, 8);
ABI(D3D12_DEPTH_STENCIL_DESC2, 60, 4);
ABI(D3D12_RASTERIZER_DESC1, 44, 4);
ABI(D3D12_RASTERIZER_DESC2, 40, 4);

static D3D12_INPUT_ELEMENT_DESC element(UINT index, UINT slot, DXGI_FORMAT format, UINT offset)
{
    D3D12_INPUT_ELEMENT_DESC e = {"TEST", index, format, slot, offset, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
    return e;
}

static void offsets(void)
{
    D3D12_INPUT_ELEMENT_DESC input[] = {
        element(0, 0, DXGI_FORMAT_R8_UNORM, D3D12_APPEND_ALIGNED_ELEMENT),
        element(1, 1, DXGI_FORMAT_R32G32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT),
        element(2, 0, DXGI_FORMAT_R16_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT),
        element(3, 1, DXGI_FORMAT_R8G8_UNORM, D3D12_APPEND_ALIGNED_ELEMENT),
        element(4, 0, DXGI_FORMAT_R32_FLOAT, 20),
        element(5, 0, DXGI_FORMAT_R8_UNORM, D3D12_APPEND_ALIGNED_ELEMENT),
        element(6, 0, DXGI_FORMAT_R8_UNORM, 0),
        element(7, 0, DXGI_FORMAT_R16_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT),
        element(8, 0, DXGI_FORMAT_R32G32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT),
    };
    /* Slot 0: byte 0, halfword 2, explicit 20, byte 24, reset 0, halfword 2, float pair 4 */
    const UINT expected[] = {0, 0, 2, 8, 20, 24, 0, 2, 4};
    D3D12_INPUT_ELEMENT_DESC original[9], output[32];
    memcpy(original, input, sizeof input);
    D3D12_INPUT_LAYOUT_DESC layout = {input, 9};
    CHECK(resolve_append_offsets(&layout, output));
    for (UINT i = 0; i < 9; i++) {
        CHECK(output[i].AlignedByteOffset == expected[i]);
        D3D12_INPUT_ELEMENT_DESC preserved = output[i];
        preserved.AlignedByteOffset = input[i].AlignedByteOffset;
        CHECK(!memcmp(&preserved, &input[i], sizeof preserved));
    }
    CHECK(!memcmp(input, original, sizeof input));
    keep_append_offsets = 1;
    CHECK(!resolve_append_offsets(&layout, output));
    keep_append_offsets = 0;
    for (UINT i = 0; i < 9; i++) input[i].AlignedByteOffset = expected[i];
    CHECK(!resolve_append_offsets(&layout, output));
    input[0].Format = DXGI_FORMAT_BC1_UNORM;
    input[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    CHECK(!resolve_append_offsets(&layout, output));
    input[0].Format = DXGI_FORMAT_R8_UNORM;
    input[0].InputSlot = D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
    CHECK(!resolve_append_offsets(&layout, output));
    input[0].InputSlot = D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1;
    CHECK(resolve_append_offsets(&layout, output));
    layout.NumElements = D3D12_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT + 1;
    CHECK(!resolve_append_offsets(&layout, output));
    layout.NumElements = 0;
    CHECK(!resolve_append_offsets(&layout, output));
    CHECK(!resolve_append_offsets(NULL, output));
    layout.NumElements = 1;
    layout.pInputElementDescs = NULL;
    CHECK(!resolve_append_offsets(&layout, output));
    printf("offset, alignment, interleaved slot and passthrough checks complete\n");
}

static void livery(void)
{
    const DXGI_FORMAT formats[] = {DXGI_FORMAT_R16G16_SNORM, DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R16G16_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R10G10B10A2_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM};
    const UINT expected[] = {0, 4, 8, 12, 16, 20, 24, 28, 32};
    D3D12_INPUT_ELEMENT_DESC input[9], output[32];
    for (UINT i = 0; i < 9; i++) input[i] = element(i, 0, formats[i], D3D12_APPEND_ALIGNED_ELEMENT);
    D3D12_INPUT_LAYOUT_DESC layout = {input, 9};
    CHECK(resolve_append_offsets(&layout, output));
    for (UINT i = 0; i < 9; i++) CHECK(output[i].AlignedByteOffset == expected[i]);
    printf("documented nine-element FH6 livery layout resolves to 36 bytes\n");
}

static void streams(void)
{
    struct Stream {
        UINT root_type;
        ID3D12RootSignature *root;
        UINT layout_type;
        D3D12_INPUT_LAYOUT_DESC layout;
        UINT mask_type;
        UINT mask;
    };
    _Static_assert(sizeof(struct Stream) == 48, "test stream must be pointer padded");
    D3D12_INPUT_ELEMENT_DESC input[] = {element(0, 0, DXGI_FORMAT_R8_UNORM, D3D12_APPEND_ALIGNED_ELEMENT), element(1, 0, DXGI_FORMAT_R32_FLOAT, D3D12_APPEND_ALIGNED_ELEMENT)};
    struct Stream stream = {0}, original;
    stream.root_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE;
    stream.root = (ID3D12RootSignature *)(UINT_PTR)0x12345678;
    stream.layout_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT;
    stream.layout.pInputElementDescs = input;
    stream.layout.NumElements = 2;
    stream.mask_type = D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK;
    stream.mask = 0xa16c73f2;
    original = stream;
    D3D12_PIPELINE_STATE_STREAM_DESC in = {sizeof stream, &stream}, out = {0};
    D3D12_INPUT_ELEMENT_DESC elements[32];
    void *copy = NULL;
    CHECK(rewrite_stream(&in, &out, &copy, elements));
    CHECK(copy && out.SizeInBytes == sizeof stream && out.pPipelineStateSubobjectStream == copy);
    if (copy) {
        struct Stream *rewritten = copy;
        CHECK(rewritten->layout.pInputElementDescs == elements);
        CHECK(elements[0].AlignedByteOffset == 0 && elements[1].AlignedByteOffset == 4);
        rewritten->layout.pInputElementDescs = input;
        CHECK(!memcmp(rewritten, &original, sizeof original));
        free(copy);
    }
    CHECK(!memcmp(&stream, &original, sizeof original));
    copy = NULL;
    stream.mask_type = 999;
    CHECK(!rewrite_stream(&in, &out, &copy, elements) && !copy);
    stream = original;
    in.SizeInBytes = offsetof(struct Stream, layout) + sizeof stream.layout - 1;
    CHECK(!rewrite_stream(&in, &out, &copy, elements) && !copy);
    in.SizeInBytes = sizeof stream;
    keep_append_offsets = 1;
    CHECK(!rewrite_stream(&in, &out, &copy, elements) && !copy);
    keep_append_offsets = 0;
    CHECK(!rewrite_stream(NULL, &out, &copy, elements));
    printf("pipeline stream rewrite, preservation, unknown and truncated cases complete\n");
}

int main(void)
{
    STREAM_ABI(0, ID3D12RootSignature *);
    for (UINT type = 1; type <= 6; type++) STREAM_ABI(type, D3D12_SHADER_BYTECODE);
    STREAM_ABI(7, D3D12_STREAM_OUTPUT_DESC);
    STREAM_ABI(8, D3D12_BLEND_DESC);
    STREAM_ABI(9, UINT);
    STREAM_ABI(10, D3D12_RASTERIZER_DESC);
    STREAM_ABI(11, D3D12_DEPTH_STENCIL_DESC);
    STREAM_ABI(12, D3D12_INPUT_LAYOUT_DESC);
    STREAM_ABI(13, D3D12_INDEX_BUFFER_STRIP_CUT_VALUE);
    STREAM_ABI(14, D3D12_PRIMITIVE_TOPOLOGY_TYPE);
    STREAM_ABI(15, D3D12_RT_FORMAT_ARRAY);
    STREAM_ABI(16, DXGI_FORMAT);
    STREAM_ABI(17, DXGI_SAMPLE_DESC);
    STREAM_ABI(18, UINT);
    STREAM_ABI(19, D3D12_CACHED_PIPELINE_STATE);
    STREAM_ABI(20, D3D12_PIPELINE_STATE_FLAGS);
    STREAM_ABI(21, D3D12_DEPTH_STENCIL_DESC1);
    STREAM_ABI(22, D3D12_VIEW_INSTANCING_DESC);
    STREAM_ABI(24, D3D12_SHADER_BYTECODE);
    STREAM_ABI(25, D3D12_SHADER_BYTECODE);
    STREAM_ABI(26, D3D12_DEPTH_STENCIL_DESC2);
    STREAM_ABI(27, D3D12_RASTERIZER_DESC1);
    STREAM_ABI(28, D3D12_RASTERIZER_DESC2);
    offsets();
    livery();
    streams();
    printf("RESULT: %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
