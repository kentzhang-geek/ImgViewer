#pragma once
#include "pch.h"
#include <dxgi1_4.h>

using namespace Microsoft::WRL;

class DX12Renderer {
public:
  DX12Renderer();
  ~DX12Renderer();

  bool Initialize(HWND hwnd, UINT width, UINT height);
  void Cleanup();
  void BeginRender();
  void EndRender();
  void OnResize(UINT width, UINT height);

  // ImGui integration
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
