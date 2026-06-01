#include "wifi_scanner.h"
#include "esphome/core/log.h"
#include "esphome/components/wifi/wifi_component.h"

namespace esphome {
namespace wifi_scanner {

static const char *TAG = "wifi_scanner";

WiFiScannerComponent *WiFiScannerComponent::instance_ = nullptr;

void WiFiScannerComponent::setup() {
    instance_ = this;
    state_ = State::CONNECTED;
    last_report_ = millis();
    ESP_LOGI(TAG, "WiFi Scanner ready (time-sliced mode)");
}

void WiFiScannerComponent::loop() {
    uint32_t now = millis();

    switch (state_) {

        case State::CONNECTED:
            // Wait for next scan window
            if (now - last_report_ >= report_interval_) {
                start_scan_();
            }
            break;

        case State::SCANNING:
            // Scan for scan_duration_ ms then stop
            if (now - state_start_ >= scan_duration_) {
                stop_scan_();
            }
            break;

        case State::RECONNECTING:
            // Wait for WiFi to reconnect
            if (wifi::global_wifi_component->is_connected()) {
                ESP_LOGI(TAG, "WiFi reconnected, reporting...");
                state_ = State::REPORTING;
                state_start_ = now;
            } else if (now - state_start_ >= reconnect_wait_) {
                ESP_LOGW(TAG, "Reconnect timeout, retrying...");
                reconnect_();
            }
            break;

        case State::REPORTING:
            report_devices_();
            state_ = State::CONNECTED;
            last_report_ = now;
            break;
    }
}

void WiFiScannerComponent::start_scan_() {
    ESP_LOGI(TAG, "Starting scan, disconnecting WiFi...");

    // Disconnect WiFi but keep stack alive
    esp_wifi_disconnect();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_callback);

    state_ = State::SCANNING;
    state_start_ = millis();
    ESP_LOGI(TAG, "Scanning...");
}

void WiFiScannerComponent::stop_scan_() {
    ESP_LOGI(TAG, "Scan done, found %d devices, reconnecting...", 
             devices_.size());

    esp_wifi_set_promiscuous(false);
    reconnect_();
}

void WiFiScannerComponent::reconnect_() {
    esp_wifi_connect();
    state_ = State::RECONNECTING;
    state_start_ = millis();
}

void WiFiScannerComponent::report_devices_() {
    ESP_LOGI(TAG, "=== Scan Results ===");
    for (auto &kv : devices_) {
        ESP_LOGI(TAG, "MAC: %s  RSSI: %d  Last seen: %dms ago",
                 kv.second.mac.c_str(),
                 kv.second.rssi,
                 millis() - kv.second.last_seen);
        on_device_found_.call(kv.second.mac, kv.second.rssi);
    }
    ESP_LOGI(TAG, "====================");
    devices_.clear();
}

void WiFiScannerComponent::promiscuous_callback(
        void *buf, wifi_promiscuous_pkt_type_t type) {

    if (type != WIFI_PKT_MGMT && type != WIFI_PKT_DATA) return;

    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *payload = pkt->payload;
    int rssi = pkt->rx_ctrl.rssi;

    // Filter weak signals (far away / noise)
    if (rssi < -90) return;

    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             payload[10], payload[11], payload[12],
             payload[13], payload[14], payload[15]);

    std::string mac_str(mac);

    // Skip broadcast and multicast MACs
    if (mac_str == "FF:FF:FF:FF:FF:FF") return;
    if (payload[10] & 0x01) return;

    auto &dev = instance_->devices_[mac_str];
    dev.mac = mac_str;
    dev.rssi = rssi;
    dev.last_seen = millis();
}

}  // namespace wifi_scanner
}  // namespace esphome