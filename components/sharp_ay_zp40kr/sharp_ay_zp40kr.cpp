#include "sharp_ay_zp40kr.h"

#include <cmath>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::sharp_ay_zp40kr {

static const char *const TAG = "sharp_ay_zp40kr";

namespace {

SharpHvacMode climate_mode_to_sharp(climate::ClimateMode mode) {
  switch (mode) {
    case climate::CLIMATE_MODE_HEAT:
      return SharpHvacMode::HEAT;
    case climate::CLIMATE_MODE_DRY:
      return SharpHvacMode::DRY;
    case climate::CLIMATE_MODE_COOL:
    default:
      return SharpHvacMode::COOL;
  }
}

climate::ClimateMode sharp_mode_to_climate(SharpHvacMode mode) {
  switch (mode) {
    case SharpHvacMode::HEAT:
      return climate::CLIMATE_MODE_HEAT;
    case SharpHvacMode::DRY:
      return climate::CLIMATE_MODE_DRY;
    case SharpHvacMode::COOL:
    default:
      return climate::CLIMATE_MODE_COOL;
  }
}

SharpFanMode climate_fan_to_sharp(climate::ClimateFanMode fan) {
  switch (fan) {
    case climate::CLIMATE_FAN_LOW:
      return SharpFanMode::LEVEL_1;
    case climate::CLIMATE_FAN_MEDIUM:
      return SharpFanMode::LEVEL_2;
    case climate::CLIMATE_FAN_HIGH:
      return SharpFanMode::LEVEL_3;
    case climate::CLIMATE_FAN_AUTO:
    default:
      return SharpFanMode::AUTO;
  }
}

climate::ClimateFanMode sharp_fan_to_climate(SharpFanMode fan) {
  switch (fan) {
    case SharpFanMode::LEVEL_1:
      return climate::CLIMATE_FAN_LOW;
    case SharpFanMode::LEVEL_2:
      return climate::CLIMATE_FAN_MEDIUM;
    case SharpFanMode::LEVEL_3:
      return climate::CLIMATE_FAN_HIGH;
    case SharpFanMode::AUTO:
    default:
      return climate::CLIMATE_FAN_AUTO;
  }
}

}  // namespace

void SharpAyZp40krClimate::setup() {
  climate_ir::ClimateIR::setup();
  this->shadow_preference_ = this->make_entity_preference<RestoreData>(0xA7880001U);
  this->restore_shadow_();
  this->sync_shadow_from_climate_();
  this->publish_from_shadow_();
  if (this->power_sensor_ != nullptr) {
    this->power_sensor_->add_on_state_callback([this](float state) {
      this->last_power_w_ = state;
      this->update_action_(state);
    });
    if (this->power_sensor_->has_state()) {
      this->last_power_w_ = this->power_sensor_->state;
      this->update_action_(this->last_power_w_);
    }
  }
}

void SharpAyZp40krClimate::update_action_(float power_w) {
  // The A788 sends no status packets, so the compressor state is inferred from
  // an external power measurement (e.g. the plug the AC is wired through):
  // above the active threshold the unit is compressing in its current mode,
  // below it the indoor fan is running idle (or the unit is off).
  climate::ClimateDeviceAction action;
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    action = climate::CLIMATE_DEVICE_ACTION_OFF;
  } else if (power_w > this->active_threshold_) {
    switch (this->mode) {
      case climate::CLIMATE_MODE_HEAT:
        action = climate::CLIMATE_DEVICE_ACTION_HEATING;
        break;
      case climate::CLIMATE_MODE_COOL:
        action = climate::CLIMATE_DEVICE_ACTION_COOLING;
        break;
      case climate::CLIMATE_MODE_DRY:
        action = climate::CLIMATE_DEVICE_ACTION_DRYING;
        break;
      default:
        action = climate::CLIMATE_DEVICE_ACTION_FAN;
        break;
    }
  } else {
    action = climate::CLIMATE_DEVICE_ACTION_IDLE;
  }
  if (action != this->current_action_) {
    ESP_LOGD(TAG, "Action %s -> %s (power %.1f W, threshold %.1f W)",
             climate::climate_device_action_str(this->current_action_),
             climate::climate_device_action_str(action), power_w, this->active_threshold_);
    this->set_current_action(action);
    this->publish_state();
  }
}

