#include "ImageRenderer.h"
#include "Logger.h"
#include "d3dx12.h"
#include "pch.h"
#include <d3dcompiler.h>


// Vertex shader for full-screen quad
const char *g_VertexShader = R"(
struct VSInput
{
    uint vertexID : SV_VertexID;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

cbuffer Constants : register(b0)
{
    float4x4 transform;
    float rangeMin;
    float rangeMax;
    float2 padding;
    float4 channelMask; // Defined here for consistency, though unused in VS
};

PSInput main(VSInput input)
{
    PSInput output;

    // Generate quad (2 triangles, 6 vertices)
    // 0, 1, 2, 3, 4, 5
    // Tri 1: 0, 1, 2 -> (0,0), (1,0), (0,1)
    // Tri 2: 3, 4, 5 -> (0,1), (1,0), (1,1) 
    
    // UVs for the 6 vertices
    const float2 uvs[6] = {
        float2(0.0f, 0.0f), // Top-Left
        float2(1.0f, 0.0f), // Top-Right
        float2(0.0f, 1.0f), // Bottom-Left
        float2(0.0f, 1.0f), // Bottom-Left
        float2(1.0f, 0.0f), // Top-Right
        float2(1.0f, 1.0f)  // Bottom-Right
    };

    float2 uv = uvs[input.vertexID];
    output.uv = uv;

    // Transform from [0,1] to [-1,1] NDC
    float2 pos = uv * 2.0 - 1.0;
    pos.y = -pos.y; // Flip Y for D3D

    output.position = mul(float4(pos, 0.0f, 1.0f), transform);

    return output;
}
)";

// Pixel shader with color remapping
const char *g_PixelShader = R"(
struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

cbuffer Constants : register(b0)
{
    float4x4 transform;
    float rangeMin;
    float rangeMax;
    float2 padding; // Explicit alignment padding
    float4 channelMask; // Start at new register
};

Texture2D<float4> imageTexture : register(t0);
SamplerState imageSampler : register(s0);

float4 main(PSInput input) : SV_TARGET
{
    float4 color = imageTexture.Sample(imageSampler, input.uv);

    // Apply range mapping
    float rangeSize = rangeMax - rangeMin;
    if (rangeSize > 0.0001)
    {
        color.rgb = (color.rgb - rangeMin) / rangeSize;
    }

    // Mask channels
    color.rgb *= channelMask.rgb;

    // Clamp to [0,1]
    color.rgb = saturate(color.rgb);

    return color;
}
)";

ImageRenderer::ImageRenderer() {}

ImageRenderer::~ImageRenderer() { Cleanup(); }

bool ImageRenderer::Initialize(ID3D12Device *device,
                               ID3D12DescriptorHeap *srvHeap,
                               UINT srvDescriptorSize) {
  LOG("ImageRenderer::Initialize - device=%p, srvHeap=%p, srvDescriptorSize=%u",
      device, srvHeap, srvDescriptorSize);

  if (!device) {
    LOG_ERROR("ImageRenderer::Initialize - device is null!");
    return false;
  }
  if (!srvHeap) {
    LOG_ERROR("ImageRenderer::Initialize - srvHeap is null!");
    return false;
  }

  // Get a descriptor handle from the SRV heap (using descriptor 1, since ImGui
  // uses 0)
  m_srvCpuHandle = srvHeap->GetCPUDescriptorHandleForHeapStart();
  m_srvCpuHandle.ptr += srvDescriptorSize; // Descriptor 1: Input Image Texture

  m_srvGpuHandle = srvHeap->GetGPUDescriptorHandleForHeapStart();
  m_srvGpuHandle.ptr += srvDescriptorSize;

  // Descriptor 2: Output Render Target Texture (for ImGui to display)
  m_outputSrvCpuHandle = m_srvCpuHandle;
  m_outputSrvCpuHandle.ptr += srvDescriptorSize;

  m_outputSrvGpuHandle = m_srvGpuHandle;
  m_outputSrvGpuHandle.ptr += srvDescriptorSize;

  LOG("ImageRenderer::Initialize - srvCpuHandle.ptr=%llu, "
      "srvGpuHandle.ptr=%llu",
      m_srvCpuHandle.ptr, m_srvGpuHandle.ptr);

  if (!CreateRootSignature(device)) {
    LOG_ERROR("ImageRenderer::Initialize - CreateRootSignature failed!");
    return false;
  }
  LOG("ImageRenderer::Initialize - RootSignature created successfully");

  if (!CreatePipelineState(device)) {
    LOG_ERROR("ImageRenderer::Initialize - CreatePipelineState failed!");
    return false;
  }
  LOG("ImageRenderer::Initialize - PipelineState created successfully");

  return true;
}

