#include "ui_lyra2demo.hpp"
#include "portapack.hpp"
#include "audio.hpp"
#include "baseband_api.hpp"

#include <cstring>

using namespace portapack;

namespace ui
{

    Lyra2View::Lyra2View(NavigationView &nav) : nav_(nav)
    {
       ::memset(peaks_buff, 0, PEAKS_BUFF_SIZE);
       ::memset(message_buff, 0, MESSAGE_BUFF_SIZE);
       ::memset(dance_buff, 0, DANCE_BUFF_SIZE);

        baseband::run_image(portapack::spi_flash::image_tag_audio_tx);
        wav_reader = std::make_unique<WAVFileReader>();

        add_children({&waterfall, &my_debug_label});
        //add_children({&waterfall});
        get_files_from_sd();

        load_wav(u"LYRA2/lyra2part4_audio.wav");
        //play_wav();
    }

    Lyra2View::~Lyra2View() 
    {
        stop_wav();
        baseband::shutdown();
    }

    void Lyra2View::update()
    {
         waterfall.on_channel_spectrum();
         tick_peak();
         tick_message();
         tick_dance();
         //debug
         //char debug_text[16];
         //snprintf(debug_text, sizeof(debug_text), "%d", dance_offset);
         //my_debug_label.set_labels({ui::Labels::Label{{120, dance_offset}, debug_text, Color::green()}});
         //my_debug_label.set_dirty();
    }

    bool Lyra2View::check_sd_card() 
    {
        return (sd_card::status() == sd_card::Status::Mounted) ? true : false; 
    }
    
    bool Lyra2View::get_files_from_sd()
    {
        auto peaks_success = peaks.open(u"LYRA2/peaks.bin");
        auto dance_success = dance.open(u"LYRA2/dance.bin");
        auto alphabet_success = alphabet.open(u"LYRA2/alphabet.bin");

        peaks_is_found = peaks_success.is_valid() == 0;
        dance_is_found = dance_success.is_valid() == 0;
        alphabet_is_found = alphabet_success.is_valid() == 0;

        peaks_buff_index_ready = 0;

        if (peaks_is_found)
        {
            peaks.seek(peak_pointer);
            peaks.read(peaks_buff, PEAKS_BUFF_SIZE_HALF);
            peak_pointer = PEAKS_BUFF_SIZE_HALF;
            peaks_buff_index_ready = 0;
        }

        return (peaks_is_found && dance_is_found && alphabet_is_found);
    }

    bool Lyra2View::tick_peak()
    {
        if (!peaks_is_found) {return false;}

        std::array<Color, 50> pixel_row;
        BYTE peak_value = (peaks_buff[peak_index]/12);

        for (BYTE i = 0; i < 25; i++) 
        {
            int rvalue = rand() & 0x1f;
            ui::Color used = rvalue > 30 ? ui::Color(0, 100, 255) : ui::Color(0, 0, 255);
            const auto pixel_color = (i < (25 - peak_value)) > 0 ? used : ui::Color(255, 255, 0);
            pixel_row[i] = pixel_color;
        }

        for (BYTE i = 25; i < 50; i++) 
        {
            int rvalue = rand() & 0x1f;
            ui::Color used = rvalue > 28 ? ui::Color(0, 100, 255) : ui::Color(0, 0, 255);
            const auto pixel_color = (i > (25 + peak_value)) ? used : ui::Color(255, 255, 0);
            pixel_row[i] = pixel_color;
        }

        const auto draw_y = display.scroll(0);
        display.draw_pixels({{0, draw_y}, {pixel_row.size(), 1}}, pixel_row);
        peaks_buffer_check_and_update();

        return true;
    }

    bool Lyra2View::tick_dance()
    {
        if (!dance_is_found) {return false;}
        //if (!dance_buff_ready) {return false;}
        const auto draw_y = display.scroll(0);
        dance_offset = draw_y;
        display.draw_bitmap({60, 60}, {120, 160}, dance_buff, ui::Color::yellow(), ui::Color::blue());
        dance_buff_ready = false;
        dance_buffer_check_and_update();
        return true;
    }

    bool Lyra2View::read_dance_frame()
    {
        if (!dance_is_found) {return false;}
        if (dance_pointer > dance.size())
        {
            dance_is_found = false;
        }
        
        for (int i = 0; i < 6; i++) 
        {
            dance.seek(dance_pointer);
            dance.read(dance_buff+(i*DANCE_BUFF_READ_CHUNK_SIZE), DANCE_BUFF_READ_CHUNK_SIZE);
            dance_pointer += DANCE_BUFF_READ_CHUNK_SIZE;
        }

        dance_buff_ready = true;
        return true;
    }

