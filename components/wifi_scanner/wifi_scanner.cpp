#include "wifi_scanner.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wifi_scanner {

static const char *TAG = "wifi_scanner";

WiFiScannerComponent *WiFiScannerComponent::instance_ = nullptr;

void WiFiScannerComponent::setup() {
    instance_ = this;
    // WiFi is managed by ESPHome normally at boot
    // Nothing to init here — just wait for commands
    ESP_LOGI(TAG, "Scanner ready, WiFi mode (sniff off)");
}

void WiFiScannerComponent::loop() {
    uint32_t now = millis();

    process_uart_command_();

    switch (state_) {

        case State::WIFI_ON:
            // ESPHome handles WiFi, OTA, API normally
            break;

        case State::SNIFFING: {
            // Channel hopping
            static uint8_t channel = 1;
            static uint32_t last_hop = 0;
            if (now - last_hop > 200) {
                channel = (channel % 13) + 1;
                esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
                last_hop = now;
            }

            // Periodic report over UART
            if (now - last_report_ >= report_interval_) {
                send_devices_over_uart_();
                devices_.clear();
                last_report_ = now;
            }
            break;
        }

        case State::RECONNECTING:
            if (wifi::global_wifi_component->is_connected()) {
                ESP_LOGI(TAG, "WiFi reconnected");
                this->write_str("WIFI:READY\n");
                state_ = State::WIFI_ON;
            } else if (now - state_start_ > 30000) {
                ESP_LOGW(TAG, "Reconnect timeout, retrying");
                wifi::global_wifi_component->start();
                state_start_ = now;
            }
            break;
    }
}

void WiFiScannerComponent::process_uart_command_() {
    while (this->available()) {
        char c = this->read();
        if (c == '\n') {
            ESP_LOGI(TAG, "CMD: %s", rx_buffer_.c_str());

            if (rx_buffer_ == "CMD:SNIFF_ON" &&
                state_ == State::WIFI_ON) {
                start_sniff_();

            } else if (rx_buffer_ == "CMD:SNIFF_OFF" &&
                       state_ == State::SNIFFING) {
                stop_sniff_();
            }

            rx_buffer_.clear();
        } else if (c != '\r') {
            rx_buffer_ += c;
        }
    }
}

void WiFiScannerComponent::start_sniff_() {
    ESP_LOGI(TAG, "Starting sniff, disconnecting WiFi");

    // Send remaining devices before going dark
    if (!devices_.empty()) {
        send_devices_over_uart_();
        devices_.clear();
    }

    wifi::global_wifi_component->disable();

    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_callback);

    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT |
                       WIFI_PROMIS_FILTER_MASK_DATA
    };
    esp_wifi_set_promiscuous_filter(&filter);

    state_ = State::SNIFFING;
    last_report_ = millis();
    this->write_str("SNIFF:ON\n");
    ESP_LOGI(TAG, "Sniffing started");
}

void WiFiScannerComponent::stop_sniff_() {
    ESP_LOGI(TAG, "Stopping sniff, reconnecting WiFi");

    // Send final batch before reconnecting
    if (!devices_.empty()) {
        send_devices_over_uart_();
        devices_.clear();
    }

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_mode(WIFI_MODE_NULL);

    wifi::global_wifi_component->start();

    state_ = State::RECONNECTING;
    state_start_ = millis();
    this->write_str("SNIFF:OFF\n");
}

void WiFiScannerComponent::send_devices_over_uart_() {
    char buf[64];
    snprintf(buf, sizeof(buf), "SCAN:%d\n", devices_.size());
    this->write_str(buf);

    for (auto &kv : devices_) {
        snprintf(buf, sizeof(buf), "DEV:%s,%d\n",
                 kv.second.mac.c_str(),
                 kv.second.rssi);
        this->write_str(buf);
        delay(10);
    }

    this->write_str("END\n");
    ESP_LOGI(TAG, "Sent %d devices", devices_.size());
}

bool WiFiScannerComponent::is_valid_mac_(uint8_t *payload) {
    if (payload[10] == 0xFF) return false;
    if (payload[10] & 0x01) return false;
    if (payload[10] == 0x00 && payload[11] == 0x00 &&
        payload[12] == 0x00 && payload[13] == 0x00 &&
        payload[14] == 0x00 && payload[15] == 0x00) return false;
    return true;
}

void WiFiScannerComponent::promiscuous_callback(
        void *buf, wifi_promiscuous_pkt_type_t type) {

    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;
    uint8_t *payload = pkt->payload;
    int rssi = pkt->rx_ctrl.rssi;

    if (rssi < instance_->rssi_threshold_) return;
    if (!instance_->is_valid_mac_(payload)) return;

    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             payload[10], payload[11], payload[12],
             payload[13], payload[14], payload[15]);

    std::string mac_str(mac);
    auto &dev = instance_->devices_[mac_str];
    dev.mac = mac_str;
    dev.rssi = rssi;
    dev.last_seen = millis();
}

}  // namespace wifi_scanner
}  // namespace esphome