void SharpAyZp40krClimate::dump_config() {
  climate_ir::ClimateIR::dump_config();
  ESP_LOGCONFIG(TAG, "  Model: Sharp AY-ZP40KR / CRMC-A788JBEZ");
  ESP_LOGCONFIG(TAG, "  Receiver suppression: %u ms", this->receive_suppression_ms_);
  ESP_LOGCONFIG(TAG, "  Packet clock: %s", this->time_ == nullptr ? "00:00 fallback" : "configured time source");
}

climate::ClimateTraits SharpAyZp40krClimate::traits() {
  auto traits = climate_ir::ClimateIR::traits();
  traits.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_HEAT,
                              climate::CLIMATE_MODE_DRY});
  return traits;
}

void SharpAyZp40krClimate::sync_shadow_from_climate_() {
  if (this->mode != climate::CLIMATE_MODE_OFF) {
    this->shadow_.power = true;
    this->shadow_.ion_only = false;
    this->shadow_.mode = climate_mode_to_sharp(this->mode);
  }
  this->shadow_.temperature_c = static_cast<uint8_t>(std::lround(clamp(this->target_temperature, 18.0f, 32.0f)));
  this->shadow_.fan = climate_fan_to_sharp(this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO));

  if (this->preset.has_value()) {
    this->shadow_.powerful = this->preset == climate::CLIMATE_PRESET_BOOST;
    this->shadow_.eco = this->preset == climate::CLIMATE_PRESET_ECO;
    this->shadow_.vacancy = this->preset == climate::CLIMATE_PRESET_AWAY;
  }
  this->remember_temperature_();
}

void SharpAyZp40krClimate::remember_temperature_() {
  if (this->shadow_.vacancy || this->shadow_.mode == SharpHvacMode::DRY)
    return;
  if (this->shadow_.mode == SharpHvacMode::HEAT)
    this->heat_temperature_ = this->shadow_.temperature_c;
  else if (this->shadow_.mode == SharpHvacMode::COOL)
    this->cool_temperature_ = this->shadow_.temperature_c;
}

void SharpAyZp40krClimate::restore_shadow_() {
  RestoreData data;
  if (!this->shadow_preference_.load(&data) || data.marker != 0xA8 || data.mode < 1 || data.mode > 3 ||
      data.temperature < 18 || data.temperature > 32 || data.heat_temperature < 18 ||
      data.heat_temperature > 32 || data.cool_temperature < 18 || data.cool_temperature > 32 || data.spot > 6 ||
      (data.fan != 2 && data.fan != 3 && data.fan != 5 && data.fan != 7)) {
    return;
  }
  this->shadow_.mode = static_cast<SharpHvacMode>(data.mode);
  this->shadow_.temperature_c = data.temperature;
  this->shadow_.fan = static_cast<SharpFanMode>(data.fan);
  this->shadow_.ion = (data.flags & 0x01) != 0;
  this->shadow_.coanda = (data.flags & 0x02) != 0;
  this->shadow_.spot = static_cast<SharpSpotPosition>(data.spot);
  this->shadow_.ion_only = false;
  this->heat_temperature_ = data.heat_temperature;
  this->cool_temperature_ = data.cool_temperature;
  ESP_LOGD(TAG, "Restored A788 shadow memory (assumed until RX or TX)");
}

