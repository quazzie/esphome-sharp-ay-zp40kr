#include "sharp_entities.h"

#include "sharp_ay_zp40kr.h"

namespace esphome::sharp_ay_zp40kr {

void SharpIonSwitch::write_state(bool state) {
  if (this->parent_ != nullptr)
    this->parent_->set_ion(state);
}

void SharpCoandaSwitch::write_state(bool state) {
  if (this->parent_ != nullptr)
    this->parent_->set_coanda(state);
}

void SharpVerticalSwingButton::press_action() {
  if (this->parent_ != nullptr)
    this->parent_->toggle_vertical_swing();
}

void SharpHorizontalSwingButton::press_action() {
  if (this->parent_ != nullptr)
    this->parent_->toggle_horizontal_swing();
}

void SharpSpotAirSelect::control(size_t index) {
  if (this->parent_ != nullptr)
    this->parent_->set_spot_air(index);
}

}  // namespace esphome::sharp_ay_zp40kr
