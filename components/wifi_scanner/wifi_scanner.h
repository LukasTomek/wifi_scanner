#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/ota/ota_component.h"
#include "esp_wifi.h"
#include <map>
#include <string>

namespace esphome {
namespace wifi_scanner {

struct DeviceInfo {
    std::string mac;
    int rssi;
    uint32_t last_seen;
};

class WiFiScannerComponent : public Component,
                             public uart::UARTDevice {
public:
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::LATE; }

    void set_report_interval(uint32_t ms) { report_interval_ = ms; }
    void set_rssi_threshold(int rssi) { rssi_threshold_ = rssi; }
    void set_ota_timeout(uint32_t ms) { ota_timeout_ = ms; }
    void set_wifi_ssid(const std::string &ssid) { wifi_ssid_ = ssid; }
    void set_wifi_password(const std::string &pass) { wifi_password_ = pass; }

private:
    enum class State {
        SCANNING,
        OTA_CONNECTING,     // connecting to WiFi for OTA
        OTA_READY,          // connected, OTA server running
        OTA_TIMEOUT,        // took too long, giving up
        OTA_RESUMING,       // disconnecting, back to scan
    };

    static void promiscuous_callback(void *buf, wifi_promiscuous_pkt_type_t type);
    static WiFiScannerComponent *instance_;

    void send_devices_over_uart_();
    void process_uart_command_();
    bool is_valid_mac_(uint8_t *payload);

    void start_ota_();
    void stop_ota_();
    bool connect_wifi_();

    std::map<std::string, DeviceInfo> devices_;
    std::string rx_buffer_;

    State state_{State::SCANNING};
    uint32_t state_start_{0};

    uint32_t report_interval_{30000};
    uint32_t last_report_{0};
    int rssi_threshold_{-90};

    uint32_t ota_timeout_{120000};  // 2 min OTA window
    std::string wifi_ssid_;
    std::string wifi_password_;
};

}  // namespace wifi_scanner
}  // namespace esphome