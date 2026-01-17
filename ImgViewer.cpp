#include "ImgViewer.h"
#include "pch.h"
#include <algorithm>
#include <cmath>

#define STBI_WINDOWS_UTF8
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "Logger.h"
#include <DirectXTex.h>
#include <Windows.h> // Required for MultiByteToWideChar
#include <filesystem>
#include <jpeglib.h>
#include <setjmp.h>
#include <stdio.h>


// Helper to convert UTF-8 std::string to std::wstring
static std::wstring Utf8ToWide(const std::string &str) {
  if (str.empty())
    return std::wstring();
  int size_needed =
      MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
  std::wstring wstrTo(size_needed, 0);
  MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0],
                      size_needed);
  return wstrTo;
}

ImgViewer::ImgViewer() {}

ImgViewer::~ImgViewer() {}

bool ImgViewer::LoadImage(const std::string &filepath) {
  Clear();

  // Determine file type by extension
  std::string ext = filepath.substr(filepath.find_last_of('.') + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  bool success = false;
  if (ext == "dds") {
    success = LoadDDS(filepath);
  } else if (ext == "jpg" || ext == "jpeg") {
    success = LoadJpeg(filepath);
  } else {
    success = LoadSTB(filepath);
  }

  if (success) {
    m_imageData.filename = filepath.substr(filepath.find_last_of("/\\") + 1);
    AnalyzeImageRange();

    // Set initial range to detected range
    m_rangeMin = m_imageData.minValue;
    m_rangeMax = m_imageData.maxValue;
  }

  return success;
}

bool ImgViewer::LoadSTB(const std::string &filepath) {
  int width, height, channels;

  // First try to load as HDR
  // stbi_is_hdr handles UTF-8 on Windows if STBI_WINDOWS_UTF8 is defined
  if (stbi_is_hdr(filepath.c_str())) {
    float *data = stbi_loadf(filepath.c_str(), &width, &height, &channels, 4);
    if (!data)
      return false;

    m_imageData.width = width;
    m_imageData.height = height;
    m_imageData.channels = channels;
    m_imageData.format = "HDR";
    m_imageData.pixelFormat = "RGBA32F";

    size_t pixelCount = width * height * 4;
    m_imageData.pixels.resize(pixelCount);
    memcpy(m_imageData.pixels.data(), data, pixelCount * sizeof(float));

    stbi_image_free(data);
    return true;
  } else {
    // Load as LDR
    unsigned char *data =
        stbi_load(filepath.c_str(), &width, &height, &channels, 4);
    if (!data)
      return false;

    m_imageData.width = width;
    m_imageData.height = height;
    m_imageData.channels = channels;

    // Determine format from extension
    std::string ext = filepath.substr(filepath.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
    m_imageData.format = ext;
    m_imageData.pixelFormat = "RGBA8";

    size_t pixelCount = width * height * 4;
    m_imageData.pixels.resize(pixelCount);

    // Convert from byte to float [0, 1]
    for (size_t i = 0; i < pixelCount; i++) {
      m_imageData.pixels[i] = data[i] / 255.0f;
    }

    stbi_image_free(data);
    return true;
  }
}

bool ImgViewer::LoadDDS(const std::string &filepath) {
  using namespace DirectX;

  std::wstring wfilepath = Utf8ToWide(filepath);

  TexMetadata metadata;
  ScratchImage image;

  HRESULT hr =
      LoadFromDDSFile(wfilepath.c_str(), DDS_FLAGS_NONE, &metadata, image);
  if (FAILED(hr))
    return false;

  m_imageData.width = static_cast<int>(metadata.width);
  m_imageData.height = static_cast<int>(metadata.height);
  m_imageData.format = "DDS";

  // Convert format name
  switch (metadata.format) {
  case DXGI_FORMAT_R8G8B8A8_UNORM:
    m_imageData.pixelFormat = "RGBA8";
    m_imageData.channels = 4;
    break;
  case DXGI_FORMAT_R32G32B32A32_FLOAT:
    m_imageData.pixelFormat = "RGBA32F";
    m_imageData.channels = 4;
    break;
  case DXGI_FORMAT_R16G16B16A16_FLOAT:
    m_imageData.pixelFormat = "RGBA16F";
    m_imageData.channels = 4;
    break;
  case DXGI_FORMAT_BC1_UNORM:
    m_imageData.pixelFormat = "BC1";
    m_imageData.channels = 4;
    break;
  case DXGI_FORMAT_BC3_UNORM:
    m_imageData.pixelFormat = "BC3";
    m_imageData.channels = 4;
    break;
  case DXGI_FORMAT_BC7_UNORM:
    m_imageData.pixelFormat = "BC7";
    m_imageData.channels = 4;
    break;
  default:
    m_imageData.pixelFormat = "Unknown";
    m_imageData.channels = 4;
    break;
  }

  // Decompress if needed
  ScratchImage decompressed;
  if (IsCompressed(metadata.format)) {
    hr = Decompress(image.GetImages(), image.GetImageCount(), metadata,
                    DXGI_FORMAT_R32G32B32A32_FLOAT, decompressed);
    if (FAILED(hr))
      return false;

    image = std::move(decompressed);
    metadata = image.GetMetadata();
  }

  // Convert to RGBA32F if not already
  if (metadata.format != DXGI_FORMAT_R32G32B32A32_FLOAT) {
    ScratchImage converted;
    hr = Convert(image.GetImages(), image.GetImageCount(), metadata,
                 DXGI_FORMAT_R32G32B32A32_FLOAT, TEX_FILTER_DEFAULT,
                 TEX_THRESHOLD_DEFAULT, converted);
    if (FAILED(hr))
      return false;

    image = std::move(converted);
    metadata = image.GetMetadata();
  }

  // Copy pixel data
  const Image *img = image.GetImage(0, 0, 0);
  if (!img)
    return false;

  size_t pixelCount = m_imageData.width * m_imageData.height * 4;
  m_imageData.pixels.resize(pixelCount);
  memcpy(m_imageData.pixels.data(), img->pixels, pixelCount * sizeof(float));

  return true;
}

void ImgViewer::AnalyzeImageRange() {
  if (m_imageData.pixels.empty())
    return;

  float minVal = FLT_MAX;
  float maxVal = -FLT_MAX;
  bool foundNaN = false;

  // Analyze all channels
  for (float value : m_imageData.pixels) {
    if (std::isnan(value)) {
      foundNaN = true;
      continue;
    }

    minVal = std::min(minVal, value);
    maxVal = std::max(maxVal, value);
  }

  m_imageData.hasNaN = foundNaN;
  m_imageData.minValue = (minVal == FLT_MAX) ? 0.0f : minVal;
  m_imageData.maxValue = (maxVal == -FLT_MAX) ? 1.0f : maxVal;
}

bool ImgViewer::LoadImageFromClipboard() {
  Clear();

  if (!OpenClipboard(nullptr))
    return false;

  bool success = false;

  // Try to get DIB format
  HANDLE hDIB = GetClipboardData(CF_DIB);
  if (hDIB) {
    BITMAPINFO *pBitmapInfo = (BITMAPINFO *)GlobalLock(hDIB);
    if (pBitmapInfo) {
      BITMAPINFOHEADER &bmih = pBitmapInfo->bmiHeader;

      if (bmih.biBitCount == 24 || bmih.biBitCount == 32) {
        m_imageData.width = bmih.biWidth;
        m_imageData.height = abs(bmih.biHeight);
        m_imageData.channels = bmih.biBitCount / 8;
        m_imageData.format = "Clipboard";
        m_imageData.pixelFormat = "RGBA8";
        m_imageData.filename = "Clipboard Image";

        BYTE *pPixels = (BYTE *)pBitmapInfo + bmih.biSize +
                        bmih.biClrUsed * sizeof(RGBQUAD);

        size_t pixelCount = m_imageData.width * m_imageData.height * 4;
        m_imageData.pixels.resize(pixelCount);

        // Convert BGR(A) to RGBA float
        int srcStride = ((m_imageData.width * bmih.biBitCount + 31) / 32) * 4;
        bool topDown = bmih.biHeight < 0;

        for (int y = 0; y < m_imageData.height; y++) {
          int srcY = topDown ? y : (m_imageData.height - 1 - y);
          BYTE *srcRow = pPixels + srcY * srcStride;

          for (int x = 0; x < m_imageData.width; x++) {
            int dstIdx = (y * m_imageData.width + x) * 4;
            int srcIdx = x * (bmih.biBitCount / 8);

            m_imageData.pixels[dstIdx + 2] = srcRow[srcIdx + 0] / 255.0f; // B
            m_imageData.pixels[dstIdx + 1] = srcRow[srcIdx + 1] / 255.0f; // G
            m_imageData.pixels[dstIdx + 0] = srcRow[srcIdx + 2] / 255.0f; // R
            m_imageData.pixels[dstIdx + 3] = (bmih.biBitCount == 32)
                                                 ? (srcRow[srcIdx + 3] / 255.0f)
                                                 : 1.0f; // A
          }
        }

        AnalyzeImageRange();
        m_rangeMin = m_imageData.minValue;
        m_rangeMax = m_imageData.maxValue;

        success = true;
      }

      GlobalUnlock(hDIB);
    }
  } else {
    // Try CF_BITMAP (Device Dependent Bitmap) - common for screenshots
    HBITMAP hBitmap = (HBITMAP)GetClipboardData(CF_BITMAP);
    if (hBitmap) {
      BITMAP bm;
      if (GetObject(hBitmap, sizeof(bm), &bm)) {
        m_imageData.width = bm.bmWidth;
        m_imageData.height = bm.bmHeight;
        m_imageData.channels = 4; // We force RGBA
        m_imageData.format = "Clipboard (Bitmap)";
        m_imageData.pixelFormat = "RGBA8";
        m_imageData.filename = "Clipboard Screenshot";

        size_t pixelCount = m_imageData.width * m_imageData.height * 4;
        m_imageData.pixels.resize(pixelCount);

        // Get the bitmap bits
        HDC hDC = GetDC(NULL);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = bm.bmWidth;
        bmi.bmiHeader.biHeight = -bm.bmHeight; // Top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        // Temp buffer for GetDIBits
        std::vector<BYTE> tempBuffer(m_imageData.width * m_imageData.height *
                                     4);

        if (GetDIBits(hDC, hBitmap, 0, m_imageData.height, tempBuffer.data(),
                      &bmi, DIB_RGB_COLORS)) {
          // Convert BGR(A) to RGBA float
          for (int i = 0; i < m_imageData.width * m_imageData.height; i++) {
            int idx = i * 4;
            m_imageData.pixels[idx + 0] = tempBuffer[idx + 2] / 255.0f; // R
            m_imageData.pixels[idx + 1] = tempBuffer[idx + 1] / 255.0f; // G
            m_imageData.pixels[idx + 2] = tempBuffer[idx + 0] / 255.0f; // B
            m_imageData.pixels[idx + 3] =
                1.0f; // Alpha - screenshots usually opaque
          }

          AnalyzeImageRange();
          m_rangeMin = m_imageData.minValue;
          m_rangeMax = m_imageData.maxValue;

          success = true;
        }

        ReleaseDC(NULL, hDC);
      }
    }
  }

  CloseClipboard();
  return success;
}

void ImgViewer::Clear() {
  m_imageData = ImageData();
  m_zoom = 1.0f;
  m_pan = {0.0f, 0.0f};
}
struct my_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

typedef struct my_error_mgr *my_error_ptr;

static void my_error_exit(j_common_ptr cinfo) {
  my_error_ptr myerr = (my_error_ptr)cinfo->err;
  // Let the memory manager delete any temp files before we die
  (*cinfo->err->output_message)(cinfo);
  longjmp(myerr->setjmp_buffer, 1);
}

bool ImgViewer::LoadJpeg(const std::string &filepath) {
  LOG("Loading JPEG: %s", filepath.c_str());
  FILE *infile;

  // Use _wfopen handles Unicode paths correctly on Windows
  std::wstring wpath = Utf8ToWide(filepath);
  if ((infile = _wfopen(wpath.c_str(), L"rb")) == NULL) {
    LOG_ERROR("Failed to open JPEG file: %s", filepath.c_str());
    return false;
  }

  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;

  // We set up the normal JPEG error routines, then override error_exit.
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = my_error_exit;

  // Establish the setjmp return context for my_error_exit to use.
  if (setjmp(jerr.setjmp_buffer)) {
    // If we get here, the JPEG code has signaled an error.
    // We need to clean up the JPEG object, close the input file, and return.
    LOG_ERROR("JPEG error occurred while loading: %s", filepath.c_str());
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    return false;
  }

  jpeg_create_decompress(&cinfo);
  jpeg_stdio_src(&cinfo, infile);

  if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
    LOG_ERROR("JPEG header invalid or not found: %s", filepath.c_str());
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
    return false;
  }

  jpeg_start_decompress(&cinfo);

  m_imageData.width = cinfo.output_width;
  m_imageData.height = cinfo.output_height;
  m_imageData.channels = 4; // We convert to RGBA
  m_imageData.format = "JPEG";
  m_imageData.pixelFormat = "RGBA8";

  size_t pixelCount = m_imageData.width * m_imageData.height * 4;
  m_imageData.pixels.resize(pixelCount);

  int row_stride = cinfo.output_width * cinfo.output_components;
  JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo,
                                                 JPOOL_IMAGE, row_stride, 1);

  while (cinfo.output_scanline < cinfo.output_height) {
    jpeg_read_scanlines(&cinfo, buffer, 1);

    // Convert to float RGBA
    int y = cinfo.output_scanline - 1;
    for (int x = 0; x < m_imageData.width; x++) {
      int dstIdx = (y * m_imageData.width + x) * 4;
      int srcIdx = x * cinfo.output_components;

      if (cinfo.output_components == 3) {
        m_imageData.pixels[dstIdx + 0] = buffer[0][srcIdx + 0] / 255.0f; // R
        m_imageData.pixels[dstIdx + 1] = buffer[0][srcIdx + 1] / 255.0f; // G
        m_imageData.pixels[dstIdx + 2] = buffer[0][srcIdx + 2] / 255.0f; // B
        m_imageData.pixels[dstIdx + 3] = 1.0f;                           // A
      } else if (cinfo.output_components == 1) {
        float val = buffer[0][srcIdx] / 255.0f;
        m_imageData.pixels[dstIdx + 0] = val;  // R
        m_imageData.pixels[dstIdx + 1] = val;  // G
        m_imageData.pixels[dstIdx + 2] = val;  // B
        m_imageData.pixels[dstIdx + 3] = 1.0f; // A
      }
    }
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  fclose(infile);

  LOG("JPEG loaded successfully: %dx%d", m_imageData.width, m_imageData.height);
  return true;
}
