/*
 * Copyright (C) 2014 Jared Boone, ShareBrained Technology, Inc.
 *
 * This file is part of PortaPack.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "ui_receiver.hpp"

#include "portapack.hpp"
using namespace portapack;

#include "string_format.hpp"

#include "analog_audio_app.hpp"
#include "ais_app.hpp"
#include "tpms_app.hpp"
#include "ert_app.hpp"
#include "spectrum_analysis_app.hpp"

namespace ui {

/* FrequencyField ********************************************************/

FrequencyField::FrequencyField(
	const Point parent_pos
) : Widget { { parent_pos, { 8 * 10, 16 } } },
	length_ { 11 },
	range(rf::tuning_range)
{
	flags.focusable = true;
}

rf::Frequency FrequencyField::value() const {
	return value_;
}

void FrequencyField::set_value(rf::Frequency new_value) {
	new_value = clamp_value(new_value);

	if( new_value != value_ ) {
		value_ = new_value;
		if( on_change ) {
			on_change(value_);
		}
		set_dirty();
	}
}

void FrequencyField::set_step(rf::Frequency new_value) {
	step = new_value;
	// TODO: Quantize current frequency to a step of the new size?
}

void FrequencyField::paint(Painter& painter) {
	const auto mhz = to_string_dec_int(value_ / 1000000, 4);
	const auto hz100 = to_string_dec_int((value_ / 100) % 10000, 4, '0');

	const auto paint_style = has_focus() ? style().invert() : style();

	painter.draw_string(
		screen_pos(),
		paint_style,
		mhz
	);
	painter.draw_string(
		screen_pos() + Point { 4 * 8, 0 },
		paint_style,
		"."
	);
	painter.draw_string(
		screen_pos() + Point { 5 * 8, 0 },
		paint_style,
		hz100
	);
}

bool FrequencyField::on_key(const ui::KeyEvent event) {
	if( event == ui::KeyEvent::Select ) {
		if( on_edit ) {
			on_edit();
			return true;
		}
	}
	return false;
}
/*
bool FrequencyField::on_key(const ui::KeyEvent event) override {
	if( event == ui::KeyEvent::Select ) {

		// NOTE: For testing sampling rate / decimation combinations
		turbo = !turbo;
		if( turbo ) {
			clock_manager.set_sampling_frequency(18432000);
			radio::set_baseband_decimation_by(6);
		} else {
			clock_manager.set_sampling_frequency(12288000);
			radio::set_baseband_decimation_by(4);
		}
		return true;
	}

	return false;
}
*/
bool FrequencyField::on_encoder(const EncoderEvent delta) {
	set_value(value() + (delta * step));
	return true;
}

bool FrequencyField::on_touch(const TouchEvent event) {
	if( event.type == TouchEvent::Type::Start ) {
		focus();
	}
	return true;
}

void FrequencyField::on_focus() {
	if( on_show_options ) {
		on_show_options();
	}
}

rf::Frequency FrequencyField::clamp_value(rf::Frequency value) {
	if( value > range.max ) {
		value = range.max;
	}
	if( value < range.min ) {
		value = range.min;
	}
	return value;
}

/* FrequencyKeypadView ***************************************************/

FrequencyKeypadView::FrequencyKeypadView(
	NavigationView& nav,
	const rf::Frequency value
) {
	add_child(&text_value);

	const auto button_fn = [this](Button& button) {
		this->on_button(button);
	};

	const char* const key_caps = "123456789<0.";

	int n = 0;
	for(auto& button : buttons) {
		add_child(&button);
		const std::string label {
			key_caps[n]
		};
		button.on_select = button_fn;
		button.set_parent_rect({
			(n % 3) * button_w,
			(n / 3) * button_h + button_h,
			button_w, button_h
		});
		button.set_text(label);
		n++;
	}

	add_child(&button_close);
	button_close.on_select = [this, &nav](Button&) {
		if( on_changed ) {
			on_changed(this->value());
		}
		nav.pop();
	};

	set_value(value);
}

void FrequencyKeypadView::focus() {
	button_close.focus();
}

rf::Frequency FrequencyKeypadView::value() const {
	return mhz.as_int() * 1000000ULL + submhz.as_int() * submhz_base;
}

void FrequencyKeypadView::set_value(const rf::Frequency new_value) {
	mhz.set(new_value / 1000000);
	mhz.remove_leading_zeros();

	submhz.set((new_value % 1000000) / submhz_base);
	submhz.remove_trailing_zeros();

	update_text();
}

void FrequencyKeypadView::on_button(Button& button) {
	const auto s = button.text();
	if( s == "." ) {
		field_toggle();
	} else if( s == "<" ) {
		digit_delete();
	} else {
		digit_add(s[0]);
	}
	update_text();
}

