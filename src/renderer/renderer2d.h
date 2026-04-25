#pragma once

#include <cstdint>
#include "pixel_buffer/pixel_buffer.h"

namespace sopho
{
    // Renderer2D: 2D drawing primitives operating on a PixelBuffer.
    // Platform-independent — no OS or window system dependencies.
    class Renderer2D
    {
        PixelBuffer* m_target;

    public:
        explicit Renderer2D(PixelBuffer* target) : m_target(target) {}

        // Clears the entire buffer to the specified color.
        void clear(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
        {
            for (std::uint64_t y = 0; y < m_target->get_height(); ++y)
            {
                for (std::uint64_t x = 0; x < m_target->get_width(); ++x)
                {
                    m_target->set_color(x, y, r, g, b, a);
                }
            }
        }

        // Draws a single pixel. Clamps to buffer bounds.
        void draw_pixel(std::uint64_t x, std::uint64_t y, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
        {
            if (x >= m_target->get_width() || y >= m_target->get_height())
                return;
            m_target->set_color(x, y, r, g, b, a);
        }

        // Draws a line from (x0,y0) to (x1,y1) using Bresenham's integer algorithm.
        // Handles all octants. Clamps endpoints to buffer bounds.
        void draw_line(std::int64_t x0, std::int64_t y0, std::int64_t x1, std::int64_t y1,
                       std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
        {
            // Clamp to buffer bounds
            std::int64_t w = static_cast<std::int64_t>(m_target->get_width());
            std::int64_t h = static_cast<std::int64_t>(m_target->get_height());
            if (w <= 0 || h <= 0)
                return;

            if (x0 < 0) x0 = 0;
            if (y0 < 0) y0 = 0;
            if (x1 < 0) x1 = 0;
            if (y1 < 0) y1 = 0;
            if (x0 >= w) x0 = w - 1;
            if (y0 >= h) y0 = h - 1;
            if (x1 >= w) x1 = w - 1;
            if (y1 >= h) y1 = h - 1;

            std::int64_t dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
            std::int64_t dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);
            std::int64_t sx = (x0 < x1) ? 1 : -1;
            std::int64_t sy = (y0 < y1) ? 1 : -1;
            std::int64_t err = dx - dy;

            while (true)
            {
                m_target->set_color(static_cast<std::uint64_t>(x0), static_cast<std::uint64_t>(y0), r, g, b, a);

                if (x0 == x1 && y0 == y1)
                    break;

                std::int64_t e2 = 2 * err;
                if (e2 > -dy)
                {
                    err -= dy;
                    x0 += sx;
                }
                if (e2 < dx)
                {
                    err += dx;
                    y0 += sy;
                }
            }
        }

        // Fills an axis-aligned rectangle at (x,y) with size (w,h).
        // Clamps to buffer bounds.
        void fill_rect(std::int64_t x, std::int64_t y, std::uint64_t w, std::uint64_t h,
                       std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
        {
            std::int64_t bw = static_cast<std::int64_t>(m_target->get_width());
            std::int64_t bh = static_cast<std::int64_t>(m_target->get_height());

            // Clamp rect to buffer
            if (x >= bw || y >= bh)
                return;
            if (x < 0) x = 0;
            if (y < 0) y = 0;

            std::uint64_t x0 = static_cast<std::uint64_t>(x);
            std::uint64_t y0 = static_cast<std::uint64_t>(y);
            std::uint64_t x1 = x0 + w;
            std::uint64_t y1 = y0 + h;
            if (x1 > static_cast<std::uint64_t>(bw))
                x1 = static_cast<std::uint64_t>(bw);
            if (y1 > static_cast<std::uint64_t>(bh))
                y1 = static_cast<std::uint64_t>(bh);

            for (std::uint64_t py = y0; py < y1; ++py)
            {
                for (std::uint64_t px = x0; px < x1; ++px)
                {
                    m_target->set_color(px, py, r, g, b, a);
                }
            }
        }
    };
} // namespace sopho
