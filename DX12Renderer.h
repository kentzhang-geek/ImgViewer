#pragma once
#include "pch.h"
#include <dxgi1_4.h>

using namespace Microsoft::WRL;

/**
 * @brief Core DirectX 12 Renderer class managing the device, swap chain, and
 * command queues.
 */
class DX12Renderer {
public:
  /**
   * @brief Constructor.
   */
  DX12Renderer();

  /**
   * @brief Destructor.
   */
  ~DX12Renderer();

  /**
   * @brief Initializes DirectX 12 resources and the swap chain.
   * @param hwnd Handle to the window.
   * @param width Initial window width.
   * @param height Initial window height.
   * @return True if initialization successfully.
   */
  bool Initialize(HWND hwnd, UINT width, UINT height);

  /**
   * @brief Cleans up D3D12 resources.
   */
  void Cleanup();

  /**
   * @brief Begins a new rendering frame (resets command list, etc.).
   */
  void BeginRender();

  /**
   * @brief Ends the current frame (executes command list, presents).
   */
  void EndRender();

  /**
   * @brief Handles window resize events.
   */
  void OnResize(UINT width, UINT height);

  // ImGui integration getters
  ID3D12Device *GetDevice() const { return m_device.Get(); }
  ID3D12CommandQueue *GetCommandQueue() const { return m_commandQueue.Get(); }
  ID3D12DescriptorHeap *GetSrvHeap() const { return m_srvHeap.Get(); }
  ID3D12GraphicsCommandList *GetCommandList() const {
    return m_commandList.Get();
  }
  D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const {
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle =
        m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += m_frameIndex * m_rtvDescriptorSize;
    return rtvHandle;
  }
  UINT GetWidth() const { return m_width; }
  UINT GetHeight() const { return m_height; }

  /**
   * @brief Waits for the GPU to finish all pending work.
   */
  void WaitForGpu();

private:
  static const UINT FrameCount = 2;

  // Pipeline objects
  ComPtr<ID3D12Device> m_device;
  ComPtr<IDXGISwapChain3> m_swapChain;
  ComPtr<ID3D12CommandQueue> m_commandQueue;
  ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
  ComPtr<ID3D12DescriptorHeap> m_srvHeap;
  ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
  ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
  ComPtr<ID3D12GraphicsCommandList> m_commandList;

  // Synchronization objects
  UINT m_frameIndex;
  HANDLE m_fenceEvent;
  ComPtr<ID3D12Fence> m_fence;
  UINT64 m_fenceValues[FrameCount];

  // Viewport and scissor
  D3D12_VIEWPORT m_viewport;
  D3D12_RECT m_scissorRect;
  UINT m_rtvDescriptorSize;

  HWND m_hwnd;
  UINT m_width;
  UINT m_height;

private:
  void MoveToNextFrame();
  void PopulateCommandList();
  void CreateRenderTargetViews();
};
