#pragma once
#include "pch.h"
#include <string>
#include <vector>

struct ImageData {
  std::vector<float> pixels; // RGBA float format for consistency
  int width = 0;
  int height = 0;
  int channels = 0;
  std::string filename;
  std::string format;      // File format (PNG, JPEG, DDS, etc.)
  std::string pixelFormat; // Internal format (RGBA8, RGB32F, etc.)
  float minValue = 0.0f;
  float maxValue = 1.0f;
  bool hasNaN = false;
};

class ImgViewer {
public:
  ImgViewer();
  ~ImgViewer();

  bool LoadImage(const std::string &filepath);
  bool LoadImageFromClipboard();
  void Clear();

  const ImageData &GetImageData() const { return m_imageData; }
  bool HasImage() const {
    return m_imageData.width > 0 && m_imageData.height > 0;
  }

  // UI state
  float GetZoom() const { return m_zoom; }
  void SetZoom(float zoom) { m_zoom = zoom; }

  const DirectX::XMFLOAT2 &GetPan() const { return m_pan; }
  void SetPan(const DirectX::XMFLOAT2 &pan) { m_pan = pan; }

  float GetRangeMin() const { return m_rangeMin; }
  float GetRangeMax() const { return m_rangeMax; }
  void SetRange(float min, float max) {
    m_rangeMin = min;
    m_rangeMax = max;
  }

private:
  ImageData m_imageData;

  // View state
  float m_zoom = 1.0f;
  DirectX::XMFLOAT2 m_pan = {0.0f, 0.0f};

  // Color mapping range
  float m_rangeMin = 0.0f;
  float m_rangeMax = 1.0f;

  bool LoadSTB(const std::string &filepath);
  bool LoadDDS(const std::string &filepath);
  void AnalyzeImageRange();
};
