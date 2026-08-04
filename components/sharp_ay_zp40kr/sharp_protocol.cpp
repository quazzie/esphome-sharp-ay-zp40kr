#include "sharp_protocol.h"

#include <algorithm>

namespace esphome::sharp_ay_zp40kr {

namespace {

constexpr uint8_t ACTION_START = 1;
constexpr uint8_t ACTION_STOP = 2;
constexpr uint8_t ACTION_UPDATE = 3;
constexpr uint8_t ACTION_SPECIAL_ON = 6;
constexpr uint8_t ACTION_SPECIAL_OFF = 7;

constexpr uint8_t AIRFLOW_NEUTRAL = 0x08;
constexpr uint8_t AIRFLOW_COANDA_OFF = 0x0D;
constexpr uint8_t AIRFLOW_COANDA_ON = 0x0E;
constexpr uint8_t AIRFLOW_VERTICAL_TOGGLE = 0x0F;
constexpr uint8_t AIRFLOW_HORIZONTAL_TOGGLE = 0xF8;

constexpr uint8_t PAYLOAD_POWERFUL = 0x01;
constexpr uint8_t PAYLOAD_TEMPERATURE = 0x04;
constexpr uint8_t PAYLOAD_FAN = 0x05;
constexpr uint8_t PAYLOAD_VERTICAL = 0x06;
constexpr uint8_t PAYLOAD_HORIZONTAL = 0x07;
constexpr uint8_t PAYLOAD_SPOT = 0x09;
constexpr uint8_t PAYLOAD_ECO = 0x20;
constexpr uint8_t PAYLOAD_ION_ONLY = 0x33;

void set_action(SharpA788Packet &packet, uint8_t action) { packet[5] = static_cast<uint8_t>((action << 4) | 0x01); }

bool decode_mode(uint8_t code, SharpHvacMode *mode) {
  switch (code) {
    case 1:
      *mode = SharpHvacMode::HEAT;
      return true;
    case 2:
      *mode = SharpHvacMode::COOL;
      return true;
    case 3:
      *mode = SharpHvacMode::DRY;
      return true;
    default:
      return false;
  }
}

bool decode_fan(uint8_t code, SharpFanMode *fan) {
  switch (code) {
    case 2:
      *fan = SharpFanMode::AUTO;
      return true;
    case 3:
      *fan = SharpFanMode::LEVEL_1;
      return true;
    case 5:
      *fan = SharpFanMode::LEVEL_2;
      return true;
    case 7:
      *fan = SharpFanMode::LEVEL_3;
      return true;
    default:
      return false;
  }
}

bool decode_spot(uint8_t code, SharpSpotPosition *spot) {
  switch (code) {
    case 0x08:
      *spot = SharpSpotPosition::OFF;
      return true;
    case 0x2C:
      *spot = SharpSpotPosition::NEAR_LEFT;
      return true;
    case 0x1C:
      *spot = SharpSpotPosition::NEAR_CENTER;
      return true;
    case 0x3C:
      *spot = SharpSpotPosition::NEAR_RIGHT;
      return true;
    case 0x2B:
      *spot = SharpSpotPosition::FAR_LEFT;
      return true;
    case 0x1B:
      *spot = SharpSpotPosition::FAR_CENTER;
      return true;
    case 0x3B:
      *spot = SharpSpotPosition::FAR_RIGHT;
      return true;
    default:
      return false;
  }
}

uint8_t normal_temperature_code(const SharpA788State &state) {
  if (state.mode == SharpHvacMode::DRY || state.vacancy)
    return 0;
  return static_cast<uint8_t>(state.temperature_c - 17);
}

void finalize_checksum(SharpA788Packet &packet) {
  packet[12] &= 0x0F;
  packet[12] |= static_cast<uint8_t>(sharp_a788_calculate_checksum(packet) << 4);
}

}  // namespace

uint8_t sharp_a788_calculate_checksum(const SharpA788Packet &packet) {
  uint8_t value = 0;
  for (size_t i = 0; i < SHARP_A788_PACKET_SIZE - 1; i++)
    value ^= packet[i];
  value ^= packet[12] & 0x0F;
  value ^= value >> 4;
  return value & 0x0F;
}

bool sharp_a788_valid_checksum(const SharpA788Packet &packet) {
  return (packet[12] >> 4) == sharp_a788_calculate_checksum(packet);
}

bool sharp_a788_valid_header(const SharpA788Packet &packet) {
  return packet[0] == 0xAA && packet[1] == 0x5A && packet[2] == 0xCF && packet[3] == 0x10;
}

uint8_t sharp_a788_spot_wire_value(SharpSpotPosition spot) {
  switch (spot) {
    case SharpSpotPosition::OFF:
      return AIRFLOW_NEUTRAL;
    case SharpSpotPosition::NEAR_LEFT:
      return 0x2C;
    case SharpSpotPosition::NEAR_CENTER:
      return 0x1C;
    case SharpSpotPosition::NEAR_RIGHT:
      return 0x3C;
    case SharpSpotPosition::FAR_LEFT:
      return 0x2B;
    case SharpSpotPosition::FAR_CENTER:
      return 0x1B;
    case SharpSpotPosition::FAR_RIGHT:
      return 0x3B;
  }
  return AIRFLOW_NEUTRAL;
}

SharpEncodeResult sharp_a788_encode(const SharpA788State &state, SharpCommandKind command, SharpClockTime clock) {
  SharpEncodeResult result;
  if (state.temperature_c < 18 || state.temperature_c > 32) {
    result.error = SharpEncodeError::INVALID_TEMPERATURE;
    return result;
  }
  if (clock.hour > 23 || clock.minute > 59) {
    result.error = SharpEncodeError::INVALID_CLOCK;
    return result;
  }

  auto &packet = result.packet;
  packet = {0xAA, 0x5A, 0xCF, 0x10, normal_temperature_code(state), 0x31,
            static_cast<uint8_t>((static_cast<uint8_t>(state.fan) << 4) | static_cast<uint8_t>(state.mode)),
            clock.hour, sharp_a788_spot_wire_value(state.spot), 0x80, clock.minute,
            static_cast<uint8_t>(state.ion ? 0xF4 : 0xF0), 0x01};

  switch (command) {
    case SharpCommandKind::POWER:
      set_action(packet, state.power ? ACTION_START : ACTION_STOP);
      break;
    case SharpCommandKind::MODE:
      if (!state.power) {
        result.error = SharpEncodeError::COMMAND_REQUIRES_POWER;
        return result;
      }
      // A powered transition to a different operating mode uses update/00.
      // Start and same-mode reselect use the POWER packet family instead.
      set_action(packet, ACTION_UPDATE);
      packet[10] = 0x00;
      break;
    case SharpCommandKind::TEMPERATURE:
      if (!state.power) {
        result.error = SharpEncodeError::COMMAND_REQUIRES_POWER;
        return result;
      }
      set_action(packet, ACTION_UPDATE);
      packet[10] = PAYLOAD_TEMPERATURE;
      break;
    case SharpCommandKind::FAN:
      if (!state.power) {
        result.error = SharpEncodeError::COMMAND_REQUIRES_POWER;
        return result;
      }
      set_action(packet, ACTION_UPDATE);
      packet[10] = PAYLOAD_FAN;
      break;
    case SharpCommandKind::ION:
      if (state.ion_only) {
        packet[4] = 0x01;
        set_action(packet, ACTION_START);
        packet[6] = 0x34;
        packet[8] = AIRFLOW_NEUTRAL;
        packet[10] = PAYLOAD_ION_ONLY;
        packet[11] = 0xF4;
      } else if (!state.power && state.ion) {
        // Captured ION-only Stop retains the persistent ION memory bit.
        set_action(packet, ACTION_STOP);
        packet[10] = PAYLOAD_ION_ONLY;
        packet[11] = 0xF4;
      } else if (!state.power) {
        result.error = SharpEncodeError::UNSUPPORTED_ION_TRANSITION;
        return result;
      } else {
        set_action(packet, state.ion ? ACTION_SPECIAL_ON : ACTION_SPECIAL_OFF);
        packet[7] = 0x08;
        packet[8] = AIRFLOW_NEUTRAL;
        packet[10] = 0x00;
      }
      break;
    case SharpCommandKind::POWERFUL:
      if (!state.power) {
        result.error = SharpEncodeError::COMMAND_REQUIRES_POWER;
        return result;
      }
      set_action(packet, state.powerful ? ACTION_SPECIAL_ON : ACTION_SPECIAL_OFF);
      if (state.powerful)
        packet[6] = static_cast<uint8_t>((static_cast<uint8_t>(SharpFanMode::AUTO) << 4) |
                                         static_cast<uint8_t>(state.mode));
      packet[7] = 0x00;
      packet[8] = AIRFLOW_NEUTRAL;
      packet[10] = PAYLOAD_POWERFUL;
      break;
    case SharpCommandKind::ECO:
      if (!state.power) {
        result.error = SharpEncodeError::COMMAND_REQUIRES_POWER;
        return result;
      }
      set_action(packet, state.eco ? ACTION_SPECIAL_ON : ACTION_SPECIAL_OFF);
      packet[7] = 0x00;
      packet[10] = PAYLOAD_ECO;
      break;
    case SharpCommandKind::VACANCY:
      if (!state.power) {
        result.error = SharpEncodeError::COMMAND_REQUIRES_POWER;
        return result;
      }
      set_action(packet, ACTION_UPDATE);
      packet[4] = state.vacancy ? 0x00 : 0x01;
      packet[6] = static_cast<uint8_t>((static_cast<uint8_t>(state.fan) << 4) |
                                       static_cast<uint8_t>(SharpHvacMode::HEAT));
      packet[10] = PAYLOAD_TEMPERATURE;
      break;
    case SharpCommandKind::VERTICAL_SWING:
      if (!state.power) {
        result.error = SharpEncodeError::COMMAND_REQUIRES_POWER;
        return result;
      }
      set_action(packet, ACTION_UPDATE);
      packet[8] = AIRFLOW_VERTICAL_TOGGLE;
      packet[10] = PAYLOAD_VERTICAL;
      break;
    case SharpCommandKind::HORIZONTAL_SWING:
      if (!state.power) {
        result.error = SharpEncodeError::COMMAND_REQUIRES_POWER;
        return result;
      }
      set_action(packet, ACTION_UPDATE);
      packet[8] = AIRFLOW_HORIZONTAL_TOGGLE;
      packet[10] = PAYLOAD_HORIZONTAL;
      break;
    case SharpCommandKind::COANDA:
      if (!state.power) {
        result.error = SharpEncodeError::COMMAND_REQUIRES_POWER;
        return result;
      }
      set_action(packet, ACTION_UPDATE);
      packet[8] = state.coanda ? AIRFLOW_COANDA_ON : AIRFLOW_COANDA_OFF;
      packet[10] = PAYLOAD_VERTICAL;
      break;
    case SharpCommandKind::SPOT_AIR:
      if (!state.power) {
        result.error = SharpEncodeError::COMMAND_REQUIRES_POWER;
        return result;
      }
      set_action(packet, ACTION_UPDATE);
      packet[8] = sharp_a788_spot_wire_value(state.spot);
      packet[10] = PAYLOAD_SPOT;
      break;
    case SharpCommandKind::UNKNOWN:
      result.error = SharpEncodeError::COMMAND_REQUIRES_POWER;
      return result;
  }

  finalize_checksum(packet);
  result.valid = true;
  return result;
}

SharpDecodedPacket sharp_a788_decode(const SharpA788Packet &packet, const SharpA788State &previous) {
  SharpDecodedPacket out;
  out.raw = packet;
  out.state = previous;
  out.header_valid = sharp_a788_valid_header(packet);
  out.checksum_valid = sharp_a788_valid_checksum(packet);
  if (!out.header_valid || !out.checksum_valid)
    return out;

  out.action_class = packet[5] >> 4;
  out.temperature_code = packet[4] & 0x0F;
  out.mode_code = packet[6] & 0x03;
  out.fan_code = (packet[6] >> 4) & 0x07;
  out.airflow_code = packet[8];
  out.command_payload = packet[10];

  const bool ion_only_start = (packet[6] & 0x0F) == 0x04 && out.action_class == ACTION_START &&
                              out.command_payload == PAYLOAD_ION_ONLY;
  const bool ion_only_stop = out.action_class == ACTION_STOP && out.command_payload == PAYLOAD_ION_ONLY;

  if (ion_only_start) {
    out.command = SharpCommandKind::ION;
    out.state.power = false;
    out.state.ion_only = true;
    out.state.ion = true;
  } else {
    SharpHvacMode mode;
    SharpFanMode fan;
    if (!decode_mode(out.mode_code, &mode) || !decode_fan(out.fan_code, &fan))
      out.has_unknown_fields = true;
    else {
      out.state.mode = mode;
      out.state.fan = fan;
    }

    out.state.ion = (packet[11] & 0x04) != 0;
    if (ion_only_stop) {
      out.command = SharpCommandKind::ION;
      out.state.power = false;
      out.state.ion_only = false;
    } else {
      out.state.ion_only = false;
      if (out.action_class == ACTION_START || out.action_class == ACTION_UPDATE ||
          out.action_class == ACTION_SPECIAL_ON || out.action_class == ACTION_SPECIAL_OFF)
        out.state.power = true;
      else if (out.action_class == ACTION_STOP)
        out.state.power = false;

      if (out.state.mode == SharpHvacMode::DRY) {
        if (out.temperature_code != 0)
          out.has_unknown_fields = true;
        out.state.vacancy = false;
      } else if (out.state.mode == SharpHvacMode::HEAT && out.temperature_code == 0) {
        out.state.vacancy = true;
      } else if (out.temperature_code >= 1 && out.temperature_code <= 15) {
        out.state.temperature_c = static_cast<uint8_t>(out.temperature_code + 17);
        out.state.vacancy = false;
      } else {
        out.has_unknown_fields = true;
      }

      if ((out.action_class == ACTION_SPECIAL_ON || out.action_class == ACTION_SPECIAL_OFF) &&
          out.command_payload == PAYLOAD_POWERFUL) {
        out.command = SharpCommandKind::POWERFUL;
        out.state.powerful = out.action_class == ACTION_SPECIAL_ON;
        if (out.state.powerful) {
          out.state.eco = false;
          out.state.spot = SharpSpotPosition::OFF;
          out.state.fan = SharpFanMode::AUTO;
        }
      } else if ((out.action_class == ACTION_SPECIAL_ON || out.action_class == ACTION_SPECIAL_OFF) &&
                 out.command_payload == PAYLOAD_ECO) {
        out.command = SharpCommandKind::ECO;
        out.state.eco = out.action_class == ACTION_SPECIAL_ON;
        if (out.state.eco)
          out.state.powerful = false;
      } else if ((out.action_class == ACTION_SPECIAL_ON || out.action_class == ACTION_SPECIAL_OFF) &&
                 out.command_payload == 0x00) {
        out.command = SharpCommandKind::ION;
      } else if (out.command_payload == PAYLOAD_TEMPERATURE && out.action_class == ACTION_UPDATE) {
        out.command = out.state.vacancy ? SharpCommandKind::VACANCY : SharpCommandKind::TEMPERATURE;
      } else if (out.command_payload == PAYLOAD_FAN && out.action_class == ACTION_UPDATE) {
        out.command = SharpCommandKind::FAN;
      } else if (out.command_payload == PAYLOAD_VERTICAL && out.action_class == ACTION_UPDATE) {
        if (out.airflow_code == AIRFLOW_VERTICAL_TOGGLE) {
          out.command = SharpCommandKind::VERTICAL_SWING;
        } else if (out.airflow_code == AIRFLOW_COANDA_ON || out.airflow_code == AIRFLOW_COANDA_OFF) {
          out.command = SharpCommandKind::COANDA;
          out.state.coanda = out.airflow_code == AIRFLOW_COANDA_ON;
          if (out.state.coanda)
            out.state.spot = SharpSpotPosition::OFF;
        }
      } else if (out.command_payload == PAYLOAD_HORIZONTAL && out.action_class == ACTION_UPDATE &&
                 out.airflow_code == AIRFLOW_HORIZONTAL_TOGGLE) {
        out.command = SharpCommandKind::HORIZONTAL_SWING;
      } else if (out.command_payload == PAYLOAD_SPOT && out.action_class == ACTION_UPDATE) {
        out.command = SharpCommandKind::SPOT_AIR;
        SharpSpotPosition spot;
        if (decode_spot(out.airflow_code, &spot)) {
          out.state.spot = spot;
          if (spot != SharpSpotPosition::OFF)
            out.state.coanda = false;
        } else {
          out.has_unknown_fields = true;
        }
      } else if (out.command_payload == 0x00 && out.action_class == ACTION_UPDATE) {
        // Captured powered mode changes (Heat/Cool/Dry) use the normal update
        // action with a zero payload.
        out.command = SharpCommandKind::MODE;
      } else if (out.action_class == ACTION_START || out.action_class == ACTION_STOP) {
        out.command = SharpCommandKind::POWER;
        // The A788 mode buttons and Start produce the same state/action family.
        // Preserve the ambiguity rather than inventing a discriminator.
        if (out.action_class == ACTION_START && previous.power && previous.mode != out.state.mode)
          out.command = SharpCommandKind::MODE;
      }

      if (out.command == SharpCommandKind::MODE)
        out.state.powerful = false;

      if (out.command != SharpCommandKind::COANDA && out.command != SharpCommandKind::VERTICAL_SWING &&
          out.command != SharpCommandKind::HORIZONTAL_SWING) {
        SharpSpotPosition serialized_spot;
        if (decode_spot(out.airflow_code, &serialized_spot)) {
          out.state.spot = serialized_spot;
          if (serialized_spot != SharpSpotPosition::OFF)
            out.state.coanda = false;
        }
      }
    }
  }

  if ((packet[4] & 0xF0) != 0 || (packet[5] & 0x0F) != 0x01 || packet[9] != 0x80 ||
      (packet[11] & 0xFB) != 0xF0 || (packet[12] & 0x0F) != 0x01)
    out.has_unknown_fields = true;

  if (out.command == SharpCommandKind::UNKNOWN)
    out.has_unknown_fields = true;
  out.valid = true;
  return out;
}

const char *sharp_a788_command_to_string(SharpCommandKind command) {
  switch (command) {
    case SharpCommandKind::POWER: return "power";
    case SharpCommandKind::MODE: return "mode";
    case SharpCommandKind::TEMPERATURE: return "temperature";
    case SharpCommandKind::FAN: return "fan";
    case SharpCommandKind::ION: return "ion";
    case SharpCommandKind::POWERFUL: return "powerful";
    case SharpCommandKind::ECO: return "eco";
    case SharpCommandKind::VACANCY: return "vacancy";
    case SharpCommandKind::VERTICAL_SWING: return "vertical_swing";
    case SharpCommandKind::HORIZONTAL_SWING: return "horizontal_swing";
    case SharpCommandKind::COANDA: return "coanda";
    case SharpCommandKind::SPOT_AIR: return "spot_air";
    case SharpCommandKind::UNKNOWN: return "unknown";
  }
  return "unknown";
}

const char *sharp_a788_spot_to_string(SharpSpotPosition spot) {
  switch (spot) {
    case SharpSpotPosition::OFF: return "Off";
    case SharpSpotPosition::NEAR_LEFT: return "Near Left";
    case SharpSpotPosition::NEAR_CENTER: return "Near Center";
    case SharpSpotPosition::NEAR_RIGHT: return "Near Right";
    case SharpSpotPosition::FAR_LEFT: return "Far Left";
    case SharpSpotPosition::FAR_CENTER: return "Far Center";
    case SharpSpotPosition::FAR_RIGHT: return "Far Right";
  }
  return "Off";
}

}  // namespace esphome::sharp_ay_zp40kr
