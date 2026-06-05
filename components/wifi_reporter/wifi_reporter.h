#pragma once
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"
#include <vector>
#include <string>

namespace esphome {
namespace wifi_scanner {

struct ReportedDevice {
    std::string mac;
    int rssi;
};

class WiFiReporterComponent : public Component,
                              public uart::UARTDevice {
public:
    void setup() override;
    void loop() override;
    float get_setup_priority() const override { return setup_priority::LATE; }

    void add_on_device_found_callback(std::function<void(std::string, int)> cb) {
        on_device_found_.add(std::move(cb));
    }
    void add_on_scan_complete_callback(std::function<void(int)> cb) {
        on_scan_complete_.add(std::move(cb));
    }

private:
    void process_line_(const std::string &line);

    CallbackManager<void(std::string, int)> on_device_found_;
    CallbackManager<void(int)> on_scan_complete_;

    std::string rx_buffer_;
    int expected_devices_{0};
    int received_devices_{0};
};

}  // namespace wifi_scanner
}  // namespace esphome