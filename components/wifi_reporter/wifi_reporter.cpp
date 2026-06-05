#include "wifi_reporter.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wifi_scanner {

static const char *TAG = "wifi_reporter";

void WiFiReporterComponent::setup() {
    ESP_LOGI(TAG, "Reporter ready");
}

void WiFiReporterComponent::loop() {
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

void WiFiReporterComponent::request_ota() {
    ESP_LOGI(TAG, "Sending OTA request to scanner");
    ota_pending_ = true;
    this->write_str("CMD:OTA\n");
    on_ota_status_.call("requesting");
}

void WiFiReporterComponent::cancel_ota() {
    ESP_LOGI(TAG, "Cancelling OTA");
    ota_pending_ = false;
    this->write_str("CMD:CANCEL_OTA\n");
    on_ota_status_.call("cancelled");
}

void WiFiReporterComponent::process_line_(const std::string &line) {
    if (line.empty()) return;

    if (line.rfind("SCAN:", 0) == 0) {
        expected_devices_ = std::stoi(line.substr(5));
        received_devices_ = 0;
        ESP_LOGI(TAG, "Incoming scan: %d devices", expected_devices_);

    } else if (line.rfind("DEV:", 0) == 0) {
        std::string data = line.substr(4);
        size_t comma = data.rfind(',');
        if (comma == std::string::npos) return;

        std::string mac = data.substr(0, comma);
        int rssi = std::stoi(data.substr(comma + 1));

        ESP_LOGI(TAG, "Device: %s RSSI: %d", mac.c_str(), rssi);
        on_device_found_.call(mac, rssi);
        received_devices_++;

    } else if (line == "END") {
        on_scan_complete_.call(received_devices_);

    } else if (line.rfind("OTA:", 0) == 0) {
        std::string status = line.substr(4);
        ESP_LOGI(TAG, "OTA status: %s", status.c_str());
        on_ota_status_.call(status);

        // Auto turn off switch when done/failed/timeout
        if (status == "DONE" || status == "FAILED" ||
            status == "TIMEOUT" || status == "CANCELLED") {
            ota_pending_ = false;
        }
    }
}

}  // namespace wifi_scanner
}  // namespace esphome