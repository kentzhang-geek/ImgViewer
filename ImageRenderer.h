#pragma once
#include "ImageViewer.h"
#include "pch.h"


using namespace Microsoft::WRL;

class ImageRenderer {
public:
  ImageRenderer();
  ~ImageRenderer();

  bool Initialize(ID3D12Device *device, ID3D12DescriptorHeap *srvHeap,
                  UINT srvDescriptorSize);
  void Cleanup();
  void ClearTexture();

  // Upload image data to GPU and create texture
  bool UploadImage(ID3D12Device *device, ID3D12GraphicsCommandList *commandList,
                   const ImageData &imageData);

  // Render the image quad to a specific viewport region
  void Render(ID3D12GraphicsCommandList *commandList, float zoom,
              const DirectX::XMFLOAT2 &pan, float rangeMin, float rangeMax,
              bool showR, bool showG, bool showB, int viewportX, int viewportY,
              int viewportWidth, int viewportHeight, int screenWidth,
              int screenHeight);

  bool HasTexture() const { return m_texture != nullptr; }
  D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle() const { return m_srvGpuHandle; }
  int GetImageWidth() const { return m_imageWidth; }
  int GetImageHeight() const { return m_imageHeight; }

private:
  ComPtr<ID3D12Resource> m_texture;
  ComPtr<ID3D12Resource> m_uploadBuffer;
  ComPtr<ID3D12RootSignature> m_rootSignature;
  ComPtr<ID3D12PipelineState> m_pipelineState;

  D3D12_GPU_DESCRIPTOR_HANDLE m_srvGpuHandle;
  D3D12_CPU_DESCRIPTOR_HANDLE m_srvCpuHandle;

  int m_imageWidth = 0;
  int m_imageHeight = 0;

  bool CreatePipelineState(ID3D12Device *device);
  bool CreateRootSignature(ID3D12Device *device);

  // Render Target support
  ComPtr<ID3D12Resource> m_renderTexture;
  ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE m_rtvCpuHandle;
  D3D12_GPU_DESCRIPTOR_HANDLE m_outputSrvGpuHandle;
  D3D12_CPU_DESCRIPTOR_HANDLE m_outputSrvCpuHandle;

  int m_renderTargetWidth = 0;
  int m_renderTargetHeight = 0;

public:
  // Resize the intermediate render target
  bool ResizeRenderTarget(ID3D12Device *device, int width, int height);

  // Render the image to the intermediate texture
  void RenderToTexture(ID3D12GraphicsCommandList *commandList, float zoom,
                       const DirectX::XMFLOAT2 &pan, float rangeMin,
                       float rangeMax, bool showR, bool showG, bool showB);

  // Get the SRV for the rendered texture (to pass to ImGui)
  D3D12_GPU_DESCRIPTOR_HANDLE GetOutputSrvGpuHandle() const {
    return m_outputSrvGpuHandle;
  }
  int GetRenderTargetWidth() const { return m_renderTargetWidth; }
  int GetRenderTargetHeight() const { return m_renderTargetHeight; }
};
