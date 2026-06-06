import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import uart

DEPENDENCIES = []
AUTO_LOAD = ["uart"]

wifi_scanner_ns = cg.esphome_ns.namespace("wifi_scanner")

WiFiScannerComponent = wifi_scanner_ns.class_(
    "WiFiScannerComponent", cg.Component, uart.UARTDevice
)
WiFiReporterComponent = wifi_scanner_ns.class_(
    "WiFiReporterComponent", cg.Component, uart.UARTDevice
)

CONF_REPORT_INTERVAL = "report_interval"
CONF_RSSI_THRESHOLD  = "rssi_threshold"

SCANNER_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WiFiScannerComponent),
    cv.Optional(CONF_REPORT_INTERVAL, default="30s"): cv.time_period,
    cv.Optional(CONF_RSSI_THRESHOLD, default=-90): cv.int_range(min=-120, max=0),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)

REPORTER_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WiFiReporterComponent),
}).extend(cv.COMPONENT_SCHEMA).extend(uart.UART_DEVICE_SCHEMA)

CONFIG_SCHEMA = cv.typed_schema({
    "scanner": SCANNER_SCHEMA,
    "reporter": REPORTER_SCHEMA,
}, key="type")

async def to_code(config):
    if config["type"] == "scanner":
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)
        await uart.register_uart_device(var, config)
        cg.add(var.set_report_interval(
            config[CONF_REPORT_INTERVAL].total_milliseconds
        ))
        cg.add(var.set_rssi_threshold(config[CONF_RSSI_THRESHOLD]))

    elif config["type"] == "reporter":
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)
        await uart.register_uart_device(var, config)