bool ImageRenderer::CreateRootSignature(ID3D12Device *device) {
  // Root parameter for constant buffer
  D3D12_ROOT_PARAMETER rootParams[2] = {};

  // Constant buffer (transform + range)
  rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
  rootParams[0].Constants.ShaderRegister = 0;
  rootParams[0].Constants.RegisterSpace = 0;
  rootParams[0].Constants.Num32BitValues =
      24; // 4x4 matrix (16) + range (2) + padding(2) + mask(4) = 24 floats
  rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

  // Texture SRV
  D3D12_DESCRIPTOR_RANGE descRange = {};
  descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  descRange.NumDescriptors = 1;
  descRange.BaseShaderRegister = 0;
  descRange.RegisterSpace = 0;
  descRange.OffsetInDescriptorsFromTableStart =
      D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

  rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
  rootParams[1].DescriptorTable.NumDescriptorRanges = 1;
  rootParams[1].DescriptorTable.pDescriptorRanges = &descRange;
  rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  // Static sampler - use POINT filter for pixel-perfect rendering
  D3D12_STATIC_SAMPLER_DESC sampler = {};
  sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
  sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
  sampler.MipLODBias = 0;
  sampler.MaxAnisotropy = 0;
  sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
  sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
  sampler.MinLOD = 0.0f;
  sampler.MaxLOD = D3D12_FLOAT32_MAX;
  sampler.ShaderRegister = 0;
  sampler.RegisterSpace = 0;
  sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

  D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
  rootSigDesc.NumParameters = _countof(rootParams);
  rootSigDesc.pParameters = rootParams;
  rootSigDesc.NumStaticSamplers = 1;
  rootSigDesc.pStaticSamplers = &sampler;
  rootSigDesc.Flags =
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

  ComPtr<ID3DBlob> signature;
  ComPtr<ID3DBlob> error;
  if (FAILED(D3D12SerializeRootSignature(
          &rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error))) {
    if (error)
      printf("Root signature error: %s\n", (char *)error->GetBufferPointer());
    return false;
  }

  if (FAILED(device->CreateRootSignature(0, signature->GetBufferPointer(),
                                         signature->GetBufferSize(),
                                         IID_PPV_ARGS(&m_rootSignature))))
    return false;

  return true;
}

bool ImageRenderer::CreatePipelineState(ID3D12Device *device) {
  // Compile shaders
  ComPtr<ID3DBlob> vertexShader;
  ComPtr<ID3DBlob> pixelShader;
  ComPtr<ID3DBlob> error;

  UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

  if (FAILED(D3DCompile(g_VertexShader, strlen(g_VertexShader), nullptr,
                        nullptr, nullptr, "main", "vs_5_0", compileFlags, 0,
                        &vertexShader, &error))) {
    if (error)
      printf("VS compile error: %s\n", (char *)error->GetBufferPointer());
    return false;
  }

  if (FAILED(D3DCompile(g_PixelShader, strlen(g_PixelShader), nullptr, nullptr,
                        nullptr, "main", "ps_5_0", compileFlags, 0,
                        &pixelShader, &error))) {
    if (error)
      printf("PS compile error: %s\n", (char *)error->GetBufferPointer());
    return false;
  }

  // Create pipeline state
  D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
  psoDesc.pRootSignature = m_rootSignature.Get();
  psoDesc.VS = {vertexShader->GetBufferPointer(),
                vertexShader->GetBufferSize()};
  psoDesc.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};
  psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask =
      D3D12_COLOR_WRITE_ENABLE_ALL;
  psoDesc.SampleMask = UINT_MAX;
  psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
  psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  psoDesc.RasterizerState.DepthClipEnable = TRUE;
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  psoDesc.NumRenderTargets = 1;
  psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
  psoDesc.SampleDesc.Count = 1;

  if (FAILED(device->CreateGraphicsPipelineState(
          &psoDesc, IID_PPV_ARGS(&m_pipelineState))))
    return false;

  return true;
}

