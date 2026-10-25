#pragma once

#include <cstdint>

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/preferences.h"
#include "sharp_entities.h"
#include "sharp_protocol.h"

namespace esphome::sharp_ay_zp40kr {

class SharpAyZp40krClimate : public climate_ir::ClimateIR {
 public:
  SharpAyZp40krClimate()
      : climate_ir::ClimateIR(18.0f, 32.0f, 1.0f, true, false,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW,
                               climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH},
                              {}, {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_AWAY,
                                   climate::CLIMATE_PRESET_BOOST, climate::CLIMATE_PRESET_ECO}) {}

  void setup() override;
  void dump_config() override;

  void set_time(time::RealTimeClock *time) { this->time_ = time; }
  void set_receive_suppression(uint32_t milliseconds) { this->receive_suppression_ms_ = milliseconds; }

  void set_ion_switch(SharpIonSwitch *entity) { this->ion_switch_ = entity; }
  void set_coanda_switch(SharpCoandaSwitch *entity) { this->coanda_switch_ = entity; }
  void set_spot_air_select(SharpSpotAirSelect *entity) { this->spot_air_select_ = entity; }
  void set_power_sensor(sensor::Sensor *power_sensor, float active_threshold_w) {
    this->power_sensor_ = power_sensor;
    this->active_threshold_ = active_threshold_w;
  }

  void set_ion(bool enabled);
  void set_coanda(bool enabled);
  void toggle_vertical_swing();
  void toggle_horizontal_swing();
  void set_spot_air(size_t index);

 protected:
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  bool send_command_(SharpCommandKind command);
  SharpClockTime clock_now_() const;
  void sync_shadow_from_climate_();
  void publish_from_shadow_();
  void publish_children_();
  void remember_temperature_();
  void restore_shadow_();
  void save_shadow_();
  void log_packet_(const char *direction, const SharpA788Packet &packet) const;
  void update_action_(float power_w);

  struct RestoreData {
    uint8_t marker{0xA8};
    uint8_t mode{static_cast<uint8_t>(SharpHvacMode::COOL)};
    uint8_t temperature{24};
    uint8_t fan{static_cast<uint8_t>(SharpFanMode::AUTO)};
    uint8_t heat_temperature{18};
    uint8_t cool_temperature{24};
    uint8_t spot{static_cast<uint8_t>(SharpSpotPosition::OFF)};
    uint8_t flags{0};
  };

  SharpA788State shadow_{};
  float heat_temperature_{18.0f};
  float cool_temperature_{24.0f};

  time::RealTimeClock *time_{nullptr};
  SharpIonSwitch *ion_switch_{nullptr};
  SharpCoandaSwitch *coanda_switch_{nullptr};
  SharpSpotAirSelect *spot_air_select_{nullptr};
  sensor::Sensor *power_sensor_{nullptr};
  float active_threshold_{50.0f};
  float last_power_w_{0.0f};

  uint32_t last_transmit_ms_{0};
  uint32_t receive_suppression_ms_{250};
  ESPPreferenceObject shadow_preference_{};
};

}  // namespace esphome::sharp_ay_zp40kr
