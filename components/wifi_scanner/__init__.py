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
CONF_OTA_TIMEOUT     = "ota_timeout"
CONF_WIFI_SSID       = "wifi_ssid"
CONF_WIFI_PASSWORD   = "wifi_password"

SCANNER_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(WiFiScannerComponent),
    cv.Optional(CONF_REPORT_INTERVAL, default="30s"): cv.time_period,
    cv.Optional(CONF_RSSI_THRESHOLD, default=-90): cv.int_range(min=-120, max=0),
    cv.Optional(CONF_OTA_TIMEOUT, default="120s"): cv.time_period,
    cv.Required(CONF_WIFI_SSID): cv.string,
    cv.Required(CONF_WIFI_PASSWORD): cv.string,
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
        cg.add(var.set_ota_timeout(
            config[CONF_OTA_TIMEOUT].total_milliseconds
        ))
        cg.add(var.set_wifi_ssid(config[CONF_WIFI_SSID]))
        cg.add(var.set_wifi_password(config[CONF_WIFI_PASSWORD]))

    elif config["type"] == "reporter":
        var = cg.new_Pvariable(config[CONF_ID])
        await cg.register_component(var, config)
        await uart.register_uart_device(var, config)