bool ImageRenderer::UploadImage(ID3D12Device *device,
                                ID3D12GraphicsCommandList *commandList,
                                const ImageData &imageData) {
  LOG("ImageRenderer::UploadImage - device=%p, commandList=%p", device,
      commandList);
  LOG("ImageRenderer::UploadImage - imageData: width=%d, height=%d, "
      "pixels.size=%zu",
      imageData.width, imageData.height, imageData.pixels.size());

  if (imageData.pixels.empty() || imageData.width == 0 ||
      imageData.height == 0) {
    LOG_ERROR("ImageRenderer::UploadImage - Invalid image data!");
    return false;
  }

  m_imageWidth = imageData.width;
  m_imageHeight = imageData.height;

  // Create texture
  D3D12_RESOURCE_DESC textureDesc = {};
  textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  textureDesc.Width = imageData.width;
  textureDesc.Height = imageData.height;
  textureDesc.DepthOrArraySize = 1;
  textureDesc.MipLevels = 1;
  textureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  textureDesc.SampleDesc.Count = 1;
  textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

  HRESULT hr = device->CreateCommittedResource(
      &heapProps, D3D12_HEAP_FLAG_NONE, &textureDesc,
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_texture));
  if (FAILED(hr)) {
    LOG_ERROR("ImageRenderer::UploadImage - CreateCommittedResource (texture) "
              "failed! hr=0x%08X",
              hr);
    return false;
  }
  LOG("ImageRenderer::UploadImage - Texture created: m_texture=%p",
      m_texture.Get());

  // Create upload buffer
  UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_texture.Get(), 0, 1);
  LOG("ImageRenderer::UploadImage - uploadBufferSize=%llu", uploadBufferSize);

  heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
  D3D12_RESOURCE_DESC uploadDesc = {};
  uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
  uploadDesc.Width = uploadBufferSize;
  uploadDesc.Height = 1;
  uploadDesc.DepthOrArraySize = 1;
  uploadDesc.MipLevels = 1;
  uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
  uploadDesc.SampleDesc.Count = 1;
  uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

  hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE,
                                       &uploadDesc,
                                       D3D12_RESOURCE_STATE_GENERIC_READ,
                                       nullptr, IID_PPV_ARGS(&m_uploadBuffer));
  if (FAILED(hr)) {
    LOG_ERROR("ImageRenderer::UploadImage - CreateCommittedResource (upload "
              "buffer) failed! hr=0x%08X",
              hr);
    return false;
  }
  LOG("ImageRenderer::UploadImage - Upload buffer created: m_uploadBuffer=%p",
      m_uploadBuffer.Get());

  // Upload texture data
  D3D12_SUBRESOURCE_DATA textureData = {};
  textureData.pData = imageData.pixels.data();
  textureData.RowPitch = imageData.width * 4 * sizeof(float);
  textureData.SlicePitch = textureData.RowPitch * imageData.height;

  LOG("ImageRenderer::UploadImage - Uploading texture data: RowPitch=%lld, "
      "SlicePitch=%lld",
      textureData.RowPitch, textureData.SlicePitch);

  UpdateSubresources(commandList, m_texture.Get(), m_uploadBuffer.Get(), 0, 0,
                     1, &textureData);

  // Transition to shader resource
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = m_texture.Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  commandList->ResourceBarrier(1, &barrier);
  LOG("ImageRenderer::UploadImage - Resource barrier recorded");

  // Create SRV
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Format = textureDesc.Format;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = 1;

  device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srvCpuHandle);
  LOG("ImageRenderer::UploadImage - SRV created at CPU handle %llu",
      m_srvCpuHandle.ptr);
  LOG("ImageRenderer::UploadImage - SUCCESS! HasTexture=%d",
      HasTexture() ? 1 : 0);

  return true;
}

static int s_renderCallCount = 0;