void FrequencyKeypadView::digit_add(const char c) {
	if( state == State::DigitMHz ) {
		if( clear_field_if_digits_entered ) {
			mhz.clear();
		}
		mhz.add_digit(c);
	} else {
		submhz.add_digit(c);
	}
	clear_field_if_digits_entered = false;
}

void FrequencyKeypadView::digit_delete() {
	if( state == State::DigitMHz ) {
		mhz.delete_digit();
	} else {
		submhz.delete_digit();
	}
}

void FrequencyKeypadView::field_toggle() {
	if( state == State::DigitMHz ) {
		state = State::DigitSubMHz;
		submhz.clear();
	} else {
		state = State::DigitMHz;
		clear_field_if_digits_entered = true;
	}
}

void FrequencyKeypadView::update_text() {
	const auto s = mhz.as_string() + "." + submhz.as_string();
	text_value.set(s);
}

/* FrequencyOptionsView **************************************************/

FrequencyOptionsView::FrequencyOptionsView(
	const Rect parent_rect,
	const Style* const style
) : View { parent_rect }
{
	set_style(style);

	options_step.on_change = [this](size_t n, OptionsField::value_t v) {
		(void)n;
		this->on_step_changed(v);
	};

	field_ppm.on_change = [this](int32_t v) {
		this->on_reference_ppm_correction_changed(v);
	};

	add_children({ {
		&text_step,
		&options_step,
		&text_correction,
		&field_ppm,
		&text_ppm,
	} });
}

void FrequencyOptionsView::set_step(rf::Frequency f) {
	options_step.set_by_value(f);
}

void FrequencyOptionsView::set_reference_ppm_correction(int32_t v) {
	field_ppm.set_value(v);
}

void FrequencyOptionsView::on_step_changed(rf::Frequency v) {
	if( on_change_step ) {
		on_change_step(v);
	}
}

void FrequencyOptionsView::on_reference_ppm_correction_changed(int32_t v) {
	if( on_change_reference_ppm_correction ) {
		on_change_reference_ppm_correction(v);
	}
}

/* RadioGainOptionsView **************************************************/

RadioGainOptionsView::RadioGainOptionsView(
	const Rect parent_rect,
	const Style* const style
) : View { parent_rect }
{
	set_style(style);

	add_children({ {
		&label_rf_amp,
		&field_rf_amp,
		//&label_agc,
		//&field_agc
	} });

	field_rf_amp.on_change = [this](int32_t v) {
		this->on_rf_amp_changed(v);
	};
	/*
	field_agc.set_value(receiver_model.agc());
	field_agc.on_change = [this](int32_t v) {
		this->on_agc_changed(v);
	};
	*/
}

void RadioGainOptionsView::set_rf_amp(int32_t v_db) {
	field_rf_amp.set_value(v_db);
}

void RadioGainOptionsView::on_rf_amp_changed(bool enable) {
	if( on_change_rf_amp ) {
		on_change_rf_amp(enable);
	}
}
/*
void RadioGainOptionsView::on_agc_changed(bool v) {
	receiver_model.set_agc(v);
}
*/

/* LNAGainField **********************************************************/

LNAGainField::LNAGainField(
	Point parent_pos
) : NumberField {
		parent_pos, 2,
		{ max2837::lna::gain_db_min, max2837::lna::gain_db_max },
		max2837::lna::gain_db_step,
		' ',
	}
{
}

void LNAGainField::on_focus() {
	//Widget::on_focus();
	if( on_show_options ) {
		on_show_options();
	}
}

/* ReceiverView **********************************************************/

