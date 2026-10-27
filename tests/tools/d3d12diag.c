/* d3d12diag.exe: draws a red triangle into a 64x64 target in several ways
 * (vertex ID only, vertex buffer, root constants, root CBV, CBV from a
 * descriptor table, depth test, indexed draw, a matrix-transformed indexed
 * cube with depth like a simple cube demo), reads the centre pixel back and
 * writes one line per case to d3d12diag.txt next to the exe. */
#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <stdio.h>
#include <string.h>

static FILE *out;
static void say(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt); vfprintf(out, fmt, ap); va_end(ap);
   va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
   fflush(out);
}

static PFN_D3D12_CREATE_DEVICE pCreateDevice;
static PFN_D3D12_SERIALIZE_ROOT_SIGNATURE pSerialize;
static pD3DCompile pCompile;

static ID3D12Device *dev;
static ID3D12CommandQueue *queue;
static ID3D12CommandAllocator *alloc;
static ID3D12GraphicsCommandList *list;
static ID3D12Fence *fence;
static UINT64 fence_value;
static HANDLE fence_event;

#define W 64
#define H 64

static ID3D12Resource *make_buffer(D3D12_HEAP_TYPE type, UINT64 size, D3D12_RESOURCE_STATES state)
{
   D3D12_HEAP_PROPERTIES hp = {type};
   D3D12_RESOURCE_DESC rd = {D3D12_RESOURCE_DIMENSION_BUFFER, 0, size, 1, 1, 1, DXGI_FORMAT_UNKNOWN,
                             {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE};
   ID3D12Resource *r = NULL;
   HRESULT hr = ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &rd, state, NULL,
                                                     &IID_ID3D12Resource, (void **)&r);
   if (FAILED(hr)) say("CreateCommittedResource(buffer) %#lx\n", hr);
   return r;
}

static ID3D12Resource *upload(const void *data, UINT64 size)
{
   ID3D12Resource *r = make_buffer(D3D12_HEAP_TYPE_UPLOAD, size, D3D12_RESOURCE_STATE_GENERIC_READ);
   void *p; D3D12_RANGE none = {0, 0};
   ID3D12Resource_Map(r, 0, &none, &p);
   memcpy(p, data, size);
   ID3D12Resource_Unmap(r, 0, NULL);
   return r;
}

static void wait_gpu(void)
{
   ID3D12CommandQueue_Signal(queue, fence, ++fence_value);
   if (ID3D12Fence_GetCompletedValue(fence) < fence_value) {
      ID3D12Fence_SetEventOnCompletion(fence, fence_value, fence_event);
      WaitForSingleObject(fence_event, 5000);
   }
}

static ID3DBlob *compile(const char *src, const char *entry, const char *target)
{
   ID3DBlob *code = NULL, *err = NULL;
   HRESULT hr = pCompile(src, strlen(src), NULL, NULL, NULL, entry, target, 0, 0, &code, &err);
   if (FAILED(hr)) say("D3DCompile %s %s: %#lx %s\n", entry, target, hr,
                       err ? (char *)ID3D10Blob_GetBufferPointer(err) : "");
   return code;
}

static ID3D12RootSignature *root_sig(const D3D12_ROOT_PARAMETER *params, UINT n, BOOL input_layout)
{
   D3D12_ROOT_SIGNATURE_DESC d = {n, params, 0, NULL,
                                  input_layout ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT : 0};
   ID3DBlob *blob = NULL, *err = NULL;
   HRESULT hr = pSerialize(&d, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err);
   if (FAILED(hr)) { say("SerializeRootSignature %#lx\n", hr); return NULL; }
   ID3D12RootSignature *rs = NULL;
   hr = ID3D12Device_CreateRootSignature(dev, 0, ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob),
                                         &IID_ID3D12RootSignature, (void **)&rs);
   if (FAILED(hr)) say("CreateRootSignature %#lx\n", hr);
   return rs;
}

