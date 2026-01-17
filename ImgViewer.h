#pragma once
#include "pch.h"
#include <string>
#include <vector>

/**
 * @brief Structure representing loaded image data.
 */
struct ImageData {
  std::vector<float> pixels; ///< RGBA float pixel data (0.0 - 1.0 range)
  int width = 0;             ///< Image width in pixels
  int height = 0;            ///< Image height in pixels
  int channels = 0;          ///< Number of color channels
  std::string filename;      ///< Source filename
  std::string format;        ///< File format (e.g., PNG, HDR, DDS)
  std::string pixelFormat;   ///< Internal pixel format description
  float minValue = 0.0f;     ///< Minimum pixel value found
  float maxValue = 1.0f;     ///< Maximum pixel value found
  bool hasNaN = false;       ///< Flag indicating presence of NaN values
};

/**
 * @brief Main class for handling image loading and state.
 */
class ImgViewer {
public:
  /**
   * @brief Constructor.
   */
  ImgViewer();

  /**
   * @brief Destructor.
   */
  ~ImgViewer();

  /**
   * @brief Loads an image from the specified file path.
   * @param filepath Path to the image file.
   * @return True if loading succeeded, false otherwise.
   */
  bool LoadImage(const std::string &filepath);

  /**
   * @brief Loads an image from the system clipboard.
   * @return True if a valid image was found and loaded, false otherwise.
   */
  bool LoadImageFromClipboard();

  /**
   * @brief Clears the current image data.
   */
  void Clear();

  /**
   * @brief Gets the current image data.
   * @return Reference to ImageData structure.
   */
  const ImageData &GetImageData() const { return m_imageData; }

  /**
   * @brief Checks if an image is currently loaded.
   * @return True if image dimensions are valid.
   */
  bool HasImage() const {
    return m_imageData.width > 0 && m_imageData.height > 0;
  }

  // UI state getters and setters

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

  /**
   * @brief Loads image using stb_image library (LDR and HDR).
   */
  bool LoadSTB(const std::string &filepath);

  /**
   * @brief Loads image using DirectXTex library (DDS).
   */
  bool LoadDDS(const std::string &filepath);

  /**
   * @brief Loads image using libjpeg-turbo (JPG/JPEG).
   */
  bool LoadJpeg(const std::string &filepath);

  /**
   * @brief Analyzes image pixels to find min/max values and NaNs.
   */
  void AnalyzeImageRange();
};
