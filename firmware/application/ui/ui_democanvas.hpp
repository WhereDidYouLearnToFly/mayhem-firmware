#ifndef __UI_DEMOCANVAS_H__
#define __UI_DEMOCANVAS_H__

#include "ui.hpp"
#include "ui_widget.hpp"

#include "event_m0.hpp"
#include "message.hpp"

#include <cstdint>
#include <cstddef>

namespace ui {
    namespace democanvas {

        class WaterfallWidget : public Widget {
        public:

            WaterfallWidget(Rect parent_rect);

            void on_show() override;
            void on_hide() override;
            void paint(Painter&) override {}

            void on_channel_spectrum();

        private:
            void clear();
        };

    }
}

#endif /*__UI_DEMOCANVAS_H__*/