static ID3D12PipelineState *pso(ID3D12RootSignature *rs, const char *vs_src, const char *ps_src,
                                const D3D12_INPUT_ELEMENT_DESC *il, UINT nil, BOOL depth)
{
   ID3DBlob *vs = compile(vs_src, "main", "vs_5_0"), *ps = compile(ps_src, "main", "ps_5_0");
   if (!vs || !ps) return NULL;
   D3D12_GRAPHICS_PIPELINE_STATE_DESC d;
   memset(&d, 0, sizeof(d));
   d.pRootSignature = rs;
   d.VS.pShaderBytecode = ID3D10Blob_GetBufferPointer(vs); d.VS.BytecodeLength = ID3D10Blob_GetBufferSize(vs);
   d.PS.pShaderBytecode = ID3D10Blob_GetBufferPointer(ps); d.PS.BytecodeLength = ID3D10Blob_GetBufferSize(ps);
   d.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
   d.SampleMask = ~0u;
   d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
   d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
   d.RasterizerState.DepthClipEnable = TRUE;
   d.DepthStencilState.DepthEnable = depth;
   d.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
   d.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
   d.InputLayout.pInputElementDescs = il; d.InputLayout.NumElements = nil;
   d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
   d.NumRenderTargets = 1;
   d.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
   d.DSVFormat = depth ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_UNKNOWN;
   d.SampleDesc.Count = 1;
   ID3D12PipelineState *p = NULL;
   HRESULT hr = ID3D12Device_CreateGraphicsPipelineState(dev, &d, &IID_ID3D12PipelineState, (void **)&p);
   if (FAILED(hr)) say("CreateGraphicsPipelineState %#lx\n", hr);
   return p;
}

static ID3D12Resource *rt, *ds, *readback;
static ID3D12DescriptorHeap *rtv_heap, *dsv_heap, *cbv_heap;

/* Clear, let `record` draw, copy the target to the readback buffer and return
 * the centre pixel (0xAABBGGRR). */