    bool Lyra2View::tick_message()
    {
        return true;
    }

    void Lyra2View::peaks_buffer_check_and_update() 
    {
        if (!peaks_is_found) {return;}
        peak_index = peak_index + 1;
        
        if (peak_index < PEAKS_BUFF_SIZE_HALF && peaks_buff_index_ready == 0){
            int diff = (PEAKS_BUFF_SIZE_HALF - peak_index);
            if (diff < PEAKS_BUFF_SIZE_QUARTER) {
                peaks.seek(peak_pointer);
                peaks.read(peaks_buff + PEAKS_BUFF_SIZE_HALF, PEAKS_BUFF_SIZE_HALF);
                peak_pointer = peak_pointer + PEAKS_BUFF_SIZE_HALF;
                peaks_buff_index_ready = 1;
            }
        } 
        if (peak_index > PEAKS_BUFF_SIZE_HALF && peaks_buff_index_ready == 1){
            int diff = (PEAKS_BUFF_SIZE - peak_index);
            if (diff < PEAKS_BUFF_SIZE_QUARTER) {
                peaks.seek(peak_pointer);
                peaks.read(peaks_buff, PEAKS_BUFF_SIZE_HALF);
                peak_pointer = peak_pointer + PEAKS_BUFF_SIZE_HALF;
                peaks_buff_index_ready = 0;
            }
        }

        if (peak_pointer >= peaks.size()) {
            peaks.close();
            peaks_is_found = false;
        }

        if (peak_index >= PEAKS_BUFF_SIZE) {peak_index = 0;}
    }
    void Lyra2View::dance_buffer_check_and_update()
    {
        if (dance_frame_wait < 2)  {dance_frame_wait = dance_frame_wait + 1; return;}
        dance_frame_wait = 0;
        read_dance_frame();
    }
    void Lyra2View::message_buffer_check_and_update() {}

    void Lyra2View::load_wav(std::filesystem::path file_path) 
    {
        wav_file_path = file_path;
        wav_reader->open(file_path);
        wav_reader->rewind();
    }

    void Lyra2View::handle_replay_thread_done(const uint32_t return_code)
    {
        (void)return_code;
        stop_wav();

        if (return_code == ReplayThread::READ_ERROR)
        {
            nav_.display_modal("Error", "File read error.");
        }

        if (return_code == ReplayThread::END_OF_FILE)
        {
            nav_.display_modal("Error", "End of File.");
        }

        if (return_code == ReplayThread::TERMINATED)
        {
            nav_.display_modal("Error", "Terminated.");
        }

    }

    void Lyra2View::set_ready() 
    {
        ready_signal = true;
    }

    void Lyra2View::on_playback_progress(const uint32_t progress)
    {
        //debug
        char debug_text[16];
        snprintf(debug_text, sizeof(debug_text), "%ld %d %d", progress, (int)((bool)replay_thread), (int)ready_signal);
        my_debug_label.set_labels({ui::Labels::Label{{120, 30}, debug_text, Color::green()}});

    }

    void Lyra2View::play_wav() 
    {
        uint32_t sample_rate;
        uint8_t bits_per_sample;

        auto reader = std::make_unique<WAVFileReader>();
        
        stop_wav();

        if (!reader->open(wav_file_path)) {
            return;
        }

        sample_rate = reader->sample_rate();
        bits_per_sample = reader->bits_per_sample();

        replay_thread = std::make_unique<ReplayThread>(
            std::move(reader),
            read_size, buffer_count,
            &ready_signal,
            [](uint32_t return_code) {
                ReplayThreadDoneMessage message{return_code};
                EventDispatcher::send_message(message);
            });

        char debug_text[16];
        snprintf(debug_text, sizeof(debug_text), "%lu %d", sample_rate, (int)((bool)replay_thread));
        my_debug_label.set_labels({ui::Labels::Label{{120, 30}, debug_text, Color::green()}});

        baseband::set_audiotx_config(
            1536000 / 20,     // Rate of sending progress updates
            0,                // Transmit BW = 0 = not transmitting
            0,                // Gain - unused
            8,                // shift_bits_s16, default 8 bits - unused
            bits_per_sample,  // bits_per_sample
            0,                // tone key disabled
            false,            // AM
            false,            // DSB
            false,            // USB
            false             // LSB
        );
        baseband::set_sample_rate(sample_rate);
        transmitter_model.set_sampling_rate(1536000);

        audio::output::start();

    }

    void Lyra2View::stop_wav()
    {
        bool is_active = (bool)replay_thread;
        if (is_active)
        {
            replay_thread.reset();
        }
        audio::output::stop();
        ready_signal = false;
    }
}