#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esp_wifi.h"
#include <set>
#include <map>
#include <string>

namespace esphome {
namespace wifi_scanner {

struct DeviceInfo {
    std::string mac;
    int rssi;
    uint32_t last_seen;
};

class WiFiScannerComponent : public Component {
public:
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::LATE; }

    void set_scan_duration(uint32_t ms) { scan_duration_ = ms; }
    void set_report_interval(uint32_t ms) { report_interval_ = ms; }
    void set_reconnect_wait(uint32_t ms) { reconnect_wait_ = ms; }

    void add_on_device_found_callback(std::function<void(std::string, int)> cb) {
        on_device_found_.add(std::move(cb));
    }

    int get_device_count() { return devices_.size(); }

private:
    enum class State {
        CONNECTED,      // Normal WiFi operation
        SCANNING,       // Promiscuous mode active
        RECONNECTING,   // Rejoining WiFi after scan
        STABILIZING,    // Wait after reconnect before reporting
        REPORTING,      // Sending results to HA
    };

    static void promiscuous_callback(void *buf, wifi_promiscuous_pkt_type_t type);
    static WiFiScannerComponent *instance_;

    void start_scan_();
    void stop_scan_();
    void reconnect_();
    void report_devices_();
    
    void set_stabilize_wait(uint32_t ms) { stabilize_wait_ = ms; }

    CallbackManager<void(std::string, int)> on_device_found_;
    std::map<std::string, DeviceInfo> devices_;

    State state_{State::CONNECTED};
    uint32_t state_start_{0};

    uint32_t stabilize_wait_{3000};     // 3s default
    uint32_t scan_duration_{5000};      // 5s scanning
    uint32_t report_interval_{60000};   // scan every 60s
    uint32_t reconnect_wait_{10000};    // 10s to reconnect
    uint32_t last_report_{0};
};

}  // namespace wifi_scanner
}  // namespace esphome