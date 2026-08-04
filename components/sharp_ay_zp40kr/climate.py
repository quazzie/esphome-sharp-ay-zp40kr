import esphome.codegen as cg
from esphome.components import button, climate_ir, select, switch, time
import esphome.config_validation as cv

AUTO_LOAD = ["button", "climate_ir", "select", "switch"]

CONF_MODEL = "model"
CONF_TIME_ID = "time_id"
CONF_RECEIVE_SUPPRESSION = "receive_suppression"
CONF_ION = "ion"
CONF_COANDA = "coanda"
CONF_VERTICAL_SWING_TOGGLE = "vertical_swing_toggle"
CONF_HORIZONTAL_SWING_TOGGLE = "horizontal_swing_toggle"
CONF_SPOT_AIR = "spot_air"

SPOT_AIR_OPTIONS = [
    "Off",
    "Near Left",
    "Near Center",
    "Near Right",
    "Far Left",
    "Far Center",
    "Far Right",
]

sharp_ns = cg.esphome_ns.namespace("sharp_ay_zp40kr")
SharpAyZp40krClimate = sharp_ns.class_(
    "SharpAyZp40krClimate", climate_ir.ClimateIR
)
SharpIonSwitch = sharp_ns.class_("SharpIonSwitch", switch.Switch)
SharpCoandaSwitch = sharp_ns.class_("SharpCoandaSwitch", switch.Switch)
SharpVerticalSwingButton = sharp_ns.class_(
    "SharpVerticalSwingButton", button.Button
)
SharpHorizontalSwingButton = sharp_ns.class_(
    "SharpHorizontalSwingButton", button.Button
)
SharpSpotAirSelect = sharp_ns.class_("SharpSpotAirSelect", select.Select)


CONFIG_SCHEMA = climate_ir.climate_ir_with_receiver_schema(
    SharpAyZp40krClimate
).extend(
    {
        cv.Optional(CONF_MODEL, default="crmc_a788jbez"): cv.one_of(
            "crmc_a788jbez", lower=True
        ),
        cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
        cv.Optional(
            CONF_RECEIVE_SUPPRESSION, default="250ms"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ION): switch.switch_schema(
            SharpIonSwitch,
            block_inverted=True,
            default_restore_mode="DISABLED",
            icon="mdi:air-filter",
        ),
        cv.Optional(CONF_COANDA): switch.switch_schema(
            SharpCoandaSwitch,
            block_inverted=True,
            default_restore_mode="DISABLED",
            icon="mdi:weather-windy",
        ),
        cv.Optional(CONF_VERTICAL_SWING_TOGGLE): button.button_schema(
            SharpVerticalSwingButton, icon="mdi:swap-vertical"
        ),
        cv.Optional(CONF_HORIZONTAL_SWING_TOGGLE): button.button_schema(
            SharpHorizontalSwingButton, icon="mdi:swap-horizontal"
        ),
        cv.Optional(CONF_SPOT_AIR): select.select_schema(
            SharpSpotAirSelect, icon="mdi:target"
        ),
    }
)


async def to_code(config):
    var = await climate_ir.new_climate_ir(config)
    cg.add(
        var.set_receive_suppression(
            config[CONF_RECEIVE_SUPPRESSION].total_milliseconds
        )
    )

    if time_id := config.get(CONF_TIME_ID):
        clock = await cg.get_variable(time_id)
        cg.add(var.set_time(clock))

    if ion_config := config.get(CONF_ION):
        entity = await switch.new_switch(ion_config)
        cg.add(entity.set_parent(var))
        cg.add(var.set_ion_switch(entity))

    if coanda_config := config.get(CONF_COANDA):
        entity = await switch.new_switch(coanda_config)
        cg.add(entity.set_parent(var))
        cg.add(var.set_coanda_switch(entity))

    if vertical_config := config.get(CONF_VERTICAL_SWING_TOGGLE):
        entity = await button.new_button(vertical_config)
        cg.add(entity.set_parent(var))

    if horizontal_config := config.get(CONF_HORIZONTAL_SWING_TOGGLE):
        entity = await button.new_button(horizontal_config)
        cg.add(entity.set_parent(var))

    if spot_config := config.get(CONF_SPOT_AIR):
        entity = await select.new_select(spot_config, options=SPOT_AIR_OPTIONS)
        cg.add(entity.set_parent(var))
        cg.add(var.set_spot_air_select(entity))