void ImageRenderer::Render(ID3D12GraphicsCommandList *commandList, float zoom,
                           const DirectX::XMFLOAT2 &pan, float rangeMin,
                           float rangeMax, bool showR, bool showG, bool showB,
                           int viewportX, int viewportY, int viewportWidth,
                           int viewportHeight, int screenWidth,
                           int screenHeight) {
  s_renderCallCount++;

  // Log only first few calls to avoid spam
  bool shouldLog = (s_renderCallCount <= 5);

  if (shouldLog) {
    LOG("ImageRenderer::Render[%d] - commandList=%p, m_texture=%p, "
        "m_pipelineState=%p, m_rootSignature=%p",
        s_renderCallCount, commandList, m_texture.Get(), m_pipelineState.Get(),
        m_rootSignature.Get());
    LOG("ImageRenderer::Render[%d] - viewport: x=%d, y=%d, w=%d, h=%d, screen: "
        "%dx%d",
        s_renderCallCount, viewportX, viewportY, viewportWidth, viewportHeight,
        screenWidth, screenHeight);
    LOG("ImageRenderer::Render[%d] - zoom=%.2f, pan=(%.1f, %.1f), range=[%.3f, "
        "%.3f]",
        s_renderCallCount, zoom, pan.x, pan.y, rangeMin, rangeMax);
    LOG("ImageRenderer::Render[%d] - m_srvGpuHandle.ptr=%llu, imageSize=%dx%d",
        s_renderCallCount, m_srvGpuHandle.ptr, m_imageWidth, m_imageHeight);
  }

  if (!m_texture) {
    if (shouldLog)
      LOG_ERROR("ImageRenderer::Render[%d] - m_texture is null, returning!",
                s_renderCallCount);
    return;
  }

  if (!m_pipelineState) {
    LOG_ERROR("ImageRenderer::Render[%d] - m_pipelineState is null!",
              s_renderCallCount);
    return;
  }

  if (!m_rootSignature) {
    LOG_ERROR("ImageRenderer::Render[%d] - m_rootSignature is null!",
              s_renderCallCount);
    return;
  }

  if (viewportWidth <= 0 || viewportHeight <= 0) {
    if (shouldLog)
      LOG_ERROR("ImageRenderer::Render[%d] - Invalid viewport size!",
                s_renderCallCount);
    return;
  }

  // Set viewport to full screen (NDC quad generation uses [-1,1] space)
  // Scissor rect clips the rendering to the target region
  D3D12_VIEWPORT viewport = {};
  viewport.TopLeftX = 0.0f;
  viewport.TopLeftY = 0.0f;
  viewport.Width = (float)screenWidth;
  viewport.Height = (float)screenHeight;
  viewport.MinDepth = 0.0f;
  viewport.MaxDepth = 1.0f;
  commandList->RSSetViewports(1, &viewport);

  // Scissor rect to the image view region
  D3D12_RECT scissor = {};
  scissor.left = viewportX;
  scissor.top = viewportY;
  scissor.right = viewportX + viewportWidth;
  scissor.bottom = viewportY + viewportHeight;
  commandList->RSSetScissorRects(1, &scissor);

  // Calculate transform matrix
  // We transform from NDC [-1,1] (quad size 2x2) to the desired viewport
  // region. The UI expects 'zoom' to means 'image pixel size / screen pixel
  // size'.
  //
  // Target size in pixels:
  // width_px = m_imageWidth * zoom
  // height_px = m_imageHeight * zoom
  //
  // Fraction of viewport:
  // width_frac = width_px / viewportWidth
  // height_frac = height_px / viewportHeight
  //
  // In NDC (size 2):
  // scaleX = width_frac
  // scaleY = height_frac

  // Note: The base quad is [-1, 1], so its width/height is 2.0.
  // Scaling by S makes it [-S, S], width/height 2*S.
  // We want 2*S = 2 * (dim * zoom / viewportDim).
  // So S = dim * zoom / viewportDim.

  float scaleX = (float)m_imageWidth * zoom / (float)viewportWidth;
  float scaleY = (float)m_imageHeight * zoom / (float)viewportHeight;

  // Position the viewport region in NDC space
  // viewportPos in window space [0, screenWidth/Height]
  // Convert to NDC: x_ndc = (x_window / screenWidth) * 2 - 1
  float viewportCenterX =
      (viewportX + viewportWidth * 0.5f) / screenWidth * 2.0f - 1.0f;
  float viewportCenterY =
      (viewportY + viewportHeight * 0.5f) / screenHeight * 2.0f - 1.0f;

  // Convert pan from pixels to NDC (relative to viewport)
  float panX = (pan.x / viewportWidth) * 2.0f;
  float panY = (pan.y / viewportHeight) * 2.0f;

  // Apply Scaling first, then Translation: S * T
  // This ensures that the translation (pan) is applied in Screen Space (NDC),
  // unaffected by the zoom level.
  DirectX::XMMATRIX transform = DirectX::XMMatrixMultiply(
      DirectX::XMMatrixScaling(scaleX, scaleY, 1.0f),
      DirectX::XMMatrixTranslation(viewportCenterX + panX,
                                   -(viewportCenterY + panY), 0.0f));

  if (shouldLog) {
    LOG("ImageRenderer::Render[%d] - scaleX=%.3f, scaleY=%.3f, panX=%.3f, "
        "panY=%.3f",
        s_renderCallCount, scaleX, scaleY, panX, panY);
  }

  // Set pipeline state
  commandList->SetPipelineState(m_pipelineState.Get());
  commandList->SetGraphicsRootSignature(m_rootSignature.Get());

  // Set constants
  struct Constants {
    DirectX::XMFLOAT4X4 transform;
    float rangeMin;
    float rangeMax;
    float padding[2];
    float channelMask[4];
  } constants;

  DirectX::XMStoreFloat4x4(&constants.transform,
                           DirectX::XMMatrixTranspose(transform));
  constants.rangeMin = rangeMin;
  constants.rangeMax = rangeMax;
  constants.padding[0] = 0.0f;
  constants.padding[1] = 0.0f;
  constants.channelMask[0] = showR ? 1.0f : 0.0f;
  constants.channelMask[1] = showG ? 1.0f : 0.0f;
  constants.channelMask[2] = showB ? 1.0f : 0.0f;
  constants.channelMask[3] = 1.0f; // Alpha always 1 for mask? Unused.

  commandList->SetGraphicsRoot32BitConstants(0, sizeof(Constants) / 4,
                                             &constants, 0);

  // Set texture
  commandList->SetGraphicsRootDescriptorTable(1, m_srvGpuHandle);

  // Draw quad (6 vertices)
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->DrawInstanced(6, 1, 0, 0);

  if (shouldLog) {
    LOG("ImageRenderer::Render[%d] - DrawInstanced called successfully",
        s_renderCallCount);
  }

  // Restore full screen viewport and scissor
  D3D12_VIEWPORT fullViewport = {};
  fullViewport.Width = (float)screenWidth;
  fullViewport.Height = (float)screenHeight;
  fullViewport.MaxDepth = 1.0f;
  commandList->RSSetViewports(1, &fullViewport);

  D3D12_RECT fullScissor = {};
  fullScissor.right = screenWidth;
  fullScissor.bottom = screenHeight;
  commandList->RSSetScissorRects(1, &fullScissor);
}

