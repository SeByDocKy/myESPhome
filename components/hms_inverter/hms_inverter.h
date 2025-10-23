#pragma once

#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/output/float_output.h"

#include <Hoymiles.h>
#include <Print.h>

namespace esphome {
namespace hms_inverter {

#define MAX_PRINT_LEN 255

class EsphLogPrint : public Print {
    private:
        char buffer[MAX_PRINT_LEN + 1];
        uint16_t index = 0;
    public:
        size_t write(uint8_t value) override;
};

class HmsInverter;

class HmsButton : public button::Button, public Component {
 private:
  void press_action() override;
  HmsInverter *parent_;

 public:
   void set_parent(HmsInverter *parent) { this->parent_ = parent; }
};


class PalevelNumber : public esphome::number::Number, public Component {
    private: 
        // ESPPreferenceObject pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
        // float current_palevel_ = 20;
        void control(float value) override;
        HmsInverter *parent_;

    public:
      void set_parent(HmsInverter *parent) { this->parent_ = parent; }
};


class PercentFloatOutput : public output::FloatOutput, public Component  { 

 private:
   void write_state(float value) override;
   HmsInverter *parent_;
      
  public:
    void set_parent(HmsInverter *parent) { this->parent_ = parent; }
};

class PercentNumber : public esphome::number::Number, public Component {
    private:
        esphome::CallbackManager<void(float)> control_callback_;
       
        ESPPreferenceObject pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
        float current_percent_power_limit_ = 100.0;
        

    public:
        void setup() override;
        void control(float value) override;
        
        void add_control_callback(std::function<void(float)> &&cb) { this->control_callback_.add(std::move(cb)); }
        float get_percent_power(void){return this->current_percent_power_limit_;}
        void set_percent_power(float value){this->current_percent_power_limit_ = value;}
};

class AbsoluteNumber : public esphome::number::Number, public Component {
    private:
        esphome::CallbackManager<void(float)> control_callback_;
       
        ESPPreferenceObject pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
        float current_absolute_power_limit_ = 1000.0;
        

    public:
        void setup() override;
        void control(float value) override;
        void add_control_callback(std::function<void(float)> &&cb) { this->control_callback_.add(std::move(cb)); }
        float get_absolute_power(void){return this->current_absolute_power_limit_;}
        void set_absolute_power(float value){this->current_absolute_power_limit_ = value;}
};



class HmsNumber : public esphome::number::Number {
    private:
        esphome::CallbackManager<void(float)> control_callback_;   
    public:
        void setup();
        void control(float value) override;
        void add_control_callback(std::function<void(float)> &&cb) { this->control_callback_.add(std::move(cb)); }

};

class HmsChannel : public esphome::Component {
    private:
        esphome::sensor::Sensor *power_ = nullptr, *energy_ = nullptr, *voltage_ = nullptr, *current_ = nullptr;
        esphome::sensor::Sensor *temperature_ = nullptr;
    public:
        void set_power_sensor(esphome::sensor::Sensor* sensor) { this->power_ = sensor; }
        void set_energy_sensor(esphome::sensor::Sensor* sensor) { this->energy_ = sensor; }
        void set_voltage_sensor(esphome::sensor::Sensor* sensor) { this->voltage_ = sensor; }
        void set_current_sensor(esphome::sensor::Sensor* sensor) { this->current_ = sensor; }
        void set_temperature(esphome::sensor::Sensor* sensor) { this->temperature_ = sensor; } 
        

        void setup() override;
        void updateSensors(bool connected, StatisticsParser* stat, ChannelType_t typ, ChannelNum_t num);
};

class HmsInverter : public esphome::Component {
    private:
        uint64_t serial_;
        std::vector<HmsChannel*> channels_ = {};
        HmsChannel *inverter_channel_ = nullptr, *ac_channel_ = nullptr;

        PercentFloatOutput *limit_percent_output_ = nullptr;
        PercentNumber *limit_percent_number_ = nullptr;
        AbsoluteNumber *limit_absolute_number_ = nullptr;

        esphome::binary_sensor::BinarySensor *is_reachable_sensor_ = nullptr;
        esphome::sensor::Sensor *rssi_ = nullptr;
 
        std::shared_ptr<InverterAbstract> inverter_ = nullptr;
        std::unique_ptr<CMT2300A> radio_;

        uint32_t system_conf_last_update_ = 0;
        uint32_t dev_info_last_update_ = 0;
        uint32_t stat_last_update_ = 0;

        bool first_ = true;
        bool active_ = true;

    public:
        void setup() override;

        void add_channel(HmsChannel* channel) { this->channels_.push_back(channel); }
        void set_ac_channel(HmsChannel* channel) { this->ac_channel_ = channel; }
        void set_inverter_channel(HmsChannel* channel) { this->inverter_channel_ = channel; }
        
        void set_limit_percent_output(PercentFloatOutput* output);
        void set_limit_percent_number(PercentNumber* number);
        void set_limit_absolute_number(AbsoluteNumber* number);

        void write_float(float value);

        void doretart();
        void set_Palevel(float value);

        void set_is_reachable_sensor(esphome::binary_sensor::BinarySensor* sensor) { this->is_reachable_sensor_ = sensor; }
        void set_serial_no(std::string serial) { this->serial_ = std::stoll(serial, nullptr, 16); }
        uint64_t serial() { return this->serial_; }
        void set_inverter(std::shared_ptr<InverterAbstract> inverter) { this->inverter_ = inverter; }
        void set_rssi (esphome::sensor::Sensor* sensor) { this->rssi_ = sensor; }
        // void set_output_percent (esphome::output::FloatOutput *output) {this->percent_output_ = output; }
        
        void loop() override;

        void updateConfiguration(bool connected, SystemConfigParaParser* parser);
};

class HmsPlatform : public esphome::PollingComponent {
    private:
        HoymilesClass* hoymiles_ = nullptr;
        std::vector<HmsInverter*> inverters_ = {};
        esphome::InternalGPIOPin* sdio_ = nullptr;
        esphome::InternalGPIOPin* clk_ = nullptr;
        esphome::InternalGPIOPin* cs_ = nullptr;
        esphome::InternalGPIOPin* fcs_ = nullptr;
        esphome::InternalGPIOPin* gpio2_ = nullptr;
        esphome::InternalGPIOPin* gpio3_ = nullptr;
    public:
        void setup() override;
        void update() override;
        void loop() override;
        void add_inverter(HmsInverter* inverter) { this->inverters_.push_back(inverter); }
        void set_sdio(esphome::InternalGPIOPin* pin) {this->sdio_ = pin;}
        void set_clk(esphome::InternalGPIOPin* pin) {this->clk_ = pin;}
        void set_cs(esphome::InternalGPIOPin* pin) {this->cs_ = pin;}
        void set_fcs(esphome::InternalGPIOPin* pin) {this->fcs_ = pin;}
        void set_gpio2(esphome::InternalGPIOPin* pin) {this->gpio2_ = pin;}
        void set_gpio3(esphome::InternalGPIOPin* pin) {this->gpio3_ = pin;}

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