ReceiverView::ReceiverView(
	NavigationView& nav
) {
	add_children({ {
		&rssi,
		&channel,
		&audio,
		&field_frequency,
		&field_lna,
		&field_vga,
		&options_modulation,
		&field_volume,
		&view_frequency_options,
		&view_rf_gain_options,
	} });

	field_frequency.set_value(receiver_model.tuning_frequency());
	field_frequency.set_step(receiver_model.frequency_step());
	field_frequency.on_change = [this](rf::Frequency f) {
		this->on_tuning_frequency_changed(f);
	};
	field_frequency.on_edit = [this, &nav]() {
		// TODO: Provide separate modal method/scheme?
		auto new_view = nav.push<FrequencyKeypadView>(receiver_model.tuning_frequency());
		new_view->on_changed = [this](rf::Frequency f) {
			this->on_tuning_frequency_changed(f);
			this->field_frequency.set_value(f);
		};
	};
	field_frequency.on_show_options = [this]() {
		this->on_show_options_frequency();
	};

	field_lna.set_value(receiver_model.lna());
	field_lna.on_change = [this](int32_t v) {
		this->on_lna_changed(v);
	};
	field_lna.on_show_options = [this]() {
		this->on_show_options_rf_gain();
	};

	field_vga.set_value(receiver_model.vga());
	field_vga.on_change = [this](int32_t v_db) {
		this->on_vga_changed(v_db);
	};

	options_modulation.set_by_value(receiver_model.modulation());
	options_modulation.on_change = [this](size_t n, OptionsField::value_t v) {
		(void)n;
		this->on_modulation_changed(static_cast<ReceiverModel::Mode>(v));
	};

	field_volume.set_value((receiver_model.headphone_volume() - wolfson::wm8731::headphone_gain_range.max).decibel() + 99);
	field_volume.on_change = [this](int32_t v) {
		this->on_headphone_volume_changed(v);
	};

	view_frequency_options.hidden(true);
	view_frequency_options.set_step(receiver_model.frequency_step());
	view_frequency_options.on_change_step = [this](rf::Frequency f) {
		this->on_frequency_step_changed(f);
	};
	view_frequency_options.set_reference_ppm_correction(receiver_model.reference_ppm_correction());
	view_frequency_options.on_change_reference_ppm_correction = [this](int32_t v) {
		this->on_reference_ppm_correction_changed(v);
	};

	view_rf_gain_options.hidden(true);
	view_rf_gain_options.set_rf_amp(receiver_model.rf_amp());
	view_rf_gain_options.on_change_rf_amp = [this](bool enable) {
		this->on_rf_amp_changed(enable);
	};

	receiver_model.enable();
}

ReceiverView::~ReceiverView() {

	// TODO: Manipulating audio codec here, and in ui_receiver.cpp. Good to do
	// both?
	audio_codec.headphone_mute();

	receiver_model.disable();
}

void ReceiverView::on_show() {
	View::on_show();
	
	// TODO: Separate concepts of baseband "modulation" and receiver "mode".
	on_modulation_changed(static_cast<ReceiverModel::Mode>(receiver_model.modulation()));
}

void ReceiverView::on_hide() {
	on_modulation_changed(static_cast<ReceiverModel::Mode>(-1));

	View::on_hide();
}

void ReceiverView::focus() {
	field_frequency.focus();
}

void ReceiverView::on_tuning_frequency_changed(rf::Frequency f) {
	receiver_model.set_tuning_frequency(f);
}

void ReceiverView::on_baseband_bandwidth_changed(uint32_t bandwidth_hz) {
	receiver_model.set_baseband_bandwidth(bandwidth_hz);
}

void ReceiverView::on_rf_amp_changed(bool v) {
	receiver_model.set_rf_amp(v);
}

void ReceiverView::on_lna_changed(int32_t v_db) {
	receiver_model.set_lna(v_db);
}

void ReceiverView::on_vga_changed(int32_t v_db) {
	receiver_model.set_vga(v_db);
}

void ReceiverView::on_modulation_changed(ReceiverModel::Mode mode) {
	remove_child(widget_content.get());
	widget_content.reset();
	
	switch(mode) {
	case ReceiverModel::Mode::AMAudio:
	case ReceiverModel::Mode::NarrowbandFMAudio:
	case ReceiverModel::Mode::WidebandFMAudio:
		widget_content = std::make_unique<AnalogAudioView>(mode);
		break;

	case ReceiverModel::Mode::SpectrumAnalysis:
		widget_content = std::make_unique<SpectrumAnalysisView>();
		break;

	default:
		break;
	}
	
	if( widget_content ) {
		add_child(widget_content.get());
		const ui::Dim header_height = 3 * 16;
		const ui::Rect rect { 0, header_height, parent_rect.width(), static_cast<ui::Dim>(parent_rect.height() - header_height) };
		widget_content->set_parent_rect(rect);
	}
}

void ReceiverView::on_show_options_frequency() {
	view_rf_gain_options.hidden(true);
	field_lna.set_style(nullptr);

	view_frequency_options.hidden(false);
	field_frequency.set_style(&view_frequency_options.style());
}

void ReceiverView::on_show_options_rf_gain() {
	view_frequency_options.hidden(true);
	field_frequency.set_style(nullptr);

	view_rf_gain_options.hidden(false);
	field_lna.set_style(&view_frequency_options.style());
}

void ReceiverView::on_frequency_step_changed(rf::Frequency f) {
	receiver_model.set_frequency_step(f);
	field_frequency.set_step(f);
}

void ReceiverView::on_reference_ppm_correction_changed(int32_t v) {
	receiver_model.set_reference_ppm_correction(v);
}

void ReceiverView::on_headphone_volume_changed(int32_t v) {
	const auto new_volume = volume_t::decibel(v - 99) + wolfson::wm8731::headphone_gain_range.max;
	receiver_model.set_headphone_volume(new_volume);
}

} /* namespace ui */
