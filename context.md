# ImageViewer Context - Session 2026-01-11

## Current State
The application builds and runs with:
- ImGui docking branch (SDKs/imgui-docking)
- DX12 rendering
- Dockable panels: "Image View", "Info", "Plot"

## Recent Changes

### Build System
- CMakeLists.txt uses `imgui-docking` (not imgui_old or imgui-1.92.5)
- Build command: Run `build_ninja.bat` from project root
- Build output: `build_ninja/RayTracingPlayground.exe`

### ImGui Docking Setup
- Enabled `ImGuiConfigFlags_DockingEnable` in main.cpp
- Fixed ImGui DX12 init to use new struct-based API (`ImGui_ImplDX12_InitInfo`)
- Added `GetCommandQueue()` to DX12Renderer.h

### UI Changes (ImageViewerUI.cpp)
1. **Dockspace**: Created proper dockspace covering main window area
2. **Three dockable windows**:
   - "Image View" - displays loaded image
   - "Info" - image metadata and controls
   - "Plot" - RGB histogram with range controls

3. **Histogram**: Changed from bar chart to smooth curves
   - Uses `AddPolyline` for R/G/B channels
   - Log scale for better visualization
   - Fixed mouse input issue: uses `InvisibleButton` instead of `Dummy`

4. **Image rendering**: Uses `ImGui::AddImage()` with GPU descriptor handle
   - Added getters to ImageRenderer: `GetSrvGpuHandle()`, `GetImageWidth()`, `GetImageHeight()`

### Fixed Issues
- Blue edge at window border (changed clear color to dark gray)
- ImGui DX12 crash (updated to new init API with CommandQueue)
- Mouse hover issue in Plot view (use InvisibleButton for proper input handling)

## Known Issues / TODO
1. Image display may not work correctly (texture format RGBA32F with ImGui)
2. Range remapping not applied when displaying image (was in custom shader, now using ImGui::Image)
3. Magnifier not implemented
4. Need to test with actual images

## Key Files Modified
- `main.cpp` - ImGui init with docking + new DX12 API
- `DX12Renderer.h/.cpp` - Added GetCommandQueue()
- `ImageViewerUI.cpp` - Dockspace, dockable windows, curve histogram
- `ImageRenderer.h` - Added getters for GPU handle and dimensions
- `CMakeLists.txt` - Uses imgui-docking

## How to Build
```batch
cd D:\Code\ray-playground\ImageViewer
build_ninja.bat
```

## How to Run
```
build_ninja\RayTracingPlayground.exe
```
