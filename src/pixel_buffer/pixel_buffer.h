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

        enum class Filter
        {
            Nearest,
            Bilinear
        };

        // Copies this buffer's pixels to dst, resampling if sizes differ and converting
        // format if channel layouts differ.
        // filter: selects sampling algorithm — Filter::Nearest (fast, pixelated) or
        //          Filter::Bilinear (smooth, slower). Defaults to Bilinear.
        void copy_pixel_buffer(PixelBuffer* dst, Filter filter) const
        {
            if (dst->m_width == 0 || dst->m_height == 0)
                return;

            const bool needs_scale = (dst->m_width != m_width) || (dst->m_height != m_height);

            if (needs_scale)
            {
                if (filter == Filter::Nearest)
                {
                    for (std::uint64_t dy = 0; dy < dst->m_height; ++dy)
                    {
                        std::uint64_t sy = dy * m_height / dst->m_height;
                        for (std::uint64_t dx = 0; dx < dst->m_width; ++dx)
                        {
                            std::uint64_t sx = dx * m_width / dst->m_width;
                            dst->m_pixels[(dy * dst->m_width + dx) * dst->m_bytes_per_pixel + dst->m_red_offset] =
                                m_pixels[(sy * m_width + sx) * m_bytes_per_pixel + m_red_offset];
                            dst->m_pixels[(dy * dst->m_width + dx) * dst->m_bytes_per_pixel + dst->m_green_offset] =
                                m_pixels[(sy * m_width + sx) * m_bytes_per_pixel + m_green_offset];
                            dst->m_pixels[(dy * dst->m_width + dx) * dst->m_bytes_per_pixel + dst->m_blue_offset] =
                                m_pixels[(sy * m_width + sx) * m_bytes_per_pixel + m_blue_offset];
                            dst->m_pixels[(dy * dst->m_width + dx) * dst->m_bytes_per_pixel + dst->m_alpha_offset] =
                                m_pixels[(sy * m_width + sx) * m_bytes_per_pixel + m_alpha_offset];
                        }
                    }
                }
                else
                {
                    const float inv_w = 1.0f / static_cast<float>(dst->m_width);
                    const float inv_h = 1.0f / static_cast<float>(dst->m_height);

                    for (std::uint64_t dy = 0; dy < dst->m_height; ++dy)
                    {
                        for (std::uint64_t dx = 0; dx < dst->m_width; ++dx)
                        {
                            float u = (static_cast<float>(dx) + 0.5f) * inv_w;
                            float v = (static_cast<float>(dy) + 0.5f) * inv_h;

                            float src_x = u * static_cast<float>(m_width);
                            float src_y = v * static_cast<float>(m_height);

                            src_x = (src_x < 0.0f) ? 0.0f : src_x;
                            src_y = (src_y < 0.0f) ? 0.0f : src_y;
                            src_x = (src_x > static_cast<float>(m_width)) ? static_cast<float>(m_width) - 1.0f : src_x;
                            src_y =
                                (src_y > static_cast<float>(m_height)) ? static_cast<float>(m_height) - 1.0f : src_y;

                            std::uint64_t x0 = static_cast<std::uint64_t>(src_x);
                            std::uint64_t y0 = static_cast<std::uint64_t>(src_y);
                            std::uint64_t x1 = (x0 + 1 < m_width) ? x0 + 1 : x0;
                            std::uint64_t y1 = (y0 + 1 < m_height) ? y0 + 1 : y0;

                            float fx = src_x - static_cast<float>(x0);
                            float fy = src_y - static_cast<float>(y0);

                            std::uint8_t p00[4], p10[4], p01[4], p11[4];
                            for (int c = 0; c < 4; ++c)
                            {
                                p00[c] = m_pixels[(y0 * m_width + x0) * m_bytes_per_pixel + c];
                                p10[c] = m_pixels[(y0 * m_width + x1) * m_bytes_per_pixel + c];
                                p01[c] = m_pixels[(y1 * m_width + x0) * m_bytes_per_pixel + c];
                                p11[c] = m_pixels[(y1 * m_width + x1) * m_bytes_per_pixel + c];
                            }

                            float r = (1.0f - fy) * ((1.0f - fx) * p00[m_red_offset] + fx * p10[m_red_offset]) +
                                fy * ((1.0f - fx) * p01[m_red_offset] + fx * p11[m_red_offset]);
                            float g = (1.0f - fy) * ((1.0f - fx) * p00[m_green_offset] + fx * p10[m_green_offset]) +
                                fy * ((1.0f - fx) * p01[m_green_offset] + fx * p11[m_green_offset]);
                            float b = (1.0f - fy) * ((1.0f - fx) * p00[m_blue_offset] + fx * p10[m_blue_offset]) +
                                fy * ((1.0f - fx) * p01[m_blue_offset] + fx * p11[m_blue_offset]);
                            float a = (1.0f - fy) * ((1.0f - fx) * p00[m_alpha_offset] + fx * p10[m_alpha_offset]) +
                                fy * ((1.0f - fx) * p01[m_alpha_offset] + fx * p11[m_alpha_offset]);

                            dst->m_pixels[(dy * dst->m_width + dx) * dst->m_bytes_per_pixel + dst->m_red_offset] =
                                static_cast<std::uint8_t>(r);
                            dst->m_pixels[(dy * dst->m_width + dx) * dst->m_bytes_per_pixel + dst->m_green_offset] =
                                static_cast<std::uint8_t>(g);
                            dst->m_pixels[(dy * dst->m_width + dx) * dst->m_bytes_per_pixel + dst->m_blue_offset] =
                                static_cast<std::uint8_t>(b);
                            dst->m_pixels[(dy * dst->m_width + dx) * dst->m_bytes_per_pixel + dst->m_alpha_offset] =
                                static_cast<std::uint8_t>(a);
                        }
                    }
                }
            }
            else
            {
                std::uint64_t count = m_width * m_height;
                for (std::uint64_t i = 0; i < count; ++i)
                {
                    dst->m_pixels[i * dst->m_bytes_per_pixel + dst->m_red_offset] =
                        m_pixels[i * m_bytes_per_pixel + m_red_offset];
                    dst->m_pixels[i * dst->m_bytes_per_pixel + dst->m_green_offset] =
                        m_pixels[i * m_bytes_per_pixel + m_green_offset];
                    dst->m_pixels[i * dst->m_bytes_per_pixel + dst->m_blue_offset] =
                        m_pixels[i * m_bytes_per_pixel + m_blue_offset];
                    dst->m_pixels[i * dst->m_bytes_per_pixel + dst->m_alpha_offset] =
                        m_pixels[i * m_bytes_per_pixel + m_alpha_offset];
                }
            }
        }
    };
} // namespace sopho
