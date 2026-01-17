#include "ImgViewerUI.h"
#include "Logger.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "pch.h"
#include <algorithm>
#include <commdlg.h>

ImgViewerUI::ImgViewerUI() : m_renderer(nullptr) {
  m_histogramR.resize(m_histogramBins, 0);
  m_histogramG.resize(m_histogramBins, 0);
  m_histogramB.resize(m_histogramBins, 0);
}

ImgViewerUI::~ImgViewerUI() {}

void ImgViewerUI::Initialize(DX12Renderer *renderer) {
  LOG("ImgViewerUI::Initialize - renderer=%p", renderer);
  m_renderer = renderer;

  // Initialize image renderer
  UINT srvDescSize = renderer->GetDevice()->GetDescriptorHandleIncrementSize(
      D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
  LOG("ImgViewerUI::Initialize - SRV descriptor size=%u", srvDescSize);

  if (!m_imageRenderer.Initialize(renderer->GetDevice(), renderer->GetSrvHeap(),
                                  srvDescSize)) {
    LOG_ERROR("ImgViewerUI::Initialize - Failed to initialize ImageRenderer!");
  } else {
    LOG("ImgViewerUI::Initialize - ImageRenderer initialized successfully");
  }

  SetupImGuiStyle();
}

void ImgViewerUI::Render() {
  ImGuiIO &io = ImGui::GetIO();

  // Render Custom Title Bar (includes Menu Bar)
  RenderTitleBar();

  // Handle Global Shortcuts
  HandleGlobalShortcuts();

  // Determine Title Bar Height for DockSpace offset
  // The Title Bar is a fixed height, usually standard frame padding + font size
  // or we can just measure it? But we need to set the position explicitly for
  // the DockSpace. Let's assume a fixed height of 32px for now (matches
  // main.cpp definition).
  float titleBarHeight = 32.0f;

  // Create dockspace over the entire window area (below menu bar)
  ImGuiWindowFlags dockspaceFlags =
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
      ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

  ImGuiViewport *viewport = ImGui::GetMainViewport();

  ImGui::SetNextWindowPos(
      ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + titleBarHeight));
  ImGui::SetNextWindowSize(
      ImVec2(viewport->WorkSize.x, viewport->WorkSize.y - titleBarHeight));
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  ImGui::Begin("DockSpaceWindow", nullptr, dockspaceFlags);
  ImGui::PopStyleVar(3);

  ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");

  // Handle Layout Reset Request
  if (m_resetLayout) {
    ImGui::DockBuilderRemoveNode(dockspaceId); // Clear existing layout
    ApplyDefaultLayout(dockspaceId);           // Re-apply default
    m_resetLayout = false;
    m_layoutInitialized = true;
  }

  // Initialize Default Layout if first run (node doesn't exist)
  if (!m_layoutInitialized) {
    if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
      ApplyDefaultLayout(dockspaceId);
    }
    m_layoutInitialized = true;
  }

  ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

  ImGui::End();

  // Render Config Panel (independent window)
  RenderConfigPanel();

  // Image View Window (dockable)
  ImGui::Begin("Image View");
  RenderImageView();
  HandleImageInteraction();
  ImGui::End();

  // Info Panel Window (dockable)
  ImGui::Begin("Info");
  RenderInfoPanel();
  ImGui::End();

  // Plot Window (dockable)
  ImGui::Begin("Plot");
  RenderRangeControls();
  RenderHistogram();
  ImGui::End();

  // Modern Window Outline
  // Draw a 1px border around the entire viewport to give it definition.
  // We use the ForegroundDrawList to draw on top of everything.
  ImGui::GetForegroundDrawList()->AddRect(
      viewport->Pos,
      ImVec2(viewport->Pos.x + viewport->Size.x,
             viewport->Pos.y + viewport->Size.y),
      IM_COL32(60, 60, 60, 255), // Subtle grey border
      6.0f,                      // Match DWM rounding (approx 6-8px, using 6)
      0,
      1.0f); // 1px thickness
}

void ImgViewerUI::RenderMainPanel() {
  const auto &imgData = m_imgViewer.GetImageData();

  if (!m_imgViewer.HasImage()) {
    ImGui::Text("Drag and drop an image file here");
    ImGui::Text("or use File > Paste from Clipboard");
    return;
  }

  // Image view area
  ImVec2 availSize = ImGui::GetContentRegionAvail();
  availSize.y -= 200.0f; // Reserve space for histogram

  ImGui::BeginChild("ImageView", availSize, true, ImGuiWindowFlags_NoScrollbar);
  RenderImageView();
  ImGui::EndChild();

  // Histogram area
  ImGui::BeginChild("Histogram", ImVec2(0, 0), true);
  RenderRangeControls();
  RenderHistogram();
  ImGui::EndChild();
}

void ImgViewerUI::RenderInfoPanel() {
  const auto &imgData = m_imgViewer.GetImageData();

  ImGui::Text("Image Information");
  ImGui::Separator();

  if (!m_imgViewer.HasImage()) {
    ImGui::Text("No image loaded");
    return;
  }

  ImGui::Text("Filename: %s", imgData.filename.c_str());
  ImGui::Text("Dimensions: %d x %d", imgData.width, imgData.height);
  ImGui::Text("Format: %s", imgData.format.c_str());
  ImGui::Text("Pixel Format: %s", imgData.pixelFormat.c_str());
  ImGui::Text("Channels: %d", imgData.channels);

  ImGui::Separator();
  ImGui::Text("Value Range:");
  ImGui::Text("  Min: %.4f", imgData.minValue);
  ImGui::Text("  Max: %.4f", imgData.maxValue);
  if (imgData.hasNaN) {
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "  Contains NaN values");
  }

  ImGui::Separator();
  ImGui::Text("View Controls:");
  float zoom = m_imgViewer.GetZoom();
  if (ImGui::SliderFloat("Zoom", &zoom, 0.1f, 10.0f)) {
    m_imgViewer.SetZoom(zoom);
  }

  if (ImGui::Button("Reset View")) {
    m_imgViewer.SetZoom(1.0f);
    m_imgViewer.SetPan({0.0f, 0.0f});
  }

  // Pixel info on hover
  if (m_hoveredPixel.x >= 0 && m_hoveredPixel.x < imgData.width &&
      m_hoveredPixel.y >= 0 && m_hoveredPixel.y < imgData.height) {
    ImGui::Separator();
    ImGui::Text("Pixel at (%d, %d):", (int)m_hoveredPixel.x,
                (int)m_hoveredPixel.y);

    int pixelIdx =
        ((int)m_hoveredPixel.y * imgData.width + (int)m_hoveredPixel.x) * 4;
    float r = imgData.pixels[pixelIdx + 0];
    float g = imgData.pixels[pixelIdx + 1];
    float b = imgData.pixels[pixelIdx + 2];
    float a = imgData.pixels[pixelIdx + 3];

    ImGui::Text("  R: %.4f", r);
    ImGui::Text("  G: %.4f", g);
    ImGui::Text("  B: %.4f", b);
    ImGui::Text("  A: %.4f", a);

    ImVec4 color(r, g, b, a);
    ImGui::ColorButton("Pixel Color", color,
                       ImGuiColorEditFlags_NoTooltip |
                           ImGuiColorEditFlags_NoBorder,
                       ImVec2(50, 50));
  }

  // Magnifier view (bottom right)
  if (m_showMagnifier) {
    ImGui::Separator();
    ImGui::Text("Magnified View");
    RenderMagnifier();
  }
}

