#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace sopho
{
    // PixelBuffer: a generic pixel array with format-aware channel offsets.
    // Supports any 4-channel format (e.g. "ARGB", "BGRA") by parsing the format string.
    class PixelBuffer
    {
        std::uint64_t m_width{};
        std::uint64_t m_height{};
        // Channel offsets into each pixel byte array (index by channel letter)
        std::uint8_t m_red_offset{};
        std::uint8_t m_blue_offset{};
        std::uint8_t m_green_offset{};
        std::uint8_t m_alpha_offset{};
        std::uint8_t m_bytes_per_pixel{};
        std::vector<std::uint8_t> m_pixels{}; // flat pixel data: [y * width + x] * bytes_per_pixel

    public:
        // Construct a pixel buffer.
        // width, height: dimensions in pixels.
        // format: 4-character string specifying channel order, e.g. "ARGB" or "BGRA".
        //          Each character sets the corresponding channel offset.
        // bytes_per_pixel: bytes per pixel (must match format length, typically 4).
        PixelBuffer(std::uint64_t width, std::uint64_t height, std::string format, std::uint8_t bytes_per_pixel) :
            m_width(width), m_height(height), m_bytes_per_pixel(bytes_per_pixel),
            m_pixels(width * height * bytes_per_pixel)
        {
            assert(format.size() == 4);
            for (size_t i = 0; i < format.size(); ++i)
            {
                switch (format[i])
                {
                case 'R':
                    m_red_offset = static_cast<std::uint8_t>(i);
                    break;
                case 'G':
                    m_green_offset = static_cast<std::uint8_t>(i);
                    break;
                case 'B':
                    m_blue_offset = static_cast<std::uint8_t>(i);
                    break;
                case 'A':
                    m_alpha_offset = static_cast<std::uint8_t>(i);
                    break;
                default:
                    break;
                }
            }
        }

        // Returns raw pointer to pixel data.
        auto get_pixels() const { return m_pixels.data(); }
        // Returns width in pixels.
        auto get_width() const { return m_width; }
        // Returns height in pixels.
        auto get_height() const { return m_height; }
        // Sets a single pixel's RGBA channels using the configured format offsets.
        auto set_color(std::uint64_t x, std::uint64_t y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
        {
            assert(x < m_width && y < m_height);
            std::uint8_t* pixel = &m_pixels[(y * m_width + x) * m_bytes_per_pixel];
            pixel[m_red_offset] = r;
            pixel[m_green_offset] = g;
            pixel[m_blue_offset] = b;
            pixel[m_alpha_offset] = a;
        }
    };
} // namespace sopho
