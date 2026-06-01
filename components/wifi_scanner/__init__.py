import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["wifi"]
AUTO_LOAD = []

wifi_scanner_ns = cg.esphome_ns.namespace("wifi_scanner")
WiFiScannerComponent = wifi_scanner_ns.class_(
    "WiFiScannerComponent", cg.Component
)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WiFiScannerComponent),
    cv.Optional("scan_interval", default="30s"): cv.time_period,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_scan_interval(
        config["scan_interval"].total_milliseconds
    ))