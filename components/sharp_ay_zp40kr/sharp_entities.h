#pragma once

#include <cstddef>

#include "esphome/components/button/button.h"
#include "esphome/components/select/select.h"
#include "esphome/components/switch/switch.h"

namespace esphome::sharp_ay_zp40kr {

class SharpAyZp40krClimate;

class SharpIonSwitch : public switch_::Switch {
 public:
  void set_parent(SharpAyZp40krClimate *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;
  SharpAyZp40krClimate *parent_{nullptr};
};

class SharpCoandaSwitch : public switch_::Switch {
 public:
  void set_parent(SharpAyZp40krClimate *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;
  SharpAyZp40krClimate *parent_{nullptr};
};

class SharpVerticalSwingButton : public button::Button {
 public:
  void set_parent(SharpAyZp40krClimate *parent) { this->parent_ = parent; }

 protected:
  void press_action() override;
  SharpAyZp40krClimate *parent_{nullptr};
};

class SharpHorizontalSwingButton : public button::Button {
 public:
  void set_parent(SharpAyZp40krClimate *parent) { this->parent_ = parent; }

 protected:
  void press_action() override;
  SharpAyZp40krClimate *parent_{nullptr};
};

class SharpSpotAirSelect : public select::Select {
 public:
  void set_parent(SharpAyZp40krClimate *parent) { this->parent_ = parent; }

 protected:
  void control(size_t index) override;
  SharpAyZp40krClimate *parent_{nullptr};
};

}  // namespace esphome::sharp_ay_zp40kr
