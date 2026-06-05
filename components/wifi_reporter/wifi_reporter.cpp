#include "wifi_reporter.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wifi_scanner {

static const char *TAG = "wifi_reporter";

void WiFiReporterComponent::setup() {
    ESP_LOGI(TAG, "Reporter ready, listening on UART");
}

void WiFiReporterComponent::loop() {
    // Read UART byte by byte into buffer
    while (this->available()) {
        char c = this->read();
        if (c == '\n') {
            process_line_(rx_buffer_);
            rx_buffer_.clear();
        } else if (c != '\r') {
            rx_buffer_ += c;
        }
    }
}

void WiFiReporterComponent::process_line_(const std::string &line) {
    if (line.empty()) return;

    if (line.rfind("SCAN:", 0) == 0) {
        // "SCAN:4" — new scan result incoming
        expected_devices_ = std::stoi(line.substr(5));
        received_devices_ = 0;
        ESP_LOGI(TAG, "Incoming scan: %d devices", expected_devices_);

    } else if (line.rfind("DEV:", 0) == 0) {
        // "DEV:AA:BB:CC:DD:EE:FF,-65"
        std::string data = line.substr(4);
        size_t comma = data.rfind(',');  // rfind avoids MAC colons
        if (comma == std::string::npos) return;

        std::string mac = data.substr(0, comma);
        int rssi = std::stoi(data.substr(comma + 1));

        ESP_LOGI(TAG, "Device: %s  RSSI: %d", mac.c_str(), rssi);
        on_device_found_.call(mac, rssi);
        received_devices_++;

    } else if (line == "END") {
        ESP_LOGI(TAG, "Scan complete: %d/%d devices received",
                 received_devices_, expected_devices_);
        on_scan_complete_.call(received_devices_);
    }
}

}  // namespace wifi_scanner
}  // namespace esphome