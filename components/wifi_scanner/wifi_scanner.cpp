#include "wifi_scanner.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esp_wifi.h"

namespace esphome {
namespace wifi_scanner {

static const char *TAG = "wifi_scanner";

WiFiScannerComponent *WiFiScannerComponent::instance_ = nullptr;

void WiFiScannerComponent::setup() {
    instance_ = this;

    // Start WiFi stack in null mode — no connection
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_start();

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_callback);

    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT |
                       WIFI_PROMIS_FILTER_MASK_DATA
    };
    esp_wifi_set_promiscuous_filter(&filter);

    ESP_LOGI(TAG, "Scanner ready");
    last_report_ = millis();
}

void WiFiScannerComponent::loop() {
    uint32_t now = millis();

    // Always check for incoming UART commands
    process_uart_command_();

    switch (state_) {

        case State::SCANNING: {
            // Channel hopping
            static uint8_t channel = 1;
            static uint32_t last_hop = 0;
            if (now - last_hop > 200) {
                channel = (channel % 13) + 1;
                esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
                last_hop = now;
            }

            // Periodic UART report
            if (now - last_report_ >= report_interval_) {
                send_devices_over_uart_();
                devices_.clear();
                last_report_ = now;
            }
            break;
        }

        case State::WIFI_CONNECTING:
            // Wait for ESPHome wifi component to connect
            if (wifi::global_wifi_component->is_connected()) {
                ESP_LOGI(TAG, "WiFi connected, OTA available");
                this->write_str("WIFI:READY\n");
                state_ = State::WIFI_ON;
            } else if (now - state_start_ > 30000) {
                ESP_LOGW(TAG, "WiFi connect timeout");
                this->write_str("WIFI:FAILED\n");
                stop_wifi_();
            }
            break;

        case State::WIFI_ON:
            // ESPHome handles everything — OTA, API, etc.
            // Just wait for CMD:WIFI_OFF
            break;
    }
}

void WiFiScannerComponent::process_uart_command_() {
    while (this->available()) {
        char c = this->read();
        if (c == '\n') {
            ESP_LOGI(TAG, "CMD: %s", rx_buffer_.c_str());

            if (rx_buffer_ == "CMD:WIFI_ON" &&
                state_ == State::SCANNING) {
                start_wifi_();

            } else if (rx_buffer_ == "CMD:WIFI_OFF" &&
                       state_ == State::WIFI_ON) {
                stop_wifi_();
            }

            rx_buffer_.clear();
        } else if (c != '\r') {
            rx_buffer_ += c;
        }
    }
}

void WiFiScannerComponent::start_wifi_() {
    ESP_LOGI(TAG, "Stopping scan, starting WiFi");
    esp_wifi_set_promiscuous(false);

    // Hand over to ESPHome wifi component
    wifi::global_wifi_component->start();

    state_ = State::WIFI_CONNECTING;
    state_start_ = millis();
    this->write_str("WIFI:CONNECTING\n");
}

void WiFiScannerComponent::stop_wifi_() {
    ESP_LOGI(TAG, "Stopping WiFi, resuming scan");

    wifi::global_wifi_component->disable();

    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_callback);

    this->write_str("WIFI:OFF\n");
    state_ = State::SCANNING;
    last_report_ = millis();
    ESP_LOGI(TAG, "Scanning resumed");
}

void WiFiScannerComponent::send_devices_over_uart_() {
    if (devices_.empty()) {
        this->write_str("SCAN:0\n");
        return;
    }

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