void ImageRenderer::Cleanup() {
  m_texture.Reset();
  m_uploadBuffer.Reset();
  m_pipelineState.Reset();
  m_rootSignature.Reset();
  m_renderTexture.Reset();
  m_rtvHeap.Reset();
}

void ImageRenderer::ClearTexture() {
  m_texture.Reset();
  m_imageWidth = 0;
  m_imageHeight = 0;
}

bool ImageRenderer::ResizeRenderTarget(ID3D12Device *device, int width,
                                       int height) {
  if (m_renderTargetWidth == width && m_renderTargetHeight == height &&
      m_renderTexture)
    return true;

  if (width <= 0 || height <= 0)
    return false;

  m_renderTargetWidth = width;
  m_renderTargetHeight = height;

  LOG("ImageRenderer::ResizeRenderTarget - Resizing to %dx%d", width, height);

  // Create RTV heap if not exists
  if (!m_rtvHeap) {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = 1;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(device->CreateDescriptorHeap(&rtvHeapDesc,
                                            IID_PPV_ARGS(&m_rtvHeap))))
      return false;

    m_rtvCpuHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
  }

  // Create render texture
  D3D12_RESOURCE_DESC textureDesc = {};
  textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
  textureDesc.Width = width;
  textureDesc.Height = height;
  textureDesc.DepthOrArraySize = 1;
  textureDesc.MipLevels = 1;
  textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  textureDesc.SampleDesc.Count = 1;
  textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
  textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

  // Clear value
  D3D12_CLEAR_VALUE clearValue = {};
  clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  clearValue.Color[0] = 0.0f;
  clearValue.Color[1] = 0.0f;
  clearValue.Color[2] = 0.0f;
  clearValue.Color[3] = 1.0f;

  D3D12_HEAP_PROPERTIES heapProps = {};
  heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

  if (FAILED(device->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_NONE, &textureDesc,
          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue,
          IID_PPV_ARGS(&m_renderTexture)))) {
    return false;
  }

  // Create RTV
  device->CreateRenderTargetView(m_renderTexture.Get(), nullptr,
                                 m_rtvCpuHandle);

  // Create SRV in the main SRV heap (at the allocated slot)
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = 1;

  device->CreateShaderResourceView(m_renderTexture.Get(), &srvDesc,
                                   m_outputSrvCpuHandle);

  return true;
}

