# ImgViewer

**ImgViewer** is a lightweight, high-performance image viewer designed for graphics programmers. It is inspired by tools like [WinPIX](https://devblogs.microsoft.com/pix/download/) and [RenderDoc](https://renderdoc.org/), providing a quick way to inspect image files without the overhead of heavy debugging suites.

## Features

- **High Dynamic Range (HDR) Support**: View `.hdr` and `.exr` (via stb_image) and other floating point formats.
- **DDS Support**: Native support for DirectDraw Surface formats including compressed textures (BC1-BC7) and float formats (RGBA32F, RGBA16F).
- **Pixel Inspection**: Hover over any pixel to see its exact RGBA values in float precision.
- **Histogram**: Real-time RGB histogram visualization.
- **Value Range Analysis**: Automatically detects min/max values and allows manual range remapping (useful for depth maps or HDR values > 1.0).
- **Magnifier**: Inspect pixel-level details with a built-in magnifier tool.
- **Modern UI**: Clean, borderless window with docking support using ImGui.
- **Performance**: GPU-accelerated rendering using DirectX 12.

## Supported Formats

- **Common**: PNG, BMP, TGA, JPG, GIF
- **HDR**: HDR (Radiance RGBE)
- **DirectX**: DDS (BC1-BC7, Uncompressed, Float)

## Build Instructions

### Prerequisites
- Windows 10/11
- Visual Studio 2022 (with "Desktop development with C++" workload)
- CMake 3.23+

### Building via Command Line
```bash
# Generate build files (using Ninja or Visual Studio)
cmake -B build
# Build project
cmake --build build --config Release
```

### Building via Visual Studio
1. Open the folder in Visual Studio.
2. Let CMake configure the project.
3. Select `imgViewer.exe` as the startup target.
4. Build and Run.

## Usage

- **Open Image**: Drag and drop an image file into the window, or paste from clipboard (`Ctrl+V` support coming soon).
- **Pan**: Middle Mouse Button drag.
- **Zoom**: Mouse Wheel.
- **Magnify**: Right-click to show the magnifier.
- **Inspect**: Hover over the image to see pixel values in the Info panel.

## License

This project is open source.