/* Copyright (c) Microsoft Corporation, licensed under the MIT license
 * Test-only excerpts from microsoft/DirectX-Headers include/directx/d3d12.h
 * Git blob 99562647170a141907183fa90809cfe61444bec4, retrieved 2026-10-24
 * These newer declarations are absent from the installed MinGW header
 */
typedef struct D3D12_RT_FORMAT_ARRAY
    {
    DXGI_FORMAT RTFormats[ 8 ];
    UINT NumRenderTargets;
    } 	D3D12_RT_FORMAT_ARRAY;

typedef struct D3D12_DEPTH_STENCILOP_DESC1
    {
    D3D12_STENCIL_OP StencilFailOp;
    D3D12_STENCIL_OP StencilDepthFailOp;
    D3D12_STENCIL_OP StencilPassOp;
    D3D12_COMPARISON_FUNC StencilFunc;
    UINT8 StencilReadMask;
    UINT8 StencilWriteMask;
    } 	D3D12_DEPTH_STENCILOP_DESC1;

typedef struct D3D12_DEPTH_STENCIL_DESC2
    {
    BOOL DepthEnable;
    D3D12_DEPTH_WRITE_MASK DepthWriteMask;
    D3D12_COMPARISON_FUNC DepthFunc;
    BOOL StencilEnable;
    D3D12_DEPTH_STENCILOP_DESC1 FrontFace;
    D3D12_DEPTH_STENCILOP_DESC1 BackFace;
    BOOL DepthBoundsTestEnable;
    } 	D3D12_DEPTH_STENCIL_DESC2;

typedef 
enum D3D12_LINE_RASTERIZATION_MODE
    {
        D3D12_LINE_RASTERIZATION_MODE_ALIASED	= 0,
        D3D12_LINE_RASTERIZATION_MODE_ALPHA_ANTIALIASED	= ( D3D12_LINE_RASTERIZATION_MODE_ALIASED + 1 ) ,
        D3D12_LINE_RASTERIZATION_MODE_QUADRILATERAL_WIDE	= ( D3D12_LINE_RASTERIZATION_MODE_ALPHA_ANTIALIASED + 1 ) ,
        D3D12_LINE_RASTERIZATION_MODE_QUADRILATERAL_NARROW	= ( D3D12_LINE_RASTERIZATION_MODE_QUADRILATERAL_WIDE + 1 ) 
    } 	D3D12_LINE_RASTERIZATION_MODE;

typedef struct D3D12_RASTERIZER_DESC1
    {
    D3D12_FILL_MODE FillMode;
    D3D12_CULL_MODE CullMode;
    BOOL FrontCounterClockwise;
    FLOAT DepthBias;
    FLOAT DepthBiasClamp;
    FLOAT SlopeScaledDepthBias;
    BOOL DepthClipEnable;
    BOOL MultisampleEnable;
    BOOL AntialiasedLineEnable;
    UINT ForcedSampleCount;
    D3D12_CONSERVATIVE_RASTERIZATION_MODE ConservativeRaster;
    } 	D3D12_RASTERIZER_DESC1;

typedef struct D3D12_RASTERIZER_DESC2
    {
    D3D12_FILL_MODE FillMode;
    D3D12_CULL_MODE CullMode;
    BOOL FrontCounterClockwise;
    FLOAT DepthBias;
    FLOAT DepthBiasClamp;
    FLOAT SlopeScaledDepthBias;
    BOOL DepthClipEnable;
    D3D12_LINE_RASTERIZATION_MODE LineRasterizationMode;
    UINT ForcedSampleCount;
    D3D12_CONSERVATIVE_RASTERIZATION_MODE ConservativeRaster;
    } 	D3D12_RASTERIZER_DESC2;
