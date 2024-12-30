
#include "ui_democanvas.hpp"
#include "spectrum_color_lut.hpp"
#include "portapack.hpp"

using namespace portapack;

#include "baseband_api.hpp"
#include "string_format.hpp"

#include <cmath>
#include <array>

namespace ui {
    namespace democanvas {

        WaterfallWidget::WaterfallWidget(Rect parent_rect) : Widget{parent_rect}
        {
            set_focusable(false);
        }

        void WaterfallWidget::on_show() {
            clear();
            display.scroll_set_area(16, 320);
        }

        void WaterfallWidget::on_hide() {
            display.scroll_disable();
        }

        void WaterfallWidget::on_channel_spectrum() {
            std::array<Color, 240> pixel_row;
            for (size_t i = 0; i < 240; i++) {
                int rvalue = rand() & 0x1f;
                ui::Color used = rvalue > 30 ? ui::Color(0, 200, 255) : ui::Color(0, 0, 255);
                if ( i > 50 && i < 190) {used = ui::Color::blue();}
                const auto pixel_color = used;
                pixel_row[i] = pixel_color;
            }

            const auto draw_y = display.scroll(1);
            display.draw_pixels( {{0, draw_y}, {pixel_row.size(), 1}}, pixel_row);
        }

        void WaterfallWidget::clear() {
            display.fill_rectangle({0,16,240,304}, ui::Color::blue());
        }
    }
}