// New method: Renders the image content into the intermediate texture
void ImgViewerUI::RenderImageToTexture(ID3D12GraphicsCommandList *commandList) {
  if (!m_imageRenderer.HasTexture() || !m_renderer)
    return;

  // We only render if we have a valid target size (calculated in UI pass)
  if (m_imageViewWidth <= 0 || m_imageViewHeight <= 0)
    return;

  // Check if we need to resize
  // We can do this here safely because we are recording commands
  // But we need the device to create resources
  if (m_imageRenderer.GetRenderTargetWidth() != m_imageViewWidth ||
      m_imageRenderer.GetRenderTargetHeight() != m_imageViewHeight) {
    // This is a bit of a hack to create resources inside render loop,
    // but since it's just resizing a texture, it should be fine.
    // Ideally we'd do this at start of frame, but we only know size after UI
    // layout (which happened prev frame? or just now?) Actually UI Render
    // happened BEFORE this in main loop? No. In main.cpp we will call
    // RenderToTexture BEFORE ImGui Render. So m_imageViewWidth is from LAST
    // frame. That is acceptable (1 frame lag on resize is invisible).
    // Wait for GPU to finish using the old resource before we destroy it!
    m_renderer->WaitForGpu();
    m_imageRenderer.ResizeRenderTarget(m_renderer->GetDevice(),
                                       m_imageViewWidth, m_imageViewHeight);
  }

  float zoom = m_imgViewer.GetZoom();
  auto pan = m_imgViewer.GetPan();
  float rangeMin = m_imgViewer.GetRangeMin();
  float rangeMax = m_imgViewer.GetRangeMax();

  m_imageRenderer.RenderToTexture(commandList, zoom, pan, rangeMin, rangeMax,
                                  m_showR, m_showG, m_showB);
}

void ImgViewerUI::RenderImageView() {
  ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImVec2 canvasSize = ImGui::GetContentRegionAvail();

  if (canvasSize.x <= 0 || canvasSize.y <= 0)
    return;

  // Save dimensions for the Render step (frame N+1 or later in frame N)
  m_imageViewX = (int)canvasPos.x;
  m_imageViewY = (int)canvasPos.y;
  m_imageViewWidth = (int)canvasSize.x;
  m_imageViewHeight = (int)canvasSize.y;

  if (!m_imgViewer.HasImage()) {
    // Draw background
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(30, 30, 30, 255));

    // Center the text
    const char *text = "Drag and drop an image file here\nor use File > Open";
    ImVec2 textSize = ImGui::CalcTextSize(text);
    ImVec2 textPos(canvasPos.x + (canvasSize.x - textSize.x) * 0.5f,
                   canvasPos.y + (canvasSize.y - textSize.y) * 0.5f);
    drawList->AddText(textPos, IM_COL32(128, 128, 128, 255), text);

    ImGui::Dummy(canvasSize); // Consume space
    return;
  }

  // Ensure Render Target exists (at least with some size) so we have a
  // descriptor
  if (m_imageRenderer.GetRenderTargetWidth() == 0) {
    m_imageRenderer.ResizeRenderTarget(m_renderer->GetDevice(),
                                       (int)canvasSize.x, (int)canvasSize.y);
  }

  // Display the Rendered Texture as an Image
  // Using the GPU handle for the SRV
  ImTextureID my_tex_id =
      (ImTextureID)m_imageRenderer.GetOutputSrvGpuHandle().ptr;

  // We want to draw the image covering the available space
  // The "content" of the texture is rendered by RenderImageToTexture
  ImGui::Image(my_tex_id, canvasSize);

  // Handle interaction
  // We use an InvisibleButton over the image to catch inputs if Image() didn't
  ImGui::SetCursorScreenPos(canvasPos);
  ImGui::InvisibleButton("ImageCanvas", canvasSize,
                         ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonMiddle |
                             ImGuiButtonFlags_MouseButtonRight);

  HandleImageInteraction();

  // Draw crosshair overlay using ImGui (ON TOP of the image)
  // Since we just drew the image using ImGui::Image, any subsequent draw calls
  // (like AddLine) will be on top of it in the draw list.
  const auto &imgData = m_imgViewer.GetImageData();
  if (m_hoveredPixel.x >= 0) {
    float zoom = m_imgViewer.GetZoom();
    auto pan = m_imgViewer.GetPan();

    int imgWidth = m_imageRenderer.GetImageWidth();
    int imgHeight = m_imageRenderer.GetImageHeight();

    // Calculations for crosshair position (screen space)
    // Similar to RenderToTexture logic, but projecting to Screen Space

    float displayWidth = imgWidth * zoom;
    float displayHeight = imgHeight * zoom;

    float offsetX = (m_imageViewWidth - displayWidth) * 0.5f + pan.x;
    float offsetY = (m_imageViewHeight - displayHeight) * 0.5f + pan.y;

    // Calculate pixel position in screen coordinates
    float pixelScreenX = m_imageViewX + offsetX + m_hoveredPixel.x * zoom;
    float pixelScreenY = m_imageViewY + offsetY + m_hoveredPixel.y * zoom;

    // Clamp to view area? No, crosshair can extend

    ImDrawList *drawList = ImGui::GetWindowDrawList();

    // Customizable Crossline Color
    ImU32 crosshairColor =
        ImGui::GetColorU32(ImVec4(m_crosslineColor[0], m_crosslineColor[1],
                                  m_crosslineColor[2], m_crosslineColor[3]));
    ImU32 boxColor = ImGui::GetColorU32(ImVec4(
        m_crosslineColor[0], m_crosslineColor[1], m_crosslineColor[2], 1.0f));

    // Pixel top-left in screen coordinates
    // We use floor to snap strictly to grid visual
    int ix = (int)m_hoveredPixel.x;
    int iy = (int)m_hoveredPixel.y;

    float px1 = m_imageViewX + offsetX + ix * zoom;
    float py1 = m_imageViewY + offsetY + iy * zoom;
    float px2 = px1 + zoom;
    float py2 = py1 + zoom;

    // Horizontal line (through center of pixel)
    float centerY = (py1 + py2) * 0.5f;
    float x1 = (float)m_imageViewX;
    float x2 = (float)(m_imageViewX + m_imageViewWidth);
    drawList->AddLine(ImVec2(x1, centerY), ImVec2(x2, centerY), crosshairColor,
                      1.0f);

    // Vertical line (through center of pixel)
    float centerX = (px1 + px2) * 0.5f;
    float y1 = (float)m_imageViewY;
    float y2 = (float)(m_imageViewY + m_imageViewHeight);
    drawList->AddLine(ImVec2(centerX, y1), ImVec2(centerX, y2), crosshairColor,
                      1.0f);

    // Bounding box around the pixel
    // Expand slightly out so it surrounds the pixel? Or exact?
    // Exact is better for precision.
    drawList->AddRect(ImVec2(px1, py1), ImVec2(px2, py2), boxColor, 0.0f, 0,
                      1.0f);
  }
}