typedef void (*record_fn)(void);
static UINT32 run(record_fn record, BOOL depth)
{
   ID3D12CommandAllocator_Reset(alloc);
   ID3D12GraphicsCommandList_Reset(list, alloc, NULL);
   D3D12_CPU_DESCRIPTOR_HANDLE rtv = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(rtv_heap);
   D3D12_CPU_DESCRIPTOR_HANDLE dsv = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(dsv_heap);
   const float clear[4] = {0.1f, 0.1f, 0.1f, 1.0f};
   ID3D12GraphicsCommandList_ClearRenderTargetView(list, rtv, clear, 0, NULL);
   if (depth)
      ID3D12GraphicsCommandList_ClearDepthStencilView(list, dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);
   ID3D12GraphicsCommandList_OMSetRenderTargets(list, 1, &rtv, FALSE, depth ? &dsv : NULL);
   D3D12_VIEWPORT vp = {0, 0, W, H, 0, 1};
   D3D12_RECT sc = {0, 0, W, H};
   ID3D12GraphicsCommandList_RSSetViewports(list, 1, &vp);
   ID3D12GraphicsCommandList_RSSetScissorRects(list, 1, &sc);
   ID3D12GraphicsCommandList_IASetPrimitiveTopology(list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   record();

   D3D12_RESOURCE_BARRIER b = {D3D12_RESOURCE_BARRIER_TYPE_TRANSITION};
   b.Transition.pResource = rt;
   b.Transition.Subresource = 0;
   b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
   b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
   ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &b);
   D3D12_TEXTURE_COPY_LOCATION dst = {readback, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
   dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
   dst.PlacedFootprint.Footprint.Width = W; dst.PlacedFootprint.Footprint.Height = H;
   dst.PlacedFootprint.Footprint.Depth = 1; dst.PlacedFootprint.Footprint.RowPitch = 256;
   D3D12_TEXTURE_COPY_LOCATION src = {rt, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
   src.SubresourceIndex = 0;
   ID3D12GraphicsCommandList_CopyTextureRegion(list, &dst, 0, 0, 0, &src, NULL);
   b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
   b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
   ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &b);
   ID3D12GraphicsCommandList_Close(list);
   ID3D12CommandQueue_ExecuteCommandLists(queue, 1, (ID3D12CommandList **)&list);
   wait_gpu();

   UINT32 *p; D3D12_RANGE r = {0, 256 * H};
   ID3D12Resource_Map(readback, 0, &r, (void **)&p);
   UINT32 v = p[(H / 2) * 64 + W / 2];
   D3D12_RANGE none = {0, 0};
   ID3D12Resource_Unmap(readback, 0, &none);
   return v;
}

/* --- cases --- */
static ID3D12RootSignature *rs_empty, *rs_il, *rs_const, *rs_cbv, *rs_table, *rs_cube;
static ID3D12PipelineState *p_vid, *p_vb, *p_const, *p_cbv, *p_table, *p_depth, *p_cube;
static ID3D12Resource *vb, *ib, *cb, *cube_vb, *cube_ib, *cube_cb;

static const char *vs_vid =
   "float4 main(uint id : SV_VertexID) : SV_Position {"
   "  float2 p = float2((id << 1) & 2, id & 2);"
   "  return float4(p * float2(2, -2) + float2(-1, 1), 0.5, 1); }";
static const char *vs_pos =
   "float4 main(float2 p : POSITION) : SV_Position { return float4(p, 0.5, 1); }";
static const char *ps_red = "float4 main() : SV_Target { return float4(1, 0, 0, 1); }";
static const char *ps_const =
   "cbuffer c : register(b0) { float4 color; }; float4 main() : SV_Target { return color; }";
static const char *vs_cube =
   "cbuffer c : register(b0) { float4x4 mvp; };"
   "struct o { float4 pos : SV_Position; float3 col : COLOR; };"
   "o main(float3 p : POSITION, float3 c : COLOR) { o r; r.pos = mul(mvp, float4(p, 1)); r.col = c; return r; }";
static const char *ps_cube =
   "float4 main(float4 pos : SV_Position, float3 c : COLOR) : SV_Target { return float4(c, 1); }";

static void rec_vid(void)
{
   ID3D12GraphicsCommandList_SetGraphicsRootSignature(list, rs_empty);
   ID3D12GraphicsCommandList_SetPipelineState(list, p_vid);
   ID3D12GraphicsCommandList_DrawInstanced(list, 3, 1, 0, 0);
}
static void bind_vb(void)
{
   D3D12_VERTEX_BUFFER_VIEW v = {ID3D12Resource_GetGPUVirtualAddress(vb), 24, 8};
   ID3D12GraphicsCommandList_IASetVertexBuffers(list, 0, 1, &v);
}
static void rec_vb(void)
{
   ID3D12GraphicsCommandList_SetGraphicsRootSignature(list, rs_il);
   ID3D12GraphicsCommandList_SetPipelineState(list, p_vb);
   bind_vb();
   ID3D12GraphicsCommandList_DrawInstanced(list, 3, 1, 0, 0);
}
static void rec_const(void)
{
   const float green[4] = {0, 1, 0, 1};
   ID3D12GraphicsCommandList_SetGraphicsRootSignature(list, rs_const);
   ID3D12GraphicsCommandList_SetPipelineState(list, p_const);
   ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(list, 0, 4, green, 0);
   bind_vb();
   ID3D12GraphicsCommandList_DrawInstanced(list, 3, 1, 0, 0);
}
static void rec_cbv(void)
{
   ID3D12GraphicsCommandList_SetGraphicsRootSignature(list, rs_cbv);
   ID3D12GraphicsCommandList_SetPipelineState(list, p_cbv);
   ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(list, 0, ID3D12Resource_GetGPUVirtualAddress(cb));
   bind_vb();
   ID3D12GraphicsCommandList_DrawInstanced(list, 3, 1, 0, 0);
}
static void rec_table(void)
{
   ID3D12GraphicsCommandList_SetGraphicsRootSignature(list, rs_table);
   ID3D12GraphicsCommandList_SetDescriptorHeaps(list, 1, &cbv_heap);
   ID3D12GraphicsCommandList_SetPipelineState(list, p_table);
   ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(list, 0,
      ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(cbv_heap));
   bind_vb();
   ID3D12GraphicsCommandList_DrawInstanced(list, 3, 1, 0, 0);
}
static void rec_depth(void)
{
   ID3D12GraphicsCommandList_SetGraphicsRootSignature(list, rs_il);
   ID3D12GraphicsCommandList_SetPipelineState(list, p_depth);
   bind_vb();
   ID3D12GraphicsCommandList_DrawInstanced(list, 3, 1, 0, 0);
}
static void rec_indexed(void)
{
   ID3D12GraphicsCommandList_SetGraphicsRootSignature(list, rs_il);
   ID3D12GraphicsCommandList_SetPipelineState(list, p_vb);
   bind_vb();
   D3D12_INDEX_BUFFER_VIEW i = {ID3D12Resource_GetGPUVirtualAddress(ib), 6, DXGI_FORMAT_R16_UINT};
   ID3D12GraphicsCommandList_IASetIndexBuffer(list, &i);
   ID3D12GraphicsCommandList_DrawIndexedInstanced(list, 3, 1, 0, 0, 0);
}
static void rec_cube(void)
{
   ID3D12GraphicsCommandList_SetGraphicsRootSignature(list, rs_cube);
   ID3D12GraphicsCommandList_SetPipelineState(list, p_cube);
   ID3D12GraphicsCommandList_SetGraphicsRootConstantBufferView(list, 0, ID3D12Resource_GetGPUVirtualAddress(cube_cb));
   D3D12_VERTEX_BUFFER_VIEW v = {ID3D12Resource_GetGPUVirtualAddress(cube_vb), 8 * 24, 24};
   ID3D12GraphicsCommandList_IASetVertexBuffers(list, 0, 1, &v);
   D3D12_INDEX_BUFFER_VIEW i = {ID3D12Resource_GetGPUVirtualAddress(cube_ib), 36 * 2, DXGI_FORMAT_R16_UINT};
   ID3D12GraphicsCommandList_IASetIndexBuffer(list, &i);
   ID3D12GraphicsCommandList_DrawIndexedInstanced(list, 36, 1, 0, 0, 0);
}

/* Full-screen red triangle into a w x h target of `fmt` with `samples`,
 * resolved if multisampled, copied to a row-pitch-aligned readback buffer.
 * Checks the centre and the last pixel of the last row. */
static void check_target(const char *name, DXGI_FORMAT fmt, UINT w, UINT h, UINT samples, UINT32 want)
{
   D3D12_HEAP_PROPERTIES def = {D3D12_HEAP_TYPE_DEFAULT};
   D3D12_RESOURCE_DESC td = {D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, w, h, 1, 1, fmt,
                             {samples, 0}, D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET};
   ID3D12Resource *t = NULL, *res = NULL;
   HRESULT hr = ID3D12Device_CreateCommittedResource(dev, &def, D3D12_HEAP_FLAG_NONE, &td,
      D3D12_RESOURCE_STATE_RENDER_TARGET, NULL, &IID_ID3D12Resource, (void **)&t);
   if (samples > 1) {
      td.SampleDesc.Count = 1; td.Flags = D3D12_RESOURCE_FLAG_NONE;
      hr |= ID3D12Device_CreateCommittedResource(dev, &def, D3D12_HEAP_FLAG_NONE, &td,
         D3D12_RESOURCE_STATE_RESOLVE_DEST, NULL, &IID_ID3D12Resource, (void **)&res);
   }
   const UINT pitch = (w * 4 + 255) & ~255u;
   ID3D12Resource *rb = make_buffer(D3D12_HEAP_TYPE_READBACK, (UINT64)pitch * h, D3D12_RESOURCE_STATE_COPY_DEST);
   if (FAILED(hr) || !rb) { say("FAIL %-38s resources %#lx\n", name, hr); return; }
   D3D12_DESCRIPTOR_HEAP_DESC hd = {D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1};
   ID3D12DescriptorHeap *heap;
   ID3D12Device_CreateDescriptorHeap(dev, &hd, &IID_ID3D12DescriptorHeap, (void **)&heap);
   D3D12_CPU_DESCRIPTOR_HANDLE rtv = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(heap);
   ID3D12Device_CreateRenderTargetView(dev, t, NULL, rtv);

   ID3DBlob *vs = compile(vs_vid, "main", "vs_5_0"), *ps = compile(ps_red, "main", "ps_5_0");
   D3D12_GRAPHICS_PIPELINE_STATE_DESC d;
   memset(&d, 0, sizeof(d));
   d.pRootSignature = rs_empty;
   d.VS.pShaderBytecode = ID3D10Blob_GetBufferPointer(vs); d.VS.BytecodeLength = ID3D10Blob_GetBufferSize(vs);
   d.PS.pShaderBytecode = ID3D10Blob_GetBufferPointer(ps); d.PS.BytecodeLength = ID3D10Blob_GetBufferSize(ps);
   d.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
   d.SampleMask = ~0u;
   d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
   d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
   d.RasterizerState.DepthClipEnable = TRUE;
   d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
   d.NumRenderTargets = 1; d.RTVFormats[0] = fmt; d.SampleDesc.Count = samples;
   ID3D12PipelineState *p = NULL;
   hr = ID3D12Device_CreateGraphicsPipelineState(dev, &d, &IID_ID3D12PipelineState, (void **)&p);
   if (FAILED(hr)) { say("FAIL %-38s pipeline %#lx\n", name, hr); return; }

   ID3D12CommandAllocator_Reset(alloc);
   ID3D12GraphicsCommandList_Reset(list, alloc, NULL);
   const float clear[4] = {0.1f, 0.1f, 0.1f, 1.0f};
   ID3D12GraphicsCommandList_ClearRenderTargetView(list, rtv, clear, 0, NULL);
   ID3D12GraphicsCommandList_OMSetRenderTargets(list, 1, &rtv, FALSE, NULL);
   D3D12_VIEWPORT vp = {0, 0, (float)w, (float)h, 0, 1};
   D3D12_RECT sc = {0, 0, (LONG)w, (LONG)h};
   ID3D12GraphicsCommandList_RSSetViewports(list, 1, &vp);
   ID3D12GraphicsCommandList_RSSetScissorRects(list, 1, &sc);
   ID3D12GraphicsCommandList_IASetPrimitiveTopology(list, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   ID3D12GraphicsCommandList_SetGraphicsRootSignature(list, rs_empty);
   ID3D12GraphicsCommandList_SetPipelineState(list, p);
   ID3D12GraphicsCommandList_DrawInstanced(list, 3, 1, 0, 0);

   D3D12_RESOURCE_BARRIER b = {D3D12_RESOURCE_BARRIER_TYPE_TRANSITION};
   b.Transition.pResource = t;
   b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
   ID3D12Resource *src_res = t;
   if (samples > 1) {
      b.Transition.StateAfter = D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
      ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &b);
      ID3D12GraphicsCommandList_ResolveSubresource(list, res, 0, t, 0, fmt);
      b.Transition.pResource = res;
      b.Transition.StateBefore = D3D12_RESOURCE_STATE_RESOLVE_DEST;
      src_res = res;
   }
   b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
   ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &b);
   D3D12_TEXTURE_COPY_LOCATION dst = {rb, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
   dst.PlacedFootprint.Footprint.Format = fmt;
   dst.PlacedFootprint.Footprint.Width = w; dst.PlacedFootprint.Footprint.Height = h;
   dst.PlacedFootprint.Footprint.Depth = 1; dst.PlacedFootprint.Footprint.RowPitch = pitch;
   D3D12_TEXTURE_COPY_LOCATION src = {src_res, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
   ID3D12GraphicsCommandList_CopyTextureRegion(list, &dst, 0, 0, 0, &src, NULL);
   ID3D12GraphicsCommandList_Close(list);
   ID3D12CommandQueue_ExecuteCommandLists(queue, 1, (ID3D12CommandList **)&list);
   wait_gpu();

   UINT8 *m; D3D12_RANGE r = {0, (SIZE_T)pitch * h};
   ID3D12Resource_Map(rb, 0, &r, (void **)&m);
   UINT32 c = *(UINT32 *)(m + (h / 2) * pitch + (w / 2) * 4);
   UINT32 l = *(UINT32 *)(m + (h - 1) * pitch + (w - 1) * 4);
   D3D12_RANGE none = {0, 0};
   ID3D12Resource_Unmap(rb, 0, &none);
   say("%s %-38s centre %08x last %08x (want %08x)\n", c == want && l == want ? "PASS" : "FAIL", name, c, l, want);
}

static void check(const char *name, record_fn f, BOOL depth, UINT32 want)
{
   UINT32 v = run(f, depth);
   say("%s %-38s centre pixel %08x (want %08x)\n", v == want ? "PASS" : "FAIL", name, v, want);
}

static LRESULT CALLBACK wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
   return DefWindowProcA(h, m, w, l);
}

/* Clear `hwnd`'s swapchain to `color` for `ms`, presenting each frame. */
static void present_test(IDXGIFactory2 *f, HWND hwnd, const float color[4], DWORD ms, const char *name)
{
   DXGI_SWAP_CHAIN_DESC1 sd;
   memset(&sd, 0, sizeof(sd));
   sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
   sd.SampleDesc.Count = 1;
   sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
   sd.BufferCount = 2;
   sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
   IDXGISwapChain1 *sc1 = NULL;
   HRESULT hr = IDXGIFactory2_CreateSwapChainForHwnd(f, (IUnknown *)queue, hwnd, &sd, NULL, NULL, &sc1);
   say("%s: CreateSwapChainForHwnd %#lx\n", name, hr);
   if (FAILED(hr)) return;
   IDXGISwapChain3 *sc = NULL;
   IDXGISwapChain1_QueryInterface(sc1, &IID_IDXGISwapChain3, (void **)&sc);
   D3D12_DESCRIPTOR_HEAP_DESC hd = {D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2};
   ID3D12DescriptorHeap *heap;
   ID3D12Device_CreateDescriptorHeap(dev, &hd, &IID_ID3D12DescriptorHeap, (void **)&heap);
   UINT inc = ID3D12Device_GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
   ID3D12Resource *bufs[2];
   D3D12_CPU_DESCRIPTOR_HANDLE rtv[2];
   for (int i = 0; i < 2; i++) {
      IDXGISwapChain3_GetBuffer(sc, i, &IID_ID3D12Resource, (void **)&bufs[i]);
      rtv[i] = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(heap);
      rtv[i].ptr += i * inc;
      ID3D12Device_CreateRenderTargetView(dev, bufs[i], NULL, rtv[i]);
   }
   DWORD start = GetTickCount();
   unsigned frames = 0;
   while (GetTickCount() - start < ms) {
      MSG msg;
      while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
      UINT i = IDXGISwapChain3_GetCurrentBackBufferIndex(sc);
      ID3D12CommandAllocator_Reset(alloc);
      ID3D12GraphicsCommandList_Reset(list, alloc, NULL);
      D3D12_RESOURCE_BARRIER b = {D3D12_RESOURCE_BARRIER_TYPE_TRANSITION};
      b.Transition.pResource = bufs[i];
      b.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
      b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
      ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &b);
      ID3D12GraphicsCommandList_ClearRenderTargetView(list, rtv[i], color, 0, NULL);
      b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
      b.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
      ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &b);
      ID3D12GraphicsCommandList_Close(list);
      ID3D12CommandQueue_ExecuteCommandLists(queue, 1, (ID3D12CommandList **)&list);
      hr = IDXGISwapChain3_Present(sc, 1, 0);
      if (FAILED(hr)) { say("%s: Present %#lx\n", name, hr); break; }
      wait_gpu();
      frames++;
   }
   say("%s: %u frames presented\n", name, frames);
   for (int i = 0; i < 2; i++) ID3D12Resource_Release(bufs[i]);
   IDXGISwapChain3_Release(sc);
   IDXGISwapChain1_Release(sc1);
}