void ImageRenderer::RenderToTexture(ID3D12GraphicsCommandList *commandList,
                                    float zoom, const DirectX::XMFLOAT2 &pan,
                                    float rangeMin, float rangeMax, bool showR,
                                    bool showG, bool showB) {
  if (!m_texture || !m_renderTexture || !m_pipelineState || !m_rootSignature)
    return;

  // Transition Render Target to RENDER_TARGET state
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Transition.pResource = m_renderTexture.Get();
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  commandList->ResourceBarrier(1, &barrier);

  // Set Render Target
  commandList->OMSetRenderTargets(1, &m_rtvCpuHandle, FALSE, nullptr);

  // Clear Render Target
  const float clearColor[] = {0.1f, 0.1f, 0.1f, 1.0f}; // Dark gray background
  commandList->ClearRenderTargetView(m_rtvCpuHandle, clearColor, 0, nullptr);

  // Set Viewport and Scissor (Full Texture)
  D3D12_VIEWPORT viewport = {};
  viewport.Width = (float)m_renderTargetWidth;
  viewport.Height = (float)m_renderTargetHeight;
  viewport.MaxDepth = 1.0f;
  commandList->RSSetViewports(1, &viewport);

  D3D12_RECT scissor = {};
  scissor.right = m_renderTargetWidth;
  scissor.bottom = m_renderTargetHeight;
  commandList->RSSetScissorRects(1, &scissor);

  // Calculate Transform
  // We want to transform the image into this render target
  // The previous logic calculated transform relative to screen/viewport.
  // Here, our "viewport" is the texture itself.
  // The UI expects 'zoom' to be a multiplier on the original image size.
  // So if zoom=1, the image is drawn 1:1 size.

  float scaleX = (float)m_imageWidth * zoom / (float)m_renderTargetWidth;
  float scaleY = (float)m_imageHeight * zoom / (float)m_renderTargetHeight;

  // Center in the render target texture
  // NDC is [-1, 1], so center is (0,0).
  // Pan is relative to screen pixels in the original logical view...
  // But since we are rendering TO A TEXTURE that will be displayed in ImGui,
  // we should treat the texture dimensions as the viewport dimensions.

  // Normalized pan (relative to viewport size 2.0 in NDC)
  float panX = (pan.x / m_renderTargetWidth) * 2.0f;
  float panY = (pan.y / m_renderTargetHeight) * 2.0f;

  DirectX::XMMATRIX transform = DirectX::XMMatrixMultiply(
      DirectX::XMMatrixScaling(scaleX, scaleY, 1.0f),
      DirectX::XMMatrixTranslation(panX, -panY, 0.0f));

  // Setup Pipeline
  commandList->SetPipelineState(m_pipelineState.Get());
  commandList->SetGraphicsRootSignature(m_rootSignature.Get());

  // Constants
  struct Constants {
    DirectX::XMFLOAT4X4 transform;
    float rangeMin;
    float rangeMax;
    float padding[2];
    float channelMask[4];
  } constants;

  DirectX::XMStoreFloat4x4(&constants.transform,
                           DirectX::XMMatrixTranspose(transform));
  constants.rangeMin = rangeMin;
  constants.rangeMax = rangeMax;
  constants.padding[0] = 0.0f;
  constants.padding[1] = 0.0f;
  constants.channelMask[0] = showR ? 1.0f : 0.0f;
  constants.channelMask[1] = showG ? 1.0f : 0.0f;
  constants.channelMask[2] = showB ? 1.0f : 0.0f;
  constants.channelMask[3] = 1.0f;

  commandList->SetGraphicsRoot32BitConstants(0, sizeof(Constants) / 4,
                                             &constants, 0);

  // Texture (Input)
  commandList->SetGraphicsRootDescriptorTable(1, m_srvGpuHandle);

  // Draw
  commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  commandList->DrawInstanced(6, 1, 0, 0);

  // Transition Render Target back to PIXEL_SHADER_RESOURCE
  barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
  barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  commandList->ResourceBarrier(1, &barrier);
}