static int s_renderImageCallCount = 0;

void ImgViewerUI::RenderImage(ID3D12GraphicsCommandList *commandList,
                              int screenWidth, int screenHeight) {
  s_renderImageCallCount++;
  bool shouldLog = (s_renderImageCallCount <= 5);

  if (shouldLog) {
    LOG("ImgViewerUI::RenderImage[%d] - m_needsImageRender=%d, HasTexture=%d",
        s_renderImageCallCount, m_needsImageRender ? 1 : 0,
        m_imageRenderer.HasTexture() ? 1 : 0);
  }

  if (!m_needsImageRender || !m_imageRenderer.HasTexture()) {
    if (shouldLog) {
      LOG("ImgViewerUI::RenderImage[%d] - Skipping: m_needsImageRender=%d, "
          "HasTexture=%d",
          s_renderImageCallCount, m_needsImageRender ? 1 : 0,
          m_imageRenderer.HasTexture() ? 1 : 0);
    }
    return;
  }

  float zoom = m_imgViewer.GetZoom();
  auto pan = m_imgViewer.GetPan();
  float rangeMin = m_imgViewer.GetRangeMin();
  float rangeMax = m_imgViewer.GetRangeMax();

  if (shouldLog) {
    LOG("ImgViewerUI::RenderImage[%d] - Calling ImageRenderer::Render with "
        "viewport (%d,%d,%d,%d)",
        s_renderImageCallCount, m_imageViewX, m_imageViewY, m_imageViewWidth,
        m_imageViewHeight);
  }

  m_imageRenderer.Render(commandList, zoom, pan, rangeMin, rangeMax, m_showR,
                         m_showG, m_showB, m_imageViewX, m_imageViewY,
                         m_imageViewWidth, m_imageViewHeight, screenWidth,
                         screenHeight);
}

void ImgViewerUI::HandleImageInteraction() {
  const auto &imgData = m_imgViewer.GetImageData();
  if (!m_imgViewer.HasImage())
    return;

  ImGuiIO &io = ImGui::GetIO();
  bool isHovered = ImGui::IsItemHovered();

  if (isHovered) {
    // Zoom with mouse wheel
    if (io.MouseWheel != 0.0f) {
      float oldZoom = m_imgViewer.GetZoom();
      float zoomFactor = (1.0f + io.MouseWheel * 0.1f);
      float newZoom = oldZoom * zoomFactor;

      // Limit zoom
      newZoom = std::max(0.1f, std::min(newZoom, 50.0f));

      // Calculate actual ratio applied
      float ratio = newZoom / oldZoom;

      if (ratio != 1.0f) {
        // We want to keep the point under the mouse stable.
        // ScreenPos = (WorldPos * Zoom) + Pan + Center
        // Mouse = ScreenPos
        // Mouse = (WorldPos * OldZoom) + OldPan + Center
        // Mouse = (WorldPos * NewZoom) + NewPan + Center

        // Solve for NewPan:
        // NewPan = Mouse - Center - (WorldPos * NewZoom)
        // WorldPos = (Mouse - Center - OldPan) / OldZoom
        // NewPan = Mouse - Center - ((Mouse - Center - OldPan) / OldZoom) *
        // NewZoom NewPan = (Mouse - Center) - (Mouse - Center - OldPan) * Ratio
        // NewPan = OldPan * Ratio + RelMouse * (1 - Ratio)

        auto oldPan = m_imgViewer.GetPan();

        // Calculate Mouse Relative to Center of Viewport
        // We need the ACTUAL viewport center, which is (m_imageViewWidth/2,
        // m_imageViewHeight/2) Mouse coordinates are relative to the top-left
        // of the viewport? Need to verify coordinate space of io.MousePos vs
        // m_imageViewX/Y.

        // m_imageViewX/Y is screen coordinates of top-left of viewport.
        float viewCenterX = m_imageViewX + m_imageViewWidth * 0.5f;
        float viewCenterY = m_imageViewY + m_imageViewHeight * 0.5f;

        float relMouseX = io.MousePos.x - viewCenterX;
        float relMouseY = io.MousePos.y - viewCenterY;

        float newPanX = oldPan.x * ratio + relMouseX * (1.0f - ratio);
        float newPanY = oldPan.y * ratio + relMouseY * (1.0f - ratio);

        m_imgViewer.SetZoom(newZoom);
        m_imgViewer.SetPan({newPanX, newPanY});
      }
    }

    // Pan with middle mouse button
    if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
      if (!m_isPanning) {
        m_isPanning = true;
        m_lastMousePos = {io.MousePos.x, io.MousePos.y};
      } else {
        auto pan = m_imgViewer.GetPan();
        pan.x += io.MousePos.x - m_lastMousePos.x;
        pan.y += io.MousePos.y - m_lastMousePos.y;
        m_imgViewer.SetPan(pan);
        m_lastMousePos = {io.MousePos.x, io.MousePos.y};
      }
    } else {
      m_isPanning = false;
    }

    // Calculate hovered pixel using the same viewport coordinates as rendering
    // Use m_imageViewX/Y/Width/Height which are the actual DX12 rendering
    // viewport
    float zoom = m_imgViewer.GetZoom();
    auto pan = m_imgViewer.GetPan();

    int imgWidth = m_imageRenderer.GetImageWidth();
    int imgHeight = m_imageRenderer.GetImageHeight();

    float displayWidth = imgWidth * zoom;
    float displayHeight = imgHeight * zoom;

    // Center image in viewport, then apply pan
    float offsetX = (m_imageViewWidth - displayWidth) * 0.5f + pan.x;
    float offsetY = (m_imageViewHeight - displayHeight) * 0.5f + pan.y;

    // Image top-left in screen coordinates
    float imageScreenX = m_imageViewX + offsetX;
    float imageScreenY = m_imageViewY + offsetY;

    // Mouse position relative to image top-left
    float relMouseX = io.MousePos.x - imageScreenX;
    float relMouseY = io.MousePos.y - imageScreenY;

    // Convert to pixel coordinates
    float pixelX = relMouseX / zoom;
    float pixelY = relMouseY / zoom;

    if (pixelX >= 0 && pixelX < imgData.width && pixelY >= 0 &&
        pixelY < imgData.height) {
      m_hoveredPixel = {pixelX, pixelY};
    } else {
      m_hoveredPixel = {-1, -1};
    }

    // Right-click drag for magnifier
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && m_hoveredPixel.x >= 0) {
      m_showMagnifier = true;
      m_magnifierPos = m_hoveredPixel;
    }
  } else {
    m_isPanning = false;
  }
}

