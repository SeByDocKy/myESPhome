#pragma once

#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/output/float_output.h"

#include <Hoymiles.h>
#include <Print.h>

namespace esphome {
namespace hm_inverter {

#define MAX_PRINT_LEN 255

class EsphLogPrint : public Print {
    private:
        char buffer[MAX_PRINT_LEN + 1];
        uint16_t index = 0;
    public:
        size_t write(uint8_t value) override;
};

class HmInverter;

class HmButton : public button::Button, public Component {
 private:
  void press_action() override;
  HmInverter *parent_;

 public:
   void set_parent(HmInverter *parent) { this->parent_ = parent; }
};

class PercentFloatOutput : public output::FloatOutput, public Component  { 

 private:
   void write_state(float value) override;
   HmInverter *parent_;
      
  public:
    void set_parent(HmInverter *parent) { this->parent_ = parent; }
};

class PalevelNumber : public esphome::number::Number, public Component {
    private: 
      int8_t current_palevel_;
      esphome::CallbackManager<void(int8_t)> control_callback_;
      ESPPreferenceObject pref_ = global_preferences->make_preference<int8_t>(this->get_object_id_hash());

    public:
      void setup() override;
      void control(float value) override;   
      void add_control_callback(std::function<void(float)> &&cb) { this->control_callback_.add(std::move(cb)); }
      float get_palevel(void){return this->current_palevel_;}
      void set_palevel(float value){this->current_palevel_ = value;}
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


class HmNumber : public esphome::number::Number {
    private:
        esphome::CallbackManager<void(float)> control_callback_;   
    public:
        void setup();
        void control(float value) override;
        void add_control_callback(std::function<void(float)> &&cb) { this->control_callback_.add(std::move(cb)); }
};

class HmChannel : public esphome::Component {
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

class HmInverter : public esphome::Component {
    private:
        uint64_t serial_;
        std::vector<HmChannel*> channels_ = {};
        HmChannel *inverter_channel_ = nullptr, *ac_channel_ = nullptr;

        PercentFloatOutput *limit_percent_output_ = nullptr;
        PercentNumber *limit_percent_number_ = nullptr;
        AbsoluteNumber *limit_absolute_number_ = nullptr;
        PalevelNumber *palevel_number_ = nullptr;

        esphome::binary_sensor::BinarySensor *is_reachable_sensor_ = nullptr;
        esphome::sensor::Sensor *rssi_ = nullptr;
 
        std::shared_ptr<InverterAbstract> inverter_ = nullptr;
		std::unique_ptr<RF24> radio_;
        
		int8_t current_palevel_ = 0;
        int8_t former_palevel_ = -1;
        uint32_t system_conf_last_update_ = 0;
        uint32_t dev_info_last_update_ = 0;
        uint32_t stat_last_update_ = 0;

        bool first_ = true;
        bool active_ = true;

    public:
        void setup() override;

        void add_channel(HmChannel* channel) { this->channels_.push_back(channel); }
        void set_ac_channel(HmChannel* channel) { this->ac_channel_ = channel; }
        void set_inverter_channel(HmChannel* channel) { this->inverter_channel_ = channel; }
        
        void set_limit_percent_output(PercentFloatOutput* output);
        void set_limit_percent_number(PercentNumber* number);
        void set_limit_absolute_number(AbsoluteNumber* number);
        void set_palevel_number(PalevelNumber* number);

        void write_float(float value);

        void doretart();
        void set_palevel(int8_t value) {this->current_palevel_ = value;}
        int8_t get_palevel() {return this->current_palevel_ ;}
        void set_oldpalevel(int8_t value) {this->former_palevel_ = value;}
        int8_t get_oldpalevel() {return this->former_palevel_ ;}

        void set_is_reachable_sensor(esphome::binary_sensor::BinarySensor* sensor) { this->is_reachable_sensor_ = sensor; }
        void set_serial_no(std::string serial) { this->serial_ = std::stoll(serial, nullptr, 16); }
        uint64_t serial() { return this->serial_; }
        void set_inverter(std::shared_ptr<InverterAbstract> inverter) { this->inverter_ = inverter; }
        void set_rssi (esphome::sensor::Sensor* sensor) { this->rssi_ = sensor; }        
        void loop() override;

        void updateConfiguration(bool connected, SystemConfigParaParser* parser);
};

class HmPlatform : public esphome::PollingComponent {
    private:
        HoymilesClass* hoymiles_ = nullptr;
        HoymilesRadio *hoymilesradio_ = nullptr;
        std::vector<HmInverter*> inverters_ = {};
        esphome::InternalGPIOPin* mosi_ = nullptr;
		esphome::InternalGPIOPin* miso_ = nullptr;
        esphome::InternalGPIOPin* clk_ = nullptr;
        esphome::InternalGPIOPin* cs_ = nullptr;
		esphome::InternalGPIOPin* en_ = nullptr;
		esphome::InternalGPIOPin* irq_ = nullptr;
     public:
        void setup() override;
        void update() override;
        void loop() override;
        void add_inverter(HmInverter* inverter) { this->inverters_.push_back(inverter); }
        void set_mosi(esphome::InternalGPIOPin* pin) {this->mosi_ = pin;}
		void set_miso(esphome::InternalGPIOPin* pin) {this->miso_ = pin;}
        void set_clk(esphome::InternalGPIOPin* pin) {this->clk_ = pin;}
        void set_cs(esphome::InternalGPIOPin* pin) {this->cs_ = pin;}
		void set_en(esphome::InternalGPIOPin* pin) {this->en_ = pin;}
		void set_irq(esphome::InternalGPIOPin* pin) {this->irq_ = pin;}
};

}

}