void SharpAyZp40krClimate::save_shadow_() {
  RestoreData data;
  data.mode = static_cast<uint8_t>(this->shadow_.mode);
  data.temperature = this->shadow_.temperature_c;
  data.fan = static_cast<uint8_t>(this->shadow_.fan);
  data.heat_temperature = static_cast<uint8_t>(this->heat_temperature_);
  data.cool_temperature = static_cast<uint8_t>(this->cool_temperature_);
  data.spot = static_cast<uint8_t>(this->shadow_.spot);
  data.flags = (this->shadow_.ion ? 0x01 : 0x00) | (this->shadow_.coanda ? 0x02 : 0x00);
  if (!this->shadow_preference_.save(&data))
    ESP_LOGW(TAG, "Failed to save A788 shadow memory");
}

void SharpAyZp40krClimate::control(const climate::ClimateCall &call) {
  const SharpA788State original = this->shadow_;
  const auto original_mode = this->mode;
  SharpCommandKind command = SharpCommandKind::UNKNOWN;

  auto requested_mode = call.get_mode();
  auto requested_temperature = call.get_target_temperature();
  auto requested_fan = call.get_fan_mode();
  auto requested_preset = call.get_preset();

  if (requested_mode.has_value()) {
    this->remember_temperature_();
    if (*requested_mode == climate::CLIMATE_MODE_OFF) {
      this->shadow_.power = false;
      this->shadow_.ion_only = false;
      command = SharpCommandKind::POWER;
    } else if (*requested_mode == climate::CLIMATE_MODE_COOL || *requested_mode == climate::CLIMATE_MODE_HEAT ||
               *requested_mode == climate::CLIMATE_MODE_DRY) {
      const auto new_mode = climate_mode_to_sharp(*requested_mode);
      const bool was_powered = this->shadow_.power;
      const bool mode_changed = !was_powered || new_mode != this->shadow_.mode;
      const bool powered_mode_transition = was_powered && new_mode != this->shadow_.mode;
      this->shadow_.mode = new_mode;
      this->shadow_.power = true;
      this->shadow_.ion_only = false;
      if (!requested_temperature.has_value()) {
        if (new_mode == SharpHvacMode::HEAT)
          this->shadow_.temperature_c = static_cast<uint8_t>(this->heat_temperature_);
        else if (new_mode == SharpHvacMode::COOL)
          this->shadow_.temperature_c = static_cast<uint8_t>(this->cool_temperature_);
      }
      if (mode_changed) {
        this->shadow_.powerful = false;
        this->shadow_.vacancy = false;
      }
      command = powered_mode_transition ? SharpCommandKind::MODE : SharpCommandKind::POWER;
    } else {
      ESP_LOGW(TAG, "Unsupported climate mode requested");
      this->publish_from_shadow_();
      return;
    }
  }

  if (requested_temperature.has_value()) {
    if (original.powerful && !requested_mode.has_value()) {
      ESP_LOGW(TAG, "Temperature is ignored while Powerful is active, matching the A788 remote");
    } else {
      this->shadow_.temperature_c =
          static_cast<uint8_t>(std::lround(clamp(*requested_temperature, 18.0f, 32.0f)));
      this->shadow_.vacancy = false;
      this->remember_temperature_();
      if (command == SharpCommandKind::UNKNOWN && this->shadow_.power)
        command = SharpCommandKind::TEMPERATURE;
    }
  }

  if (requested_fan.has_value()) {
    if (original.powerful && !requested_mode.has_value()) {
      ESP_LOGW(TAG, "Fan changes are ignored while Powerful is active, matching the A788 remote");
    } else {
      this->shadow_.fan = climate_fan_to_sharp(*requested_fan);
      if (command == SharpCommandKind::UNKNOWN && this->shadow_.power)
        command = SharpCommandKind::FAN;
    }
  }

  if (requested_preset.has_value()) {
    if (*requested_preset != climate::CLIMATE_PRESET_NONE && !this->shadow_.power) {
      ESP_LOGW(TAG, "Preset requires normal HVAC operation; no unverified off-state packet was sent");
      this->shadow_ = original;
      this->publish_from_shadow_();
      return;
    }
    switch (*requested_preset) {
      case climate::CLIMATE_PRESET_BOOST:
        this->shadow_.powerful = true;
        this->shadow_.eco = false;
        this->shadow_.spot = SharpSpotPosition::OFF;
        this->shadow_.fan = SharpFanMode::AUTO;
        command = SharpCommandKind::POWERFUL;
        break;
      case climate::CLIMATE_PRESET_ECO:
        this->shadow_.eco = true;
        this->shadow_.powerful = false;
        command = SharpCommandKind::ECO;
        break;
      case climate::CLIMATE_PRESET_AWAY:
        this->shadow_.vacancy = true;
        this->shadow_.powerful = false;
        this->shadow_.mode = SharpHvacMode::HEAT;
        this->shadow_.temperature_c = 18;
        command = SharpCommandKind::VACANCY;
        break;
      case climate::CLIMATE_PRESET_NONE:
        if (original.powerful) {
          this->shadow_.powerful = false;
          command = SharpCommandKind::POWERFUL;
        } else if (original.eco) {
          this->shadow_.eco = false;
          command = SharpCommandKind::ECO;
        } else if (original.vacancy) {
          this->shadow_.vacancy = false;
          this->shadow_.mode = SharpHvacMode::HEAT;
          this->shadow_.temperature_c = 18;
          command = SharpCommandKind::VACANCY;
        }
        break;
      default:
        ESP_LOGW(TAG, "Unsupported preset requested");
        this->shadow_ = original;
        this->publish_from_shadow_();
        return;
    }
  }

  if (command != SharpCommandKind::UNKNOWN && !this->send_command_(command)) {
    this->shadow_ = original;
  } else if (command == SharpCommandKind::UNKNOWN && original_mode == climate::CLIMATE_MODE_OFF) {
    // Temperature and fan may be prepared while off without emitting an
    // unobserved packet. They are included in the next complete Start packet.
    ESP_LOGD(TAG, "Updated remembered state while off; no IR packet sent");
    this->save_shadow_();
  }
  this->publish_from_shadow_();
}