void ImgViewerUI::RenderRangeControls() {
  float rangeMin = m_imgViewer.GetRangeMin();
  float rangeMax = m_imgViewer.GetRangeMax();

  ImGui::Text("Plot Value Range");

  bool changed = false;
  changed |= ImGui::DragFloat("Min", &rangeMin, 0.01f);
  changed |= ImGui::DragFloat("Max", &rangeMax, 0.01f);

  if (changed) {
    m_imgViewer.SetRange(rangeMin, rangeMax);
    UpdateHistogram(); // Update histogram when range changes
  }

  if (ImGui::Button("Auto Range")) {
    const auto &imgData = m_imgViewer.GetImageData();
    float targetMin = 0.0f;
    float targetMax = 1.0f;
    bool apply = false;

    // If all channels are selected, use the pre-calculated global min/max
    if (m_showR && m_showG && m_showB) {
      targetMin = imgData.minValue;
      targetMax = imgData.maxValue;
      apply = true;
    } else if (!m_showR && !m_showG && !m_showB) {
      // No channels selected, do nothing or reset to 0-1
      targetMin = 0.0f;
      targetMax = 1.0f;
      apply = true;
    } else {
      // Calculate min/max for selected channels
      float minVal = FLT_MAX;
      float maxVal = -FLT_MAX;
      int numPixels = imgData.width * imgData.height;

      // Assuming RGBA packed
      for (int i = 0; i < numPixels; ++i) {
        size_t idx = i * 4;

        if (m_showR) {
          float v = imgData.pixels[idx + 0];
          if (!std::isnan(v)) {
            minVal = std::min(minVal, v);
            maxVal = std::max(maxVal, v);
          }
        }
        if (m_showG) {
          float v = imgData.pixels[idx + 1];
          if (!std::isnan(v)) {
            minVal = std::min(minVal, v);
            maxVal = std::max(maxVal, v);
          }
        }
        if (m_showB) {
          float v = imgData.pixels[idx + 2];
          if (!std::isnan(v)) {
            minVal = std::min(minVal, v);
            maxVal = std::max(maxVal, v);
          }
        }
      }

      if (minVal <= maxVal) {
        targetMin = minVal;
        targetMax = maxVal;
        apply = true;
      }
    }

    if (apply) {
      m_imgViewer.SetRange(targetMin, targetMax);
      m_plotViewMin = targetMin;
      m_plotViewMax = targetMax;
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("0-1 Range")) {
    m_imgViewer.SetRange(0.0f, 1.0f);
    m_plotViewMin = 0.0f;
    m_plotViewMax = 1.0f;
  }

  ImGui::Separator();
  ImGui::Text("Channels:");
  ImGui::Checkbox("R", &m_showR);
  ImGui::SameLine();
  ImGui::Checkbox("G", &m_showG);
  ImGui::SameLine();
  ImGui::Checkbox("B", &m_showB);
}

void ImgViewerUI::RenderHistogram() {
  const auto &imgData = m_imgViewer.GetImageData();

  // Legend
  ImGui::TextColored(ImVec4(0.97f, 0.46f, 0.56f, 1.0f), "R"); // #f7768e
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.62f, 0.81f, 0.42f, 1.0f), "G"); // #9ece6a
  ImGui::SameLine();
  ImGui::TextColored(ImVec4(0.48f, 0.64f, 0.97f, 1.0f), "B"); // #7aa2f7

  if (m_histogramR.empty()) {
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                       "Load an image to see histogram");
    return;
  }

  // Calculate size for the plot area - use remaining space
  ImVec2 availSize = ImGui::GetContentRegionAvail();
  if (availSize.x < 10 || availSize.y < 10)
    return;

  // Use BeginChild to create isolated coordinate space for the plot
  ImGui::BeginChild("##HistogramPlot", availSize, ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
  {
    ImVec2 size = ImGui::GetContentRegionAvail();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGuiIO &io = ImGui::GetIO();
    ImDrawList *drawList = ImGui::GetWindowDrawList();

    // Background
    drawList->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y),
                            IM_COL32(20, 20, 20, 255));
    drawList->AddRect(p, ImVec2(p.x + size.x, p.y + size.y),
                      IM_COL32(60, 60, 60, 255));

    ImGui::InvisibleButton("##PlotHitBox", size);
    bool isHovered = ImGui::IsItemHovered();
    bool isActive = ImGui::IsItemActive();

    // -- Interaction Logic --
    float viewRange = m_plotViewMax - m_plotViewMin;
    if (viewRange < 0.00001f)
      viewRange = 1.0f;

    // Zoom (Mouse Wheel)
    if (isHovered && io.MouseWheel != 0.0f) {
      float zoomFactor = (io.MouseWheel > 0) ? 0.9f : 1.1f;
      float mouseRelPos = (io.MousePos.x - p.x) / size.x;
      float mouseVal = m_plotViewMin + mouseRelPos * viewRange;

      float newRange = viewRange * zoomFactor;
      m_plotViewMin = mouseVal - mouseRelPos * newRange;
      m_plotViewMax = mouseVal + (1.0f - mouseRelPos) * newRange;
      viewRange = m_plotViewMax - m_plotViewMin;
    }

    // Pan (Middle Mouse Drag)
    if (isHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
      m_isPanningPlot = true;
    }

    if (m_isPanningPlot) {
      if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        float dx = io.MouseDelta.x / size.x * viewRange;
        m_plotViewMin -= dx;
        m_plotViewMax -= dx;
      } else {
        m_isPanningPlot = false;
      }
    }

    // Helpers
    auto ValToScreenX = [&](float val) -> float {
      return p.x + ((val - m_plotViewMin) / viewRange) * size.x;
    };

    auto ScreenXToVal = [&](float sx) -> float {
      return m_plotViewMin + ((sx - p.x) / size.x) * viewRange;
    };

    // -- Draw Grid & X Axis Labels --
    int gridLines = 10;
    for (int i = 0; i <= gridLines; i++) {
      float t = (float)i / gridLines;
      float val = m_plotViewMin + t * viewRange;
      float sx = p.x + t * size.x;

      // Vertical line
      drawList->AddLine(ImVec2(sx, p.y), ImVec2(sx, p.y + size.y),
                        IM_COL32(50, 50, 50, 100));

      // Label
      char buf[32];
      snprintf(buf, sizeof(buf), "%.2f", val);
      drawList->AddText(ImVec2(sx + 4, p.y + size.y - 16),
                        IM_COL32(150, 150, 150, 255), buf);
    }

    // -- Draw Histograms --
    float histRun = m_histMax - m_histMin;
    if (histRun <= 0)
      histRun = 1.0f;

    // Find Global Max (for Y scaling)
    float maxCount = 1.0f;
    for (int i = 0; i < m_histogramBins; i++) {
      float maxVal = (float)std::max(
          m_histogramR[i], std::max(m_histogramG[i], m_histogramB[i]));
      if (maxVal > 0)
        maxCount = std::max(maxCount, std::log(maxVal + 1.0f));
    }

    // Draw Curve
    auto drawCurve = [&](const std::vector<int> &hist, ImU32 color) {
      if (hist.empty())
        return;
      std::vector<ImVec2> points;
      // Sampling all bins for simplicity
      for (int i = 0; i < m_histogramBins; i++) {
        float binVal = m_histMin + (float)i / (m_histogramBins - 1) * histRun;

        // Clip roughly? RenderAll is fine for 2048 points
        float sx = ValToScreenX(binVal);
        float count = (float)hist[i];
        float valY = (count > 0) ? std::log(count + 1.0f) : 0.0f;
        float sy = p.y + size.y - (valY / maxCount) * size.y;

        points.push_back(ImVec2(sx, sy));
      }
      drawList->AddPolyline(points.data(), (int)points.size(), color,
                            ImDrawFlags_None, 1.5f);
    };

    if (m_showB)
      drawCurve(m_histogramB, IM_COL32(122, 162, 247, 255)); // #7aa2f7
    if (m_showG)
      drawCurve(m_histogramG, IM_COL32(158, 206, 106, 255)); // #9ece6a
    if (m_showR)
      drawCurve(m_histogramR, IM_COL32(247, 118, 142, 255)); // #f7768e

    // -- Selection Handles --
    float currentRangeMin = m_imgViewer.GetRangeMin();
    float currentRangeMax = m_imgViewer.GetRangeMax();

    float sMin = ValToScreenX(currentRangeMin);
    float sMax = ValToScreenX(currentRangeMax);

    // Draw Vertical Lines
    drawList->AddLine(ImVec2(sMin, p.y), ImVec2(sMin, p.y + size.y),
                      IM_COL32(255, 255, 0, 200), 2.0f);
    drawList->AddLine(ImVec2(sMax, p.y), ImVec2(sMax, p.y + size.y),
                      IM_COL32(255, 255, 0, 200), 2.0f);

    // Handle Triangles
    float triHeight = 12.0f;
    float triWidth = 9.0f;

    // Min Handle (Right-Pointing Triangle >)
    // Vertical back aligned with sMin line
    ImVec2 tMin1(sMin, p.y);
    ImVec2 tMin2(sMin, p.y + triHeight);
    ImVec2 tMin3(sMin + triWidth, p.y + triHeight * 0.5f);

    // Max Handle (Left-Pointing Triangle <)
    // Vertical back aligned with sMax line
    ImVec2 tMax1(sMax, p.y);
    ImVec2 tMax2(sMax, p.y + triHeight);
    ImVec2 tMax3(sMax - triWidth, p.y + triHeight * 0.5f);

    // Hit Testing
    // Only drag if mouse is in the area (roughly)
    // We check absolute distance in screen X
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && isHovered) {
      if (!m_isDraggingPlotMin && !m_isDraggingPlotMax) {
        float distMin = std::abs(io.MousePos.x - sMin);
        float distMax = std::abs(io.MousePos.x - sMax);
        float threshold = 10.0f;

        if (distMin < threshold && distMin < distMax)
          m_isDraggingPlotMin = true;
        else if (distMax < threshold)
          m_isDraggingPlotMax = true;
      }
    } else {
      m_isDraggingPlotMin = false;
      m_isDraggingPlotMax = false;
    }

    // Apply Drag
    if (m_isDraggingPlotMin) {
      float val = ScreenXToVal(io.MousePos.x);
      val = std::min(val, currentRangeMax); // Don't cross
      m_imgViewer.SetRange(val, currentRangeMax);
    } else if (m_isDraggingPlotMax) {
      float val = ScreenXToVal(io.MousePos.x);
      val = std::max(val, currentRangeMin); // Don't cross
      m_imgViewer.SetRange(currentRangeMin, val);
    }

    // Draw Handles (Highlight if dragging)
    drawList->AddTriangleFilled(tMin1, tMin2, tMin3,
                                m_isDraggingPlotMin
                                    ? IM_COL32(255, 255, 255, 255)
                                    : IM_COL32(255, 255, 0, 255));
    drawList->AddTriangleFilled(tMax1, tMax2, tMax3,
                                m_isDraggingPlotMax
                                    ? IM_COL32(255, 255, 255, 255)
                                    : IM_COL32(255, 255, 0, 255));

    // -- Crosshair --
    // Only show if hovering and NOT dragging handles
    if (isHovered && !m_isDraggingPlotMin && !m_isDraggingPlotMax) {
      float mx = io.MousePos.x;
      drawList->AddLine(ImVec2(mx, p.y), ImVec2(mx, p.y + size.y),
                        IM_COL32(255, 255, 255, 128));

      float val = ScreenXToVal(mx);
      char valBuf[32];
      snprintf(valBuf, sizeof(valBuf), "%.4f", val);
      drawList->AddText(ImVec2(mx + 4, io.MousePos.y),
                        IM_COL32(255, 255, 255, 255), valBuf);
    }
  }
  ImGui::EndChild();
}

