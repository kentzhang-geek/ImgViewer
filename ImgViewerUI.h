#include "DX12Renderer.h"
#include "ImageRenderer.h"
#include "ImgViewer.h"
#include "imgui.h"

/**
 * @brief Manages the User Interface and interaction logic.
 */
class ImgViewerUI {
public:
  /**
   * @brief Constructor.
   */
  ImgViewerUI();

  /**
   * @brief Destructor.
   */
  ~ImgViewerUI();

  /**
   * @brief Initializes the UI and core components.
   * @param renderer Pointer to the DX12Renderer instance.
   */
  void Initialize(DX12Renderer *renderer);

  /**
   * @brief Renders the ImGui user interface.
   * \note This should be called within an active ImGui frame.
   */
  void Render();

  /**
   * @brief Renders the current image into an intermediate texture for display.
   * @param commandList The graphics command list to record rendering commands
   * into.
   */
  void RenderImageToTexture(ID3D12GraphicsCommandList *commandList);

  /**
   * @deprecated Old immediate render method. Use RenderImageToTexture instead.
   */
  void RenderImage(ID3D12GraphicsCommandList *commandList, int screenWidth,
                   int screenHeight);

  /**
   * @brief Handles file drag and drop events.
   * @param filepath Path to the dropped file.
   */
  void HandleDragDrop(const std::string &filepath);

  /**
   * @brief Accessor for the main ImgViewer instance.
   */
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

  // Config & Layout
  bool m_showConfigPanel = false;
  float m_crosslineColor[4] = {1.0f, 1.0f, 0.0f,
                               0.5f}; // Default Yellow, semi-transparent
  bool m_layoutInitialized = false;

  void RenderConfigPanel();
  void ApplyDefaultLayout(ImGuiID dockspaceId);
};
