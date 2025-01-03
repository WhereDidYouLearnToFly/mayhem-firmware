#ifndef __LYRA2_PART4_APP_H__
#define __LYRA2_PART4_APP_H__

#include "ui.hpp"
#include "ui_widget.hpp"
#include "ui_democanvas.hpp"
#include "ui_navigation.hpp"
#include "string_format.hpp"
#include "io_wave.hpp"
#include "replay_thread_controlled.hpp"

#include "file.hpp"
#include "sd_card.hpp"

#define READ_CHUNK_MAX_SIZE 512

#define PEAKS_BUFF_SIZE_QUARTER 256
#define PEAKS_BUFF_SIZE_HALF 512
#define PEAKS_BUFF_SIZE (PEAKS_BUFF_SIZE_HALF * 2)

#define ALPHABET_BUFF_SIZE 768
#define ALPHABET_BUFF_SIZE_HALF 384
#define DANCE_BUFF_SIZE 2400
#define DANCE_BUFF_READ_CHUNK_SIZE 400

#define LETTER_SCALE 3

namespace ui
{
    class Lyra2View : public View
    {
    public:

        Lyra2View(NavigationView &nav);
        ~Lyra2View();

        std::string title() const override { 
            return "Lyra2Part4";
        };

    private:

        app_settings::SettingsManager settings_{"lyra2part4", app_settings::Mode::NO_RF};

        NavigationView& nav_;

        void update();
        MessageHandlerRegistration message_handler_update{
            Message::ID::DisplayFrameSync,
            [this](const Message *const) {
                this->update();
            }
        };

        bool check_sd_card();
        bool get_files_from_sd();

        int alphabet_index = -1;
        bool alphabet_is_found = false;
        int peak_index = 0; uint64_t peak_pointer = 0; bool peaks_is_found = false; int peaks_buff_index_ready = 0;
        int dance_frame_wait = 0; uint64_t dance_pointer = 0; bool dance_is_found = false; bool dance_buff_ready = false;
        int dance_offset = 0;

        bool tick_peak();
        bool tick_dance();
        bool tick_message();

        void peaks_buffer_check_and_update();
        void dance_buffer_check_and_update();
        void message_buffer_check_and_update();

        bool read_dance_frame();

        File peaks;
        File dance;
        File alphabet;

        BYTE alphabet_buff[ALPHABET_BUFF_SIZE];
        BYTE peaks_buff[PEAKS_BUFF_SIZE];
        BYTE dance_buff[DANCE_BUFF_SIZE];

        democanvas::WaterfallWidget waterfall{{0,16,240,304}};
        Labels my_debug_label {{{120, 30}, "MyDebugLabel", Color::yellow()},};

        // Message
        std::string font_order = " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
        std::string message = " HELLO WORLD!";
        BYTE letter_data[8];
        char dbg_letter = '?';
        int letter_index = 0;
        int letter_scale_iter = 0;
        int scanline_index = 0;
        bool letter_in_transmit = false;

        // Audio
        void handle_replay_thread_done(const uint32_t return_code);
        void on_playback_progress(const uint32_t progress);
        void set_ready();

        std::unique_ptr<WAVFileReader> wav_reader{};
        std::filesystem::path wav_file_path{};
        std::unique_ptr<ReplayThreadControlled> replay_thread{};

        bool ready_signal{false};
        const size_t read_size{4096};
        const size_t buffer_count{3};
        const uint32_t progress_interval_samples{1536000 / 20};

        void load_wav(std::filesystem::path file_path);
        void play_wav();
        void stop_wav();
        void check_and_read_buffers();

        MessageHandlerRegistration message_handler_replay_thread_error {
        Message::ID::ReplayThreadDone,
        [this](const Message* const p) {
            const auto message = *reinterpret_cast<const ReplayThreadDoneMessage*>(p);
            this->handle_replay_thread_done(message.return_code);
        }};

        MessageHandlerRegistration message_handler_fifo_signal{
        Message::ID::RequestSignal,
        [this](const Message* const p) {
            const auto message = static_cast<const RequestSignalMessage*>(p);
            if (message->signal == RequestSignalMessage::Signal::FillRequest) {
                this->set_ready();
            }
        }};

        MessageHandlerRegistration message_handler_tx_progress{
        Message::ID::TXProgress,
        [this](const Message* const p) {
            const auto message = *reinterpret_cast<const TXProgressMessage*>(p);
            this->on_playback_progress(message.progress);
        }};

    };
} 
#endif