void ImgViewerUI::RenderMagnifier() {
  const auto &imgData = m_imgViewer.GetImageData();
  if (!m_imgViewer.HasImage())
    return;

  int magnifySize = 13;    // 13x13 pixels
  float pixelSize = 15.0f; // Size of each magnified pixel on screen

  ImVec2 size(magnifySize * pixelSize, magnifySize * pixelSize);

  // Position for the magnifier content
  ImVec2 p = ImGui::GetCursorScreenPos();

  ImGui::InvisibleButton("##MagnifierArea", size);

  ImDrawList *drawList = ImGui::GetWindowDrawList();

  // Background for out of bounds
  drawList->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y),
                          IM_COL32(20, 20, 20, 255));

  int halfSize = magnifySize / 2;
  int centerX = (int)m_magnifierPos.x;
  int centerY = (int)m_magnifierPos.y;

  for (int y = 0; y < magnifySize; y++) {
    for (int x = 0; x < magnifySize; x++) {
      int imgX = centerX + (x - halfSize);
      int imgY = centerY + (y - halfSize);

      // Calculate screen position for this pixel rect
      float screenX = p.x + x * pixelSize;
      float screenY = p.y + y * pixelSize;

      if (imgX >= 0 && imgX < imgData.width && imgY >= 0 &&
          imgY < imgData.height) {
        // Get pixel color
        int pixelIdx = (imgY * imgData.width + imgX) * 4;
        float r = imgData.pixels[pixelIdx + 0];
        float g = imgData.pixels[pixelIdx + 1];
        float b = imgData.pixels[pixelIdx + 2];
        float a = imgData.pixels[pixelIdx + 3];

        // Apply Range and Channel Settings
        float rangeMin = m_imgViewer.GetRangeMin();
        float rangeMax = m_imgViewer.GetRangeMax();
        float rangeSize = rangeMax - rangeMin;
        if (rangeSize <= 0.0f)
          rangeSize = 1.0f;

        auto remap = [&](float val) -> float {
          return std::max(0.0f, std::min(1.0f, (val - rangeMin) / rangeSize));
        };

        if (!m_showR)
          r = 0.0f;
        else
          r = remap(r);

        if (!m_showG)
          g = 0.0f;
        else
          g = remap(g);

        if (!m_showB)
          b = 0.0f;
        else
          b = remap(b);

        ImU32 color = ImGui::ColorConvertFloat4ToU32(
            ImVec4(r, g, b,
                   1.0f)); // Force alpha to 1 for visibility
        // Using checkerboard for transparency would be nice, but simple color
        // is fine for now. Let's use alpha blending if A < 1

        // Draw pixel
        drawList->AddRectFilled(
            ImVec2(screenX, screenY),
            ImVec2(screenX + pixelSize, screenY + pixelSize), color);
      }

      // Draw grid lines
      drawList->AddRect(ImVec2(screenX, screenY),
                        ImVec2(screenX + pixelSize, screenY + pixelSize),
                        IM_COL32(50, 50, 50, 255));
    }
  }

  // Highlight center pixel
  {
    float screenX = p.x + halfSize * pixelSize;
    float screenY = p.y + halfSize * pixelSize;
    drawList->AddRect(ImVec2(screenX, screenY),
                      ImVec2(screenX + pixelSize, screenY + pixelSize),
                      IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);
  }

  ImGui::Text("Magnifier at (%d, %d)", centerX, centerY);

  // Display Center Pixel Info
  if (centerX >= 0 && centerX < imgData.width && centerY >= 0 &&
      centerY < imgData.height) {
    int pixelIdx = (centerY * imgData.width + centerX) * 4;

    float r = imgData.pixels[pixelIdx + 0];
    float g = imgData.pixels[pixelIdx + 1];
    float b = imgData.pixels[pixelIdx + 2];
    float a = imgData.pixels[pixelIdx + 3];

    ImGui::Text("R: %.4f  G: %.4f", r, g);
    ImGui::Text("B: %.4f  A: %.4f", b, a);

    // Optional: Hex representation
    ImU32 colorU32 = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
    ImGui::Text("Hex: #%08X", colorU32);
  }

  if (ImGui::Button("Close Magnifier")) {
    m_showMagnifier = false;
  }
}

