#include "wifi_scanner.h"
#include "esphome/core/log.h"

namespace esphome {
namespace wifi_scanner {

static const char *TAG = "wifi_scanner";

WiFiScannerComponent *WiFiScannerComponent::instance_ = nullptr;

void WiFiScannerComponent::setup() {
    instance_ = this;

    // No WiFi connection — raw promiscuous only
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_start();

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(&promiscuous_callback);

    // Scan all channels by setting channel to 0 (auto hop)
    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT |
                       WIFI_PROMIS_FILTER_MASK_DATA
    };
    esp_wifi_set_promiscuous_filter(&filter);

    ESP_LOGI(TAG, "Scanner ready, sending over UART");
    last_report_ = millis();
}

void WiFiScannerComponent::loop() {
    uint32_t now = millis();

    // Hop WiFi channels manually to catch all devices
    static uint8_t channel = 1;
    static uint32_t last_hop = 0;
    if (now - last_hop > 200) {  // hop every 200ms
        channel = (channel % 13) + 1;
        esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
        last_hop = now;
    }

    // Periodically send collected devices over UART
    if (now - last_report_ >= report_interval_) {
        send_devices_over_uart_();
        devices_.clear();
        last_report_ = now;
    }
}

void WiFiScannerComponent::send_devices_over_uart_() {
    if (devices_.empty()) {
        this->write_str("SCAN:0\n");
        return;
    }

    // Send count first
    char buf[64];
    snprintf(buf, sizeof(buf), "SCAN:%d\n", devices_.size());
    this->write_str(buf);

    // Send each device
    for (auto &kv : devices_) {
        snprintf(buf, sizeof(buf), "DEV:%s,%d\n",
                 kv.second.mac.c_str(),
                 kv.second.rssi);
        this->write_str(buf);
        delay(10);  // small gap so reporter can keep up
    }

    this->write_str("END\n");
    ESP_LOGI(TAG, "Sent %d devices over UART", devices_.size());
}

bool WiFiScannerComponent::is_valid_mac_(uint8_t *payload) {
    // Skip broadcast
    if (payload[10] == 0xFF) return false;
    // Skip multicast
    if (payload[10] & 0x01) return false;
    // Skip null MAC
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
    dev.rssi = rssi;  // update with latest rssi
    dev.last_seen = millis();
}

}  // namespace wifi_scanner
}  // namespace esphome