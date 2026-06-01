import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["wifi"]

wifi_scanner_ns = cg.esphome_ns.namespace("wifi_scanner")
WiFiScannerComponent = wifi_scanner_ns.class_(
    "WiFiScannerComponent", cg.Component
)

CONF_SCAN_DURATION = "scan_duration"
CONF_REPORT_INTERVAL = "report_interval"
CONF_RECONNECT_WAIT = "reconnect_wait"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WiFiScannerComponent),
    cv.Optional(CONF_SCAN_DURATION, default="5s"): cv.time_period,
    cv.Optional(CONF_REPORT_INTERVAL, default="60s"): cv.time_period,
    cv.Optional(CONF_RECONNECT_WAIT, default="10s"): cv.time_period,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_scan_duration(
        config[CONF_SCAN_DURATION].total_milliseconds
    ))
    cg.add(var.set_report_interval(
        config[CONF_REPORT_INTERVAL].total_milliseconds
    ))
    cg.add(var.set_reconnect_wait(
        config[CONF_RECONNECT_WAIT].total_milliseconds
    ))