void ImgViewerUI::UpdateHistogram() {
  const auto &imgData = m_imgViewer.GetImageData();
  if (!m_imgViewer.HasImage())
    return;

  // Check if bins size matches (in case it was changed dynamically, though we
  // use fixed 2048 now)
  if (m_histogramR.size() != m_histogramBins) {
    m_histogramR.resize(m_histogramBins);
    m_histogramG.resize(m_histogramBins);
    m_histogramB.resize(m_histogramBins);
  }

  // Clear histograms
  std::fill(m_histogramR.begin(), m_histogramR.end(), 0);
  std::fill(m_histogramG.begin(), m_histogramG.end(), 0);
  std::fill(m_histogramB.begin(), m_histogramB.end(), 0);

  // Use global image range
  float rangeMin = imgData.minValue;
  float rangeMax = imgData.maxValue;
  if (rangeMax <= rangeMin)
    rangeMax = rangeMin + 1.0f;

  m_histMin = rangeMin;
  m_histMax = rangeMax;

  float rangeSize = rangeMax - rangeMin;

  // Build histograms
  int numPixels = imgData.width * imgData.height;
  for (int i = 0; i < numPixels; i++) {
    int pixelIdx = i * 4;

    for (int ch = 0; ch < 3; ch++) {
      float value = imgData.pixels[pixelIdx + ch];

      if (std::isnan(value))
        continue;

      // Map to bin
      int bin = (int)((value - rangeMin) / rangeSize * (m_histogramBins - 1));
      bin = std::max(0, std::min(m_histogramBins - 1, bin));

      if (ch == 0)
        m_histogramR[bin]++;
      else if (ch == 1)
        m_histogramG[bin]++;
      else
        m_histogramB[bin]++;
    }
  }
}

void ImgViewerUI::HandleDragDrop(const std::string &filepath) {
  LOG("ImgViewerUI::HandleDragDrop - filepath=%s", filepath.c_str());

  // Clear existing texture before loading new one
  if (m_imageRenderer.HasTexture()) {
    m_renderer->WaitForGpu();
    m_imageRenderer.ClearTexture();
  }

  if (m_imgViewer.LoadImage(filepath)) {
    LOG("ImgViewerUI::HandleDragDrop - Image loaded successfully");
    const auto &imgData = m_imgViewer.GetImageData();
    LOG("ImgViewerUI::HandleDragDrop - Image size: %dx%d, pixels.size=%zu",
        imgData.width, imgData.height, imgData.pixels.size());

    UpdateHistogram();
    LOG("ImgViewerUI::HandleDragDrop - Histogram updated");

    // Reset Plot View to full range
    m_plotViewMin = m_histMin;
    m_plotViewMax = m_histMax;

    // Upload image to GPU
    LOG("ImgViewerUI::HandleDragDrop - Starting GPU upload...");
    m_renderer->BeginRender();
    bool uploadResult = m_imageRenderer.UploadImage(
        m_renderer->GetDevice(), m_renderer->GetCommandList(),
        m_imgViewer.GetImageData());
    m_renderer->EndRender();

    if (uploadResult) {
      LOG("ImgViewerUI::HandleDragDrop - GPU upload successful! "
          "HasTexture=%d",
          m_imageRenderer.HasTexture() ? 1 : 0);
    } else {
      LOG_ERROR("ImgViewerUI::HandleDragDrop - GPU upload FAILED!");
    }
  } else {
    LOG_ERROR("ImgViewerUI::HandleDragDrop - Failed to load image: %s",
              filepath.c_str());
  }
}