void SharpAyZp40krClimate::transmit_state() {
  this->sync_shadow_from_climate_();
  this->send_command_(SharpCommandKind::POWER);
}

SharpClockTime SharpAyZp40krClimate::clock_now_() const {
  if (this->time_ != nullptr) {
    auto now = this->time_->now();
    if (now.is_valid())
      return {static_cast<uint8_t>(now.hour), static_cast<uint8_t>(now.minute)};
  }
  return {};
}

bool SharpAyZp40krClimate::send_command_(SharpCommandKind command) {
  const auto encoded = sharp_a788_encode(this->shadow_, command, this->clock_now_());
  if (!encoded.valid) {
    ESP_LOGE(TAG, "Refusing to transmit %s: encoder error %u", sharp_a788_command_to_string(command),
             static_cast<unsigned>(encoded.error));
    return false;
  }
  if (!sharp_a788_valid_checksum(encoded.packet)) {
    ESP_LOGE(TAG, "Refusing to transmit packet with invalid checksum");
    return false;
  }

  this->log_packet_("TX", encoded.packet);
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->reserve(2 + SHARP_A788_PACKET_SIZE * 16 + 1);
  data->set_carrier_frequency(SHARP_A788_CARRIER_HZ);
  data->mark(SHARP_A788_HEADER_MARK_US);
  data->space(SHARP_A788_HEADER_SPACE_US);
  for (uint8_t byte : encoded.packet) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      data->mark(SHARP_A788_BIT_MARK_US);
      data->space((byte & (1U << bit)) != 0 ? SHARP_A788_ONE_SPACE_US : SHARP_A788_ZERO_SPACE_US);
    }
  }
  data->mark(SHARP_A788_BIT_MARK_US);
  this->last_transmit_ms_ = millis();
  transmit.perform();
  this->save_shadow_();
  return true;
}

