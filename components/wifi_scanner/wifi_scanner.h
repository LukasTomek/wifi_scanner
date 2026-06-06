#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/wifi/wifi_component.h"
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

private:
    enum class State {
        WIFI_ON,        // default — WiFi connected, OTA available
        SNIFFING,       // promiscuous mode, WiFi off
        RECONNECTING,   // returning to WiFi after sniff
    };

    static void promiscuous_callback(void *buf, wifi_promiscuous_pkt_type_t type);
    static WiFiScannerComponent *instance_;

    void start_sniff_();
    void stop_sniff_();
    void send_devices_over_uart_();
    void process_uart_command_();
    bool is_valid_mac_(uint8_t *payload);

    std::map<std::string, DeviceInfo> devices_;
    std::string rx_buffer_;

    State state_{State::WIFI_ON};
    uint32_t state_start_{0};

    uint32_t report_interval_{30000};
    uint32_t last_report_{0};
    int rssi_threshold_{-90};
};

}  // namespace wifi_scanner
}  // namespace esphome