void ImgViewerUI::SetupImGuiStyle() {
  ImGuiStyle &style = ImGui::GetStyle();

  // Rounding
  style.WindowRounding = 6.0f;
  style.FrameRounding = 4.0f;
  style.PopupRounding = 4.0f;
  style.ScrollbarRounding = 4.0f;
  style.GrabRounding = 4.0f;
  style.TabRounding = 6.0f;

  // Sizes
  style.WindowBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.PopupBorderSize = 1.0f;
  style.FramePadding = ImVec2(8, 4);
  style.ItemSpacing = ImVec2(8, 6);
  style.ScrollbarSize = 14.0f;
  style.WindowPadding = ImVec2(10, 10);

  // Colors (Tokyo Night Theme)
  ImVec4 *colors = style.Colors;

  // Tokyo Night Palette
  // Backgrounds: #1a1b26 (Window), #16161e (Title), #24283b (Frames)
  // Foreground: #c0caf5 (Text), #a9b1d6 (Subtext)
  // Accents: #7aa2f7 (Blue), #bb9af7 (Purple), #9ece6a (Green)

  // Backgrounds
  colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.11f, 0.15f, 1.00f); // #1a1b26
  colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.11f, 0.15f, 1.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.11f, 0.15f, 0.98f);

  // Headers / Title Bars
  colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.09f, 0.12f, 1.00f); // #16161e
  colors[ImGuiCol_TitleBgActive] = ImVec4(0.09f, 0.09f, 0.12f, 1.00f);
  colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.09f, 0.09f, 0.12f, 1.00f);
  colors[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.09f, 0.12f, 1.00f);

  // Borders
  colors[ImGuiCol_Border] = ImVec4(0.34f, 0.37f, 0.54f, 0.50f); // #565f89
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

  // Frames (Checkboxes, Inputs, Buttons)
  colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.16f, 0.23f, 1.00f); // #24283b
  colors[ImGuiCol_FrameBgHovered] =
      ImVec4(0.25f, 0.28f, 0.41f, 1.00f); // #414868
  colors[ImGuiCol_FrameBgActive] =
      ImVec4(0.34f, 0.37f, 0.54f, 1.00f); // #565f89

  // Tabs
  colors[ImGuiCol_Tab] = ImVec4(0.10f, 0.11f, 0.15f, 1.00f);
  colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.28f, 0.41f, 1.00f);
  colors[ImGuiCol_TabActive] = ImVec4(0.14f, 0.16f, 0.23f, 1.00f);
  colors[ImGuiCol_TabUnfocused] = ImVec4(0.10f, 0.11f, 0.15f, 1.00f);
  colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.16f, 0.23f, 1.00f);

  // Interactive
  colors[ImGuiCol_CheckMark] =
      ImVec4(0.48f, 0.64f, 0.97f, 1.00f); // #7aa2f7 (Blue Accent)
  colors[ImGuiCol_SliderGrab] = ImVec4(0.48f, 0.64f, 0.97f, 1.00f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.58f, 0.74f, 1.00f, 1.00f);
  colors[ImGuiCol_Button] = ImVec4(0.25f, 0.28f, 0.41f, 1.00f); // #414868
  colors[ImGuiCol_ButtonHovered] =
      ImVec4(0.34f, 0.37f, 0.54f, 1.00f);                             // #565f89
  colors[ImGuiCol_ButtonActive] = ImVec4(0.48f, 0.64f, 0.97f, 1.00f); // #7aa2f7

  // Headers (Dropdowns, Collapsing Headers)
  colors[ImGuiCol_Header] = ImVec4(0.25f, 0.28f, 0.41f, 1.00f); // #414868
  colors[ImGuiCol_HeaderHovered] =
      ImVec4(0.34f, 0.37f, 0.54f, 1.00f);                             // #565f89
  colors[ImGuiCol_HeaderActive] = ImVec4(0.48f, 0.64f, 0.97f, 1.00f); // #7aa2f7

  // Text
  colors[ImGuiCol_Text] = ImVec4(0.75f, 0.79f, 0.96f, 1.00f);         // #c0caf5
  colors[ImGuiCol_TextDisabled] = ImVec4(0.34f, 0.37f, 0.54f, 1.00f); // #565f89
}

void ImgViewerUI::RenderTitleBar() {
  ImGuiViewport *viewport = ImGui::GetMainViewport();
  float titleBarHeight = 32.0f;

  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, titleBarHeight));

  ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
      ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
      ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoNav; // Use a unique name
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 5));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_WindowBg,
                        ImVec4(0.09f, 0.09f, 0.12f, 1.00f)); // Dark title bar

  if (ImGui::Begin("##TitleBar", nullptr, flags)) {
    // 1. App Icon/Title
    ImGui::SetCursorPos(ImVec2(10, 5)); // Use specific padding

    // Use Title Font (Index 1) if available
    if (ImGui::GetIO().Fonts->Fonts.Size > 1) {
      ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
    }

    ImGui::TextColored(ImVec4(0.4f, 0.6f, 1.0f, 1.0f),
                       "IMG"); // Placeholder icon
    ImGui::SameLine();
    ImGui::Text("ImgViewer");

    if (ImGui::GetIO().Fonts->Fonts.Size > 1) {
      ImGui::PopFont();
    }

    ImGui::SameLine(0, 20); // Spacing
    ImGui::SetCursorPosY(
        0.0f); // Align to top so 32px button is centered in 32px title bar

    // 2. Menu Bar (Embedded)
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // Transparent
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(1.0f, 1.0f, 1.0f, 0.1f)); // Subtle hover
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign,
                        ImVec2(0.5f, 0.5f)); // Center text
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2(0, 0)); // Remove padding to allow full centering

    if (ImGui::Button("File", ImVec2(55, 32))) {
      ImGui::OpenPopup("FileMenu");
    }

    // Calculate position for the popup (bottom-left of the button)
    ImVec2 btnMin = ImGui::GetItemRectMin();
    ImVec2 btnMax = ImGui::GetItemRectMax();
    ImVec2 popupPos(btnMin.x, btnMax.y);

    m_titleBarInteractWidth = btnMax.x + 10.0f;
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3); // Pop FrameRounding, ButtonTextAlign, FramePadding

    ImGui::SetNextWindowPos(popupPos);
    if (ImGui::BeginPopup("FileMenu")) {
      if (ImGui::MenuItem("Open...", "Ctrl+O")) {
        OpenFileDialog();
      }
      if (ImGui::MenuItem("Paste from Clipboard", "Ctrl+V")) {
        PasteFromClipboard();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Configuration", nullptr, &m_showConfigPanel)) {
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Exit", "Alt+F4")) {
        exit(0);
      }
      ImGui::EndPopup();
    }

    // Window Controls (Right Aligned)
    float buttonWidth = 46.0f;  // Wider buttons
    float buttonHeight = 32.0f; // Full height
    float buttonsAreaWidth = buttonWidth * 3;

    // Reset Y to 0 for buttons to be flush with top
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - buttonsAreaWidth, 0));

    // We need to remove padding to make buttons flush with edges if
    // desired, or keep them as isolated buttons. Modern apps often have
    // flush buttons in top right. Let's keep them as buttons but style
    // them.

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Button,
                          ImVec4(0, 0, 0, 0)); // Transparent background

    // Minimize
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
    if (ImGui::Button("##min", ImVec2(buttonWidth, buttonHeight))) {
      ShowWindow(GetActiveWindow(), SW_MINIMIZE);
    }
    // Custom Draw for Minimize Icon (Underscore)
    // Draw ON TOP of button (which is transparent)
    // Note: GetWindowDrawList draws in window local space? No, screen space
    // usually? Logic: GetWindowDrawList() is safer.
    {
      ImVec2 rectMin = ImGui::GetItemRectMin();
      ImVec2 rectMax = ImGui::GetItemRectMax();
      ImVec2 center = ImVec2((rectMin.x + rectMax.x) * 0.5f,
                             (rectMin.y + rectMax.y) * 0.5f);
      ImGui::GetWindowDrawList()->AddLine(ImVec2(center.x - 5, center.y + 2),
                                          ImVec2(center.x + 5, center.y + 2),
                                          IM_COL32(200, 200, 200, 255), 1.0f);
    }
    ImGui::PopStyleColor();

    // Maximize/Restore
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
    bool isMaximized = IsZoomed(GetActiveWindow());
    if (ImGui::Button("##max", ImVec2(buttonWidth, buttonHeight))) {
      if (isMaximized)
        ShowWindow(GetActiveWindow(), SW_RESTORE);
      else
        ShowWindow(GetActiveWindow(), SW_MAXIMIZE);
    }
    // Custom Draw for Maximize Icon (Square) or Restore (Two Squares)
    {
      ImVec2 rectMin = ImGui::GetItemRectMin();
      ImVec2 rectMax = ImGui::GetItemRectMax();
      ImVec2 center = ImVec2((rectMin.x + rectMax.x) * 0.5f,
                             (rectMin.y + rectMax.y) * 0.5f);
      if (isMaximized) {
        // Restore icon (simplified)
        ImGui::GetWindowDrawList()->AddRect(ImVec2(center.x - 4, center.y - 1),
                                            ImVec2(center.x + 2, center.y + 5),
                                            IM_COL32(200, 200, 200, 255), 0.0f,
                                            0, 1.0f);
      } else {
        // Maximize icon
        ImGui::GetWindowDrawList()->AddRect(ImVec2(center.x - 4, center.y - 4),
                                            ImVec2(center.x + 4, center.y + 4),
                                            IM_COL32(200, 200, 200, 255), 0.0f,
                                            0, 1.0f);
      }
    }
    ImGui::PopStyleColor();

    // Close
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          ImVec4(0.9f, 0.2f, 0.2f, 1.0f)); // Red on hover
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          ImVec4(0.7f, 0.1f, 0.1f, 1.0f));
    if (ImGui::Button("##close", ImVec2(buttonWidth, buttonHeight))) {
      exit(0);
    }
    // Custom Draw for Close Icon (X)
    {
      ImVec2 rectMin = ImGui::GetItemRectMin();
      ImVec2 rectMax = ImGui::GetItemRectMax();
      ImVec2 center = ImVec2((rectMin.x + rectMax.x) * 0.5f,
                             (rectMin.y + rectMax.y) * 0.5f);
      ImGui::GetWindowDrawList()->AddLine(ImVec2(center.x - 4, center.y - 4),
                                          ImVec2(center.x + 4, center.y + 4),
                                          IM_COL32(200, 200, 200, 255), 1.0f);
      ImGui::GetWindowDrawList()->AddLine(ImVec2(center.x + 4, center.y - 4),
                                          ImVec2(center.x - 4, center.y + 4),
                                          IM_COL32(200, 200, 200, 255), 1.0f);
    }
    ImGui::PopStyleColor(2);

    ImGui::PopStyleColor(); // Button Transparent
    ImGui::PopStyleVar(2);

    // Decorative Line under Title Bar
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(viewport->Pos.x, viewport->Pos.y + titleBarHeight),
        ImVec2(viewport->Pos.x + viewport->Size.x,
               viewport->Pos.y + titleBarHeight),
        IM_COL32(122, 162, 247, 255), // Tokyo Night Blue Accent #7aa2f7
        2.0f);                        // Thicker line
  }
  ImGui::End();

  ImGui::PopStyleColor(); // WindowBg
  ImGui::PopStyleVar(2);
}

