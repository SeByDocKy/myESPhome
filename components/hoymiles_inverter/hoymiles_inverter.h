#pragma once

#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include <Hoymiles.h>
#include <Print.h>


namespace esphome {
namespace hoymiles_inverter {

#define MAX_PRINT_LEN 255

class EsphLogPrint : public Print {
    private:
        char buffer[MAX_PRINT_LEN + 1];
        uint16_t index = 0;
    public:
        size_t write(uint8_t value) override;
};

class HoymilesNumber : public esphome::number::Number {
    private:
        esphome::CallbackManager<void(float)> control_callback_;
    public:
        void control(float value) override;
        void add_control_callback(std::function<void(float)> &&cb) { this->control_callback_.add(std::move(cb)); }
};

class HoymilesChannel : public esphome::Component {
    private:
        esphome::sensor::Sensor *power_ = nullptr, *energy_ = nullptr, *voltage_ = nullptr, *current_ = nullptr;
    public:
        void set_power_sensor(esphome::sensor::Sensor* sensor) { this->power_ = sensor; }
        void set_energy_sensor(esphome::sensor::Sensor* sensor) { this->energy_ = sensor; }
        void set_voltage_sensor(esphome::sensor::Sensor* sensor) { this->voltage_ = sensor; }
        void set_current_sensor(esphome::sensor::Sensor* sensor) { this->current_ = sensor; }

        void setup() override;
        void updateSensors(bool connected, StatisticsParser* stat, ChannelType_t typ, ChannelNum_t num);
};

class HoymilesInverter : public esphome::Component {
    private:
        uint64_t serial_;
        std::vector<HoymilesChannel*> channels_ = {};
        HoymilesChannel *inverter_channel_ = nullptr, *ac_channel_ = nullptr;

        HoymilesNumber *limit_percent_number_ = nullptr, *limit_absolute_number_ = nullptr;
        esphome::binary_sensor::BinarySensor *is_reachable_sensor_ = nullptr;

        std::shared_ptr<InverterAbstract> inverter_ = nullptr;

        uint32_t system_conf_last_update_ = 0;
        uint32_t dev_info_last_update_ = 0;
        uint32_t stat_last_update_ = 0;
    public:
        void setup() override;

        void add_channel(HoymilesChannel* channel) { this->channels_.push_back(channel); }
        void set_ac_channel(HoymilesChannel* channel) { this->ac_channel_ = channel; }
        void set_inverter_channel(HoymilesChannel* channel) { this->inverter_channel_ = channel; }
        void set_limit_percent_number(HoymilesNumber* number);
        void set_limit_absolute_number(HoymilesNumber* number);
        void set_is_reachable_sensor(esphome::binary_sensor::BinarySensor* sensor) { this->is_reachable_sensor_ = sensor; }
        
        void set_serial_no(std::string serial) { this->serial_ = std::stoll(serial, nullptr, 16); }
        // void set_serial_no(std::string serial) { this->serial_ = serial; }
        uint64_t serial() { return this->serial_; }
        void set_inverter(std::shared_ptr<InverterAbstract> inverter) { this->inverter_ = inverter; }
        void loop() override;

        void updateConfiguration(bool connected, SystemConfigParaParser* parser);
};

class HoymilesPlatform : public esphome::PollingComponent {
    private:
        HoymilesClass* hoymiles_ = nullptr;
        std::vector<HoymilesInverter*> inverters_ = {};
    public:
        void setup() override;
        void update() override;
        void loop() override;
        void add_inverter(HoymilesInverter* inverter) { this->inverters_.push_back(inverter); }
        void set_pins(
            esphome::InternalGPIOPin* sdio,
            esphome::InternalGPIOPin* clk,
            esphome::InternalGPIOPin* cs,
            esphome::InternalGPIOPin* fcs,
            esphome::InternalGPIOPin* gpio2,
            esphome::InternalGPIOPin* gpio3
        );
};

}
}