bool SharpAyZp40krClimate::on_receive(remote_base::RemoteReceiveData data) {
  if (this->last_transmit_ms_ != 0 && millis() - this->last_transmit_ms_ < this->receive_suppression_ms_) {
    ESP_LOGV(TAG, "Ignoring IR during local-transmit suppression window");
    return false;
  }
  if (data.size() < static_cast<int32_t>(2 + SHARP_A788_PACKET_SIZE * 16))
    return false;

  data.set_tolerance(300, remote_base::TOLERANCE_MODE_TIME);
  if (!data.expect_item(SHARP_A788_HEADER_MARK_US, SHARP_A788_HEADER_SPACE_US))
    return false;

  data.set_tolerance(250, remote_base::TOLERANCE_MODE_TIME);
  SharpA788Packet packet{};
  for (size_t byte = 0; byte < packet.size(); byte++) {
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(SHARP_A788_BIT_MARK_US, SHARP_A788_ONE_SPACE_US)) {
        packet[byte] |= static_cast<uint8_t>(1U << bit);
      } else if (!data.expect_item(SHARP_A788_BIT_MARK_US, SHARP_A788_ZERO_SPACE_US)) {
        ESP_LOGV(TAG, "RX timing mismatch at byte %u bit %u (raw count=%d)", static_cast<unsigned>(byte), bit,
                 data.size());
        return false;
      }
    }
  }
  if (data.is_valid() && !data.expect_mark(SHARP_A788_BIT_MARK_US))
    ESP_LOGV(TAG, "RX footer mark not matched; packet body will still be validated");

  this->log_packet_("RX", packet);
  auto decoded = sharp_a788_decode(packet, this->shadow_);
  if (!decoded.valid) {
    ESP_LOGW(TAG, "Rejected Sharp packet: header=%s checksum=%s", YESNO(decoded.header_valid),
             YESNO(decoded.checksum_valid));
    return false;
  }
  if (decoded.has_unknown_fields) {
    ESP_LOGW(TAG, "Valid Sharp packet contains unknown fields; preserving current published state");
    return true;
  }

  this->shadow_ = decoded.state;
  ESP_LOGD(TAG, "Decoded command=%s mode=%u temp=%u fan=%u ion=%s powerful=%s eco=%s vacancy=%s coanda=%s "
                "spot=%s",
           sharp_a788_command_to_string(decoded.command), static_cast<unsigned>(this->shadow_.mode),
           this->shadow_.temperature_c, static_cast<unsigned>(this->shadow_.fan), YESNO(this->shadow_.ion),
           YESNO(this->shadow_.powerful), YESNO(this->shadow_.eco), YESNO(this->shadow_.vacancy),
           YESNO(this->shadow_.coanda), sharp_a788_spot_to_string(this->shadow_.spot));
  this->remember_temperature_();
  this->save_shadow_();
  this->publish_from_shadow_();
  return true;
}

void SharpAyZp40krClimate::publish_from_shadow_() {
  this->mode = this->shadow_.power ? sharp_mode_to_climate(this->shadow_.mode) : climate::CLIMATE_MODE_OFF;
  this->target_temperature = this->shadow_.temperature_c;
  this->fan_mode = sharp_fan_to_climate(this->shadow_.fan);
  if (this->shadow_.powerful)
    this->preset = climate::CLIMATE_PRESET_BOOST;
  else if (this->shadow_.eco)
    this->preset = climate::CLIMATE_PRESET_ECO;
  else if (this->shadow_.vacancy)
    this->preset = climate::CLIMATE_PRESET_AWAY;
  else
    this->preset = climate::CLIMATE_PRESET_NONE;
  if (this->power_sensor_ != nullptr)
    this->update_action_(this->last_power_w_);
  this->publish_state();
  this->publish_children_();
}