static void run_present(void)
{
   HMODULE dxgi = LoadLibraryA("dxgi.dll");
   HRESULT (WINAPI *create)(UINT, REFIID, void **) =
      (void *)GetProcAddress(dxgi, "CreateDXGIFactory2");
   IDXGIFactory2 *f = NULL;
   HRESULT hr = create(0, &IID_IDXGIFactory2, (void **)&f);
   say("CreateDXGIFactory2 %#lx\n", hr);
   if (FAILED(hr)) return;
   WNDCLASSA wc = {0};
   wc.lpfnWndProc = wndproc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "d3d12diag";
   wc.hbrBackground = (HBRUSH)GetStockObject(GRAY_BRUSH);
   RegisterClassA(&wc);
   HWND top = CreateWindowA("d3d12diag", "d3d12diag: red = top-level, then green = child",
                            WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 50, 640, 480, NULL, NULL, wc.hInstance, NULL);
   UpdateWindow(top);
   const float red[4] = {1, 0, 0, 1}, green[4] = {0, 1, 0, 1};
   present_test(f, top, red, 4000, "top-level window (red)");
   HWND child = CreateWindowA("d3d12diag", NULL, WS_CHILD | WS_VISIBLE, 100, 100, 400, 250, top, NULL,
                              wc.hInstance, NULL);
   present_test(f, child, green, 4000, "child window (green)");
   DestroyWindow(top);
}