void ImgViewerUI::OpenFileDialog() {
  OPENFILENAMEA ofn = {};
  char filename[MAX_PATH] = {};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = NULL;
  ofn.lpstrFilter =
      "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.hdr;*.dds\0All "
      "Files\0*.*\0\0";
  ofn.lpstrFile = filename;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
  if (GetOpenFileNameA(&ofn)) {
    // Clear existing texture before loading new one
    if (m_imageRenderer.HasTexture()) {
      m_renderer->WaitForGpu();
      m_imageRenderer.ClearTexture();
    }

    if (m_imgViewer.LoadImage(filename)) {
      UpdateHistogram();
      m_plotViewMin = m_histMin;
      m_plotViewMax = m_histMax;
      m_renderer->BeginRender();
      m_imageRenderer.UploadImage(m_renderer->GetDevice(),
                                  m_renderer->GetCommandList(),
                                  m_imgViewer.GetImageData());
      m_renderer->EndRender();
    }
  }
}

void ImgViewerUI::PasteFromClipboard() {
  // Clear existing texture before loading new one
  if (m_imageRenderer.HasTexture()) {
    m_renderer->WaitForGpu();
    m_imageRenderer.ClearTexture();
  }

  if (m_imgViewer.LoadImageFromClipboard()) {
    UpdateHistogram();
    m_plotViewMin = m_histMin;
    m_plotViewMax = m_histMax;

    // Upload the new image to GPU
    m_renderer->BeginRender();
    m_imageRenderer.UploadImage(m_renderer->GetDevice(),
                                m_renderer->GetCommandList(),
                                m_imgViewer.GetImageData());
    m_renderer->EndRender();
  }
}

void ImgViewerUI::HandleGlobalShortcuts() {
  // Check for Ctrl+O
  if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) {
    OpenFileDialog();
  }

  // Check for Ctrl+V
  if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
    PasteFromClipboard();
  }
}

void ImgViewerUI::RenderConfigPanel() {
  if (!m_showConfigPanel)
    return;

  // Make Config Panel non-dockable so it floats
  if (ImGui::Begin("Configuration", &m_showConfigPanel,
                   ImGuiWindowFlags_NoDocking |
                       ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("UI Settings");
    ImGui::Separator();

    ImGui::Text("Crossline Color");
    ImGui::ColorPicker4("##CrosslineColor", m_crosslineColor,
                        ImGuiColorEditFlags_AlphaBar |
                            ImGuiColorEditFlags_NoSidePreview |
                            ImGuiColorEditFlags_NoSmallPreview);

    ImGui::Separator();
    ImGui::Text("Layout");
    if (ImGui::Button("Reset to Default Layout")) {
      m_resetLayout = true;
    }
  }
  ImGui::End();
}

void ImgViewerUI::ApplyDefaultLayout(ImGuiID dockspaceId) {
  ImGui::DockBuilderRemoveNode(dockspaceId);
  ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

  ImGuiID dock_main_id = dockspaceId;
  ImGuiID dock_id_right =
      ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr,
                                  &dock_main_id); // Info panel on Right
  ImGuiID dock_id_bottom =
      ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr,
                                  &dock_main_id); // Plot/Histogram on Bottom

  ImGui::DockBuilderDockWindow("Image View", dock_main_id);
  ImGui::DockBuilderDockWindow("Info", dock_id_right);
  ImGui::DockBuilderDockWindow("Plot", dock_id_bottom);

  ImGui::DockBuilderFinish(dockspaceId);
}