void SharpAyZp40krClimate::publish_children_() {
  if (this->ion_switch_ != nullptr)
    this->ion_switch_->publish_state(this->shadow_.ion && (this->shadow_.power || this->shadow_.ion_only));
  if (this->coanda_switch_ != nullptr)
    this->coanda_switch_->publish_state(this->shadow_.coanda);
  if (this->spot_air_select_ != nullptr)
    this->spot_air_select_->publish_state(sharp_a788_spot_to_string(this->shadow_.spot));
}

void SharpAyZp40krClimate::set_ion(bool enabled) {
  const auto original = this->shadow_;
  const bool active = this->shadow_.ion && (this->shadow_.power || this->shadow_.ion_only);
  if (enabled == active) {
    this->publish_children_();
    return;
  }
  if (this->shadow_.power) {
    this->shadow_.ion = enabled;
  } else if (enabled) {
    this->shadow_.ion = true;
    this->shadow_.ion_only = true;
  } else if (this->shadow_.ion_only) {
    // The captured Stop packet keeps ION memory (F4), but stops ION-only
    // operation. The switch reports active operation, not dormant memory.
    this->shadow_.ion_only = false;
    this->shadow_.ion = true;
  } else {
    ESP_LOGW(TAG, "No captured packet clears dormant ION memory while off; no IR packet sent");
    this->publish_children_();
    return;
  }
  if (!this->send_command_(SharpCommandKind::ION))
    this->shadow_ = original;
  this->publish_from_shadow_();
}

void SharpAyZp40krClimate::set_coanda(bool enabled) {
  if (!this->shadow_.power) {
    ESP_LOGW(TAG, "Coanda requires normal HVAC operation");
    this->publish_children_();
    return;
  }
  const auto original = this->shadow_;
  this->shadow_.coanda = enabled;
  if (enabled)
    this->shadow_.spot = SharpSpotPosition::OFF;
  if (!this->send_command_(SharpCommandKind::COANDA))
    this->shadow_ = original;
  this->publish_from_shadow_();
}

void SharpAyZp40krClimate::toggle_vertical_swing() {
  if (!this->shadow_.power) {
    ESP_LOGW(TAG, "Vertical swing toggle requires normal HVAC operation");
    return;
  }
  this->send_command_(SharpCommandKind::VERTICAL_SWING);
}

void SharpAyZp40krClimate::toggle_horizontal_swing() {
  if (!this->shadow_.power) {
    ESP_LOGW(TAG, "Horizontal swing toggle requires normal HVAC operation");
    return;
  }
  this->send_command_(SharpCommandKind::HORIZONTAL_SWING);
}

void SharpAyZp40krClimate::set_spot_air(size_t index) {
  if (index > static_cast<size_t>(SharpSpotPosition::FAR_RIGHT)) {
    ESP_LOGW(TAG, "Invalid Spot Air index %u", static_cast<unsigned>(index));
    this->publish_children_();
    return;
  }
  if (!this->shadow_.power) {
    ESP_LOGW(TAG, "Spot Air requires normal HVAC operation");
    this->publish_children_();
    return;
  }
  const auto original = this->shadow_;
  this->shadow_.spot = static_cast<SharpSpotPosition>(index);
  if (this->shadow_.spot != SharpSpotPosition::OFF)
    this->shadow_.coanda = false;
  if (!this->send_command_(SharpCommandKind::SPOT_AIR))
    this->shadow_ = original;
  this->publish_from_shadow_();
}

void SharpAyZp40krClimate::log_packet_(const char *direction, const SharpA788Packet &packet) const {
  ESP_LOGD(TAG, "%s %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X", direction,
           packet[0], packet[1], packet[2], packet[3], packet[4], packet[5], packet[6], packet[7], packet[8],
           packet[9], packet[10], packet[11], packet[12]);
}

}  // namespace esphome::sharp_ay_zp40kr
