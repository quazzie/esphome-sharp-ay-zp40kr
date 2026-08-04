#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome::sharp_ay_zp40kr {

constexpr size_t SHARP_A788_PACKET_SIZE = 13;
constexpr uint16_t SHARP_A788_HEADER_MARK_US = 3800;
constexpr uint16_t SHARP_A788_HEADER_SPACE_US = 1900;
constexpr uint16_t SHARP_A788_BIT_MARK_US = 470;
constexpr uint16_t SHARP_A788_ZERO_SPACE_US = 500;
constexpr uint16_t SHARP_A788_ONE_SPACE_US = 1400;
constexpr uint32_t SHARP_A788_CARRIER_HZ = 38000;

using SharpA788Packet = std::array<uint8_t, SHARP_A788_PACKET_SIZE>;

enum class SharpHvacMode : uint8_t {
  HEAT = 1,
  COOL = 2,
  DRY = 3,
};

enum class SharpFanMode : uint8_t {
  AUTO = 2,
  LEVEL_1 = 3,
  LEVEL_2 = 5,
  LEVEL_3 = 7,
};

enum class SharpSpotPosition : uint8_t {
  OFF,
  NEAR_LEFT,
  NEAR_CENTER,
  NEAR_RIGHT,
  FAR_LEFT,
  FAR_CENTER,
  FAR_RIGHT,
};

enum class SharpCommandKind : uint8_t {
  UNKNOWN,
  POWER,
  MODE,
  TEMPERATURE,
  FAN,
  ION,
  POWERFUL,
  ECO,
  VACANCY,
  VERTICAL_SWING,
  HORIZONTAL_SWING,
  COANDA,
  SPOT_AIR,
};

struct SharpClockTime {
  uint8_t hour{0};
  uint8_t minute{0};
};

struct SharpA788State {
  // `power` is normal Heat/Cool/Dry operation. ION-only is represented
  // separately so it is never published as a climate operating mode.
  bool power{false};
  bool ion_only{false};
  SharpHvacMode mode{SharpHvacMode::COOL};
  uint8_t temperature_c{24};
  SharpFanMode fan{SharpFanMode::AUTO};

  // `ion` is the remote's persistent ION memory bit. It may remain set while
  // both normal HVAC operation and ION-only operation are stopped.
  bool ion{false};
  bool powerful{false};
  bool eco{false};
  bool vacancy{false};
  bool coanda{false};
  SharpSpotPosition spot{SharpSpotPosition::OFF};
};

enum class SharpEncodeError : uint8_t {
  NONE,
  INVALID_TEMPERATURE,
  INVALID_CLOCK,
  COMMAND_REQUIRES_POWER,
  UNSUPPORTED_ION_TRANSITION,
};

struct SharpEncodeResult {
  bool valid{false};
  SharpEncodeError error{SharpEncodeError::NONE};
  SharpA788Packet packet{};
};

struct SharpDecodedPacket {
  bool valid{false};
  bool header_valid{false};
  bool checksum_valid{false};
  bool has_unknown_fields{false};
  SharpCommandKind command{SharpCommandKind::UNKNOWN};
  SharpA788State state{};
  SharpA788Packet raw{};

  uint8_t action_class{0};
  uint8_t temperature_code{0};
  uint8_t mode_code{0};
  uint8_t fan_code{0};
  uint8_t airflow_code{0};
  uint8_t command_payload{0};
};

uint8_t sharp_a788_calculate_checksum(const SharpA788Packet &packet);
bool sharp_a788_valid_checksum(const SharpA788Packet &packet);
bool sharp_a788_valid_header(const SharpA788Packet &packet);

SharpEncodeResult sharp_a788_encode(const SharpA788State &state, SharpCommandKind command,
                                    SharpClockTime clock = {});
SharpDecodedPacket sharp_a788_decode(const SharpA788Packet &packet, const SharpA788State &previous = {});

uint8_t sharp_a788_spot_wire_value(SharpSpotPosition spot);
const char *sharp_a788_command_to_string(SharpCommandKind command);
const char *sharp_a788_spot_to_string(SharpSpotPosition spot);

}  // namespace esphome::sharp_ay_zp40kr