int main(void)
{
   char path[MAX_PATH];
   GetModuleFileNameA(NULL, path, MAX_PATH);
   char *slash = strrchr(path, '\\');
   strcpy(slash ? slash + 1 : path, "d3d12diag.txt");
   out = fopen(path, "w");
   if (!out) out = stdout;

   HMODULE d3d12 = LoadLibraryA("d3d12.dll");
   HMODULE dc = LoadLibraryA("d3dcompiler_47.dll");
   if (!d3d12 || !dc) { say("LoadLibrary d3d12 %p d3dcompiler_47 %p\n", d3d12, dc); return 1; }
   pCreateDevice = (PFN_D3D12_CREATE_DEVICE)GetProcAddress(d3d12, "D3D12CreateDevice");
   pSerialize = (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)GetProcAddress(d3d12, "D3D12SerializeRootSignature");
   pCompile = (pD3DCompile)GetProcAddress(dc, "D3DCompile");

   HRESULT hr = pCreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&dev);
   say("D3D12CreateDevice FL11_0: %#lx\n", hr);
   if (FAILED(hr)) return 1;
   D3D12_COMMAND_QUEUE_DESC qd = {D3D12_COMMAND_LIST_TYPE_DIRECT};
   ID3D12Device_CreateCommandQueue(dev, &qd, &IID_ID3D12CommandQueue, (void **)&queue);
   ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void **)&alloc);
   ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, NULL,
                                  &IID_ID3D12GraphicsCommandList, (void **)&list);
   ID3D12GraphicsCommandList_Close(list);
   ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void **)&fence);
   fence_event = CreateEventA(NULL, FALSE, FALSE, NULL);

   /* Targets and heaps. */
   D3D12_HEAP_PROPERTIES def = {D3D12_HEAP_TYPE_DEFAULT};
   D3D12_RESOURCE_DESC td = {D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, W, H, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
                             {1, 0}, D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET};
   hr = ID3D12Device_CreateCommittedResource(dev, &def, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_RENDER_TARGET,
                                             NULL, &IID_ID3D12Resource, (void **)&rt);
   td.Format = DXGI_FORMAT_D32_FLOAT; td.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
   D3D12_CLEAR_VALUE cv = {DXGI_FORMAT_D32_FLOAT}; cv.DepthStencil.Depth = 1.0f;
   hr |= ID3D12Device_CreateCommittedResource(dev, &def, D3D12_HEAP_FLAG_NONE, &td, D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                              &cv, &IID_ID3D12Resource, (void **)&ds);
   readback = make_buffer(D3D12_HEAP_TYPE_READBACK, 256 * H, D3D12_RESOURCE_STATE_COPY_DEST);
   say("targets %#lx\n", hr);
   D3D12_DESCRIPTOR_HEAP_DESC hd = {D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1};
   ID3D12Device_CreateDescriptorHeap(dev, &hd, &IID_ID3D12DescriptorHeap, (void **)&rtv_heap);
   hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
   ID3D12Device_CreateDescriptorHeap(dev, &hd, &IID_ID3D12DescriptorHeap, (void **)&dsv_heap);
   hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
   ID3D12Device_CreateDescriptorHeap(dev, &hd, &IID_ID3D12DescriptorHeap, (void **)&cbv_heap);
   ID3D12Device_CreateRenderTargetView(dev, rt, NULL, ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(rtv_heap));
   ID3D12Device_CreateDepthStencilView(dev, ds, NULL, ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(dsv_heap));

   /* Geometry and constants: a triangle covering the centre. */
   const float tri[6] = {-1, -1, 0, 1, 1, -1};
   const UINT16 idx[3] = {0, 1, 2};
   const float blue[64] = {0, 0, 1, 1};
   vb = upload(tri, sizeof(tri));
   ib = upload(idx, sizeof(idx));
   cb = upload(blue, sizeof(blue));
   D3D12_CONSTANT_BUFFER_VIEW_DESC cbvd = {ID3D12Resource_GetGPUVirtualAddress(cb), 256};
   ID3D12Device_CreateConstantBufferView(dev, &cbvd, ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(cbv_heap));

   /* Cube: corners coloured by position, scaled down and rotated so the
    * front corner covers the centre; mvp is column-major as HLSL expects. */
   float cv8[8][6];
   for (int i = 0; i < 8; i++) {
      cv8[i][0] = (i & 1) ? 0.5f : -0.5f; cv8[i][1] = (i & 2) ? 0.5f : -0.5f; cv8[i][2] = (i & 4) ? 0.5f : -0.5f;
      cv8[i][3] = 1; cv8[i][4] = 0; cv8[i][5] = 0;
   }
   const UINT16 cidx[36] = {0,1,3, 0,3,2, 4,6,7, 4,7,5, 0,4,5, 0,5,1, 2,3,7, 2,7,6, 0,2,6, 0,6,4, 1,5,7, 1,7,3};
   /* mvp = translate z 0.5 * scale 0.8: maps the cube into clip z [0.1, 0.9]. */
   const float mvp[64] = {0.8f,0,0,0, 0,0.8f,0,0, 0,0,0.8f,0, 0,0,0.5f,1};
   cube_vb = upload(cv8, sizeof(cv8));
   cube_ib = upload(cidx, sizeof(cidx));
   cube_cb = upload(mvp, sizeof(mvp));

   /* Root signatures and pipelines. */
   D3D12_INPUT_ELEMENT_DESC il[] = {{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
                                     D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
   D3D12_INPUT_ELEMENT_DESC cil[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
   D3D12_ROOT_PARAMETER p_c = {D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS};
   p_c.Constants.Num32BitValues = 4; p_c.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
   D3D12_ROOT_PARAMETER p_v = {D3D12_ROOT_PARAMETER_TYPE_CBV};
   p_v.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
   D3D12_DESCRIPTOR_RANGE range = {D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, 0};
   D3D12_ROOT_PARAMETER p_t = {D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE};
   p_t.DescriptorTable.NumDescriptorRanges = 1; p_t.DescriptorTable.pDescriptorRanges = &range;
   p_t.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
   rs_empty = root_sig(NULL, 0, FALSE);
   rs_il = root_sig(NULL, 0, TRUE);
   rs_const = root_sig(&p_c, 1, TRUE);
   rs_cbv = root_sig(&p_v, 1, TRUE);
   rs_table = root_sig(&p_t, 1, TRUE);
   rs_cube = root_sig(&p_v, 1, TRUE);
   p_vid = pso(rs_empty, vs_vid, ps_red, NULL, 0, FALSE);
   p_vb = pso(rs_il, vs_pos, ps_red, il, 1, FALSE);
   p_const = pso(rs_const, vs_pos, ps_const, il, 1, FALSE);
   p_cbv = pso(rs_cbv, vs_pos, ps_const, il, 1, FALSE);
   p_table = pso(rs_table, vs_pos, ps_const, il, 1, FALSE);
   p_depth = pso(rs_il, vs_pos, ps_red, il, 1, TRUE);
   p_cube = pso(rs_cube, vs_cube, ps_cube, cil, 2, TRUE);
   say("pipelines %p %p %p %p %p %p %p\n", p_vid, p_vb, p_const, p_cbv, p_table, p_depth, p_cube);

   check("1 SV_VertexID, no buffers", rec_vid, FALSE, 0xff0000ff);
   check("2 vertex buffer + input layout", rec_vb, FALSE, 0xff0000ff);
   check("3 root constants (green)", rec_const, FALSE, 0xff00ff00);
   check("4 root CBV (blue)", rec_cbv, FALSE, 0xffff0000);
   check("5 CBV in descriptor table (blue)", rec_table, FALSE, 0xffff0000);
   check("6 depth test, D32 cleared to 1", rec_depth, TRUE, 0xff0000ff);
   check("7 indexed draw, R16 indices", rec_indexed, FALSE, 0xff0000ff);
   check("8 cube: root CBV matrix + depth + IB", rec_cube, TRUE, 0xff0000ff);
   check_target("9 RGBA8 852x586 (padded rows)", DXGI_FORMAT_R8G8B8A8_UNORM, 852, 586, 1, 0xff0000ff);
   check_target("10 BGRA8 852x586", DXGI_FORMAT_B8G8R8A8_UNORM, 852, 586, 1, 0xffff0000);
   check_target("11 RGB10A2 852x586", DXGI_FORMAT_R10G10B10A2_UNORM, 852, 586, 1, 0xc00003ff);
   check_target("12 RGBA8 852x586 MSAA4 + resolve", DXGI_FORMAT_R8G8B8A8_UNORM, 852, 586, 4, 0xff0000ff);
   check_target("13 BGRA8 852x586 MSAA4 + resolve", DXGI_FORMAT_B8G8R8A8_UNORM, 852, 586, 4, 0xffff0000);
   check_target("14 RGBA8 1172x694 (padded rows)", DXGI_FORMAT_R8G8B8A8_UNORM, 1172, 694, 1, 0xff0000ff);
   run_present();
   say("done\n");
   fclose(out);
   return 0;
}
