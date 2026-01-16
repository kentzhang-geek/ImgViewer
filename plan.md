# ImageViewer for Graphics Programmers

## Overview
A read-only image viewer application designed for graphics programmers who need to quickly view and analyze images during development. The application provides features similar to RenderDoc's image viewer for inspecting and analyzing image data.

## Technology Stack
- **Graphics API**: DirectX 12
- **GUI Framework**: ImGui
- **Image Loading**:
  - DDS format: detex SDK
  - Common formats (PNG, JPEG, BMP, TGA, etc.): stb_image

## Core Features

### User Interface Layout
- **Main Panel**: Contains the primary image viewing and analysis interface
  - **Left Side**: Main image view with zoom, pan, and crosshair functionality
  - **Top Right**: Plot value range controls (upper and lower limits for x-axis)
  - **Bottom Right**: Magnified view of the area under the right-click position
- **Info Panel**: Displays image metadata and properties
  - Image dimensions (width, height)
  - Image format (file format)
  - Pixel format (internal representation, e.g., RGBA8, RGB32F, etc.)
  - Other relevant image information

### Image Viewing
- Display images with support for multiple formats
- **File Loading**: Drag and drop image files onto the window to open them
- **Zoom**: Ability to zoom in/out on images
- **Pan**: Middle mouse button drag to pan/scroll the image
- **Crosshair**: Visual crosshair overlay to select and inspect specific pixels
- **Pixel Information**: Display pixel values (RGB/RGBA) when hovering over the image with the mouse cursor
- **Magnified View**: Right-click on the main image view to show a zoomed-in view of the clicked area in the bottom-right panel
  - Image magnification uses box filter for scaling

### Image Analysis
- **Value Distribution Plot**: Histogram/plot showing pixel value distribution
  - Separate plots for R, G, B channels
  - Configurable plot range: adjustable upper and lower limits for the x-axis (value range)
  - Plot range controls located in the top-right panel
- **Color Mapping**: The plot value range (upper and lower limits) affects the color mapping of the main image view
  - Adjusting the plot range dynamically updates how pixel values are displayed in the image
- **Auto Range Detection**: When an image is loaded (via drag and drop or other methods), automatically detect the min/max value range from the image data and set the plot limits accordingly
  - The auto-detected range immediately affects the image color mapping
  - NaN values must be detected and filtered out during range detection
- **Pixel Inspection**: Real-time pixel value display at cursor position

### Clipboard Integration
- Extract and load images from the system clipboard
- Support for clipboard image formats (bitmap, PNG, etc.)

## Design Constraints
- **Read-only**: The application does not provide image editing capabilities
- Focus on viewing and analysis features only
- **File Modification Restriction**: AI assistants are strictly prohibited from modifying, creating, or deleting any files outside of this project directory (`D:\Code\ray-playground\ImageViewer\`). Only files within this directory may be modified, created, or deleted.
- **Operation Logging**: AI assistants must log all significant operations and decisions to `ai.md` in a concise format to save tokens. This log helps restore context when starting a new session.
