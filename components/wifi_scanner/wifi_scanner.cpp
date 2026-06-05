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

        case State::OTA_CONNECTING:
            if (connect_wifi_()) {
                ESP_LOGI(TAG, "WiFi connected, OTA ready");
                this->write_str("OTA:READY\n");
                state_ = State::OTA_READY;
                state_start_ = now;
            } else if (now - state_start_ >= 15000) {
                // 15s connect timeout
                ESP_LOGW(TAG, "WiFi connect failed");
                this->write_str("OTA:FAILED\n");
                state_ = State::OTA_RESUMING;
                state_start_ = now;
            }
            break;

        case State::OTA_READY:
            // Let ESPHome OTA component do its work
            App.loop();

            // Timeout — no update came
            if (now - state_start_ >= ota_timeout_) {
                ESP_LOGW(TAG, "OTA timeout, resuming scan");
                this->write_str("OTA:TIMEOUT\n");
                state_ = State::OTA_RESUMING;
                state_start_ = now;
            }
            break;

        case State::OTA_TIMEOUT:
            stop_ota_();
            break;

        case State::OTA_RESUMING:
            stop_ota_();
            break;
    }
}

void WiFiScannerComponent::process_uart_command_() {
    while (this->available()) {
        char c = this->read();
        if (c == '\n') {
            ESP_LOGI(TAG, "UART cmd: %s", rx_buffer_.c_str());

            if (rx_buffer_ == "CMD:OTA" && state_ == State::SCANNING) {
                start_ota_();
            } else if (rx_buffer_ == "CMD:CANCEL_OTA") {
                if (state_ == State::OTA_READY ||
                    state_ == State::OTA_CONNECTING) {
                    ESP_LOGI(TAG, "OTA cancelled by reporter");
                    this->write_str("OTA:CANCELLED\n");
                    state_ = State::OTA_RESUMING;
                    state_start_ = millis();
                }
            }

            rx_buffer_.clear();
        } else if (c != '\r') {
            rx_buffer_ += c;
        }
    }
}

void WiFiScannerComponent::start_ota_() {
    ESP_LOGI(TAG, "OTA requested, stopping scan");

    esp_wifi_set_promiscuous(false);
    esp_wifi_set_mode(WIFI_MODE_STA);

    state_ = State::OTA_CONNECTING;
    state_start_ = millis();
}

void WiFiScannerComponent::stop_ota_() {
    ESP_LOGI(TAG, "Resuming scan mode");

    esp_wifi_disconnect();
    esp_wifi_set_mode(WIFI_MODE_NULL);

    delay(500);

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_callback);

    this->write_str("OTA:DONE\n");

    state_ = State::SCANNING;
    last_report_ = millis();
    ESP_LOGI(TAG, "Scanning resumed");
}

bool WiFiScannerComponent::connect_wifi_() {
    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid,
            wifi_ssid_.c_str(), sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password,
            wifi_password_.c_str(), sizeof(wifi_config.sta.password));

    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_connect();

    // Check connection status
    wifi_ap_record_t ap_info;
    return esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK;
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