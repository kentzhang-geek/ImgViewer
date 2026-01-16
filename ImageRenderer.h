#pragma once
#include "ImgViewer.h"
#include "pch.h"

using namespace Microsoft::WRL;

/**
 * @brief Handles DirectX 12 rendering of images.
 */
class ImageRenderer {
public:
  /**
   * @brief Constructor.
   */
  ImageRenderer();

  /**
   * @brief Destructor.
   */
  ~ImageRenderer();

  /**
   * @brief Initializes the renderer resources.
   * @param device Pointer to the D3D12 Device.
   * @param srvHeap Pointer to the SRV Descriptor Heap.
   * @param srvDescriptorSize Size of the SRV descriptor increment.
   * @return True if initialization succeeded.
   */
  bool Initialize(ID3D12Device *device, ID3D12DescriptorHeap *srvHeap,
                  UINT srvDescriptorSize);

  /**
   * @brief Cleans up resources.
   */
  void Cleanup();

  /**
   * @brief Clears the current texture and resets state.
   */
  void ClearTexture();

  /**
   * @brief Uploads image data to the GPU and creates a texture resource.
   * @param device D3D12 Device.
   * @param commandList Graphics command list to record upload commands.
   * @param imageData Data to upload.
   * @return True if successful.
   */
  bool UploadImage(ID3D12Device *device, ID3D12GraphicsCommandList *commandList,
                   const ImageData &imageData);

  /**
   * @brief Renders the image quad to the screen (immediate mode).
   * @note Used by the old rendering path.
   */
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
  /**
   * @brief Resizes the intermediate render target texture.
   * @param width New width.
   * @param height New height.
   * @return True if successful.
   */
  bool ResizeRenderTarget(ID3D12Device *device, int width, int height);

  /**
   * @brief Renders the image into the intermediate render target.
   */
  void RenderToTexture(ID3D12GraphicsCommandList *commandList, float zoom,
                       const DirectX::XMFLOAT2 &pan, float rangeMin,
                       float rangeMax, bool showR, bool showG, bool showB);

  /**
   * @brief Gets the SRV GPU handle for the rendered intermediate texture.
   * @return GPU handle for use with ImGui::Image.
   */
  D3D12_GPU_DESCRIPTOR_HANDLE GetOutputSrvGpuHandle() const {
    return m_outputSrvGpuHandle;
  }
  int GetRenderTargetWidth() const { return m_renderTargetWidth; }
  int GetRenderTargetHeight() const { return m_renderTargetHeight; }
};
