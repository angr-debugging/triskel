// A simple SVG renderer

#include "triskel/triskel.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>

#include <fmt/base.h>
#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/format.h>

#include "triskel/utils/point.hpp"

/// NOLINTNEXTLINE(google-build-using-namespace)
using namespace triskel;

namespace {

constexpr size_t FONT_SIZE = 12;

struct SVGRenderer : public ExportingRenderer {
    fmt::memory_buffer buf;

    SVGRenderer() = default;

    void begin(float width, float height) override {
        fmt::format_to(
            std::back_inserter(buf),
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"  //
            "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" "
            "width=\"{}\" height=\"{}\" viewBox=\"0 0 {} {}\">\n",
            width, height, width, height);

        fmt::format_to(std::back_inserter(buf),
                       "<style>\n"                      //
                       "\ttext {{\n"                    //
                       "\t\tfont-family: monospace;\n"  //
                       "\t\tfont-size:{}px;\n"          //
                       "\t\tfill: black;\n"             //
                       "\t}}\n"                         //
                       "</style>",
                       FONT_SIZE);
    }

    void end() override { fmt::format_to(std::back_inserter(buf), "</svg>\n"); }

    void draw_line(Point start, Point end, const StrokeStyle& style) override {
        fmt::format_to(std::back_inserter(buf),
                       "<line x1=\"{}\" y1=\"{}\" x2=\"{}\" y2=\"{}\" "
                       "stroke=\"#{:08X}\" stroke-width=\"{}\" />\n",
                       start.x, start.y, end.x, end.y, style.color.to_hex(),
                       style.thickness);
    }

    void draw_triangle(Point v1, Point v2, Point v3, Color fill) override {
        fmt::format_to(
            std::back_inserter(buf),
            "<polygon points=\"{},{} {},{} {},{}\" fill=\"#{:08X}\" />\n", v1.x,
            v1.y, v2.x, v2.y, v3.x, v3.y, fill.to_hex());
    }

    void draw_rectangle(Point tl,
                        float width,
                        float height,
                        Color fill) override {
        fmt::format_to(std::back_inserter(buf),
                       "<rect x=\"{}\" y=\"{}\" width=\"{}\" height=\"{}\""
                       "fill=\"#{:08X}\" />\n",
                       tl.x, tl.y, width, height, fill.to_hex());
    };

    void draw_rectangle_border(Point tl,
                               float width,
                               float height,
                               const StrokeStyle& style) override {
        fmt::format_to(
            std::back_inserter(buf),
            "<rect x=\"{}\" y=\"{}\" width=\"{}\" height=\"{}\""
            "fill=\"none\" stroke=\"#{:08X}\" stroke-width=\"{}\" />\n",
            tl.x, tl.y, width, height, style.color.to_hex(), style.thickness);
    };

    void draw_text(Point tl,
                   const std::string& text,
                   const TextStyle& /*style*/) override {
        fmt::format_to(std::back_inserter(buf),
                       "<text x=\"{}\" y=\"{}\">{}</text>\n", tl.x, tl.y, text);
    };

    [[nodiscard]] auto measure_text(const std::string& text,
                                    const TextStyle& /*style*/) const
        -> Point override {
        size_t max_len = 1;
        size_t lines   = 1;

        size_t len = 0;
        for (const auto c : text) {
            if (c == '\n') {
                max_len = std::max(max_len, len);
                len     = 0;
                ++lines;
            } else {
                ++len;
            }
        }
        max_len = std::max(max_len, len);

        const auto width  = static_cast<float>(max_len * FONT_SIZE) * 0.6F;
        const auto height = static_cast<float>(FONT_SIZE * lines);

        return {.x = width + (2 * BLOCK_PADDING),
                .y = height + (2 * BLOCK_PADDING)};
    }

    void save(const std::filesystem::path& path) override {
        const auto str = fmt::to_string(buf);

        std::ofstream file{path};

        if (!file) {
            fmt::print("Could not open file \"{}\"\n", path.string());
            throw std::invalid_argument("Could not opent path");
        }

        file << str;
        file.close();
    };
};

}  // namespace

auto triskel::make_fast_svg_renderer() -> std::unique_ptr<ExportingRenderer> {
    return std::make_unique<SVGRenderer>();
}
