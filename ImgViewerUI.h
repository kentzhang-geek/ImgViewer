#pragma once
#include "DX12Renderer.h"
#include "ImageRenderer.h"
#include "ImgViewer.h"

class ImgViewerUI {
public:
  ImgViewerUI();
  ~ImgViewerUI();

  void Initialize(DX12Renderer *renderer);
  void Render();

  // New method for Render-to-Texture
  void RenderImageToTexture(ID3D12GraphicsCommandList *commandList);

  // Old method - can be removed or deprecated
  void RenderImage(ID3D12GraphicsCommandList *commandList, int screenWidth,
                   int screenHeight);
  void HandleDragDrop(const std::string &filepath);

  ImgViewer &GetImgViewer() { return m_imgViewer; }

private:
  ImgViewer m_imgViewer;
  DX12Renderer *m_renderer;
  ImageRenderer m_imageRenderer;

  // UI state
  DirectX::XMFLOAT2 m_lastMousePos = {0, 0};
  DirectX::XMFLOAT2 m_hoveredPixel = {-1, -1};
  bool m_isPanning = false;
  bool m_showMagnifier = false;
  bool m_showR = true;
  bool m_showG = true;
  bool m_showB = true;
  DirectX::XMFLOAT2 m_magnifierPos = {0, 0};
  float m_sidePanelWidth = 300.0f;

  // Image view rendering info (saved during Render(), used by RenderImage())
  bool m_needsImageRender = false;
  int m_imageViewX = 0;
  int m_imageViewY = 0;
  int m_imageViewWidth = 0;
  int m_imageViewHeight = 0;

  float m_titleBarInteractWidth = 400.0f; // Default safety value

public:
  float GetTitleBarInteractWidth() const { return m_titleBarInteractWidth; }

private:
  // Histogram data
  std::vector<int> m_histogramR;
  std::vector<int> m_histogramG;
  std::vector<int> m_histogramB;
  int m_histogramBins = 256;

  void RenderMainPanel();
  void RenderInfoPanel();
  void RenderImageView();
  void RenderHistogram();
  void RenderRangeControls();
  void RenderMagnifier();

  void UpdateHistogram();
  void HandleImageInteraction();

  // Modern UI
  void RenderTitleBar();
  void SetupImGuiStyle();

  // Helpers
  void OpenFileDialog();
  void PasteFromClipboard();
  void HandleGlobalShortcuts();
};
