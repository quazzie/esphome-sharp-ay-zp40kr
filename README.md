# Sharp AY-ZP40KR ESPHome component

Native ESPHome external component for the Sharp AY-ZP40KR air conditioner and
CRMC-A788JBEZ infrared remote. It sends and receives the captured 13-byte Sharp
A907-family state protocol without a runtime dependency on IRremoteESP8266.

## Supported controls

- Power, Cool, Heat, and Dry
- 18–32 °C setpoint
- Auto fan and physical fan levels 1–3 (mapped to Auto/Low/Medium/High)
- ION, including captured standalone ION operation
- Powerful (`Boost` climate preset)
- ECO (`Eco` climate preset)
- Vacancy / 10 °C heat (`Away` climate preset)
- Coanda
- Stateless vertical and horizontal swing buttons
- All six Spot Air targets plus Off
- Optional receiver synchronization from the physical remote
- Optional current-temperature reporting from any ESPHome sensor

Auto mode, Fan-only mode, fixed louvre positions, and timer functions are not
exposed because they are not controls on the A788 remote.

## Installation

Point `external_components` at this repository and add the climate platform.
See [`examples/sharp_ay_zp40kr.yaml`](examples/sharp_ay_zp40kr.yaml) for the
complete schema. A transmitter is required and a receiver is optional.

```yaml
external_components:
  - source: github://quazzie/esphome-sharp-ay-zp40kr
    components: [sharp_ay_zp40kr]

climate:
  - platform: sharp_ay_zp40kr
    name: Sharp Air Conditioner
    transmitter_id: sharp_ir_tx
    receiver_id: sharp_ir_rx
    time_id: home_time
    sensor: room_temperature
    ion:
      name: Sharp ION
    coanda:
      name: Sharp Coanda
    vertical_swing_toggle:
      name: Toggle Vertical Swing
    horizontal_swing_toggle:
      name: Toggle Horizontal Swing
    spot_air:
      name: Sharp Spot Air
```

`sensor` is optional and uses ESPHome's standard `climate_ir` sensor option. It
must reference the `id` of a sensor defined elsewhere in the same device
configuration. For example, a Home Assistant temperature entity can be imported
with:

```yaml
sensor:
  - platform: homeassistant
    id: room_temperature
    entity_id: sensor.living_room_temperature
```

The referenced sensor is published as the climate entity's current temperature;
it does not affect the IR target-temperature encoding.

`time_id` is optional. When it is omitted or not synchronized, ordinary packets
use the deliberate fallback clock value `00:00`. The physical remote serializes
its current hour/minute in ordinary Start/Stop packets; whether the indoor unit
cares about those values still needs hardware replay testing.

The default `receive_suppression: 250ms` prevents a nearby receiver from
immediately processing the component's own transmission. Set it to another
positive duration if the optical layout requires tuning.

## State model and limitations

The indoor unit does not send its actual state back. Published state is a shadow
of the last local command, restored climate state, or valid physical-remote
packet. It may diverge if the unit misses IR or loses power.

Vertical and horizontal swing are buttons because the A788 sends the same toggle
packet in both directions. ION memory persists while the unit is stopped; the
ION switch reports active operation, not dormant memory. No unobserved packet is
sent to clear dormant ION memory while off.

All current vectors were captured with the indoor unit disconnected. Packet
encoding and remote behavior are tested, but every generated command still needs
replay against a connected AY-ZP40KR before the component should be described as
hardware-validated.

## Tests and capture analysis

```powershell
.\.venv\Scripts\python.exe -m unittest discover -s tests -p "test_*.py" -v
.\.venv\Scripts\python.exe tools\analyze_packets.py --verbose
```

The test suite validates all 55 real captures, including checksum, temperature,
fan, advanced commands, airflow targets, and clock multiplexing. New captures
belong in `tests/captures` and should be checked with the analysis tool before a
new wire value is implemented.

## Hardware safety

Drive the IR LED through a suitable transistor or MOSFET with a current-limiting
resistor. Do not drive a high-power IR LED directly from an ESP GPIO.

## Protocol provenance

The implementation is based on independent CRMC-A788JBEZ captures in
`tests/captures/a788_session_2026-08-04.json`. HeatpumpIR, IRremoteESP8266, and
pyhvac were used only as protocol-family cross-checks. Choose and add a project
license before distributing the repository.
