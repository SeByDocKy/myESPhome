#pragma once

#include "esphome/core/component.h"
#include "esphome/core/application.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/output/float_output.h"

#include <Hoymiles.h>
#include <Print.h>

namespace esphome {
namespace hm_hms_inverter {

#define MAX_PRINT_LEN 255

class EsphLogPrint : public Print {
    private:
        char buffer[MAX_PRINT_LEN + 1];
        uint16_t index = 0;
    public:
        size_t write(uint8_t value) override;
};

class HmHmsInverter;

class HmHmsButton : public button::Button, public Component {
 private:
  void press_action() override;
  HmHmsInverter *parent_;

 public:
   void set_parent(HmHmsInverter *parent) { this->parent_ = parent; }
};

class PercentFloatOutput : public output::FloatOutput, public Component  { 

 private:
   void write_state(float value) override;
   HmHmsInverter *parent_;
      
  public:
    void set_parent(HmHmsInverter *parent) { this->parent_ = parent; }
};

// class PalevelNumber : public esphome::number::Number, public Component {
//     private: 
//       int8_t current_palevel_;
//       esphome::CallbackManager<void(int8_t)> control_callback_;
//       ESPPreferenceObject pref_ = global_preferences->make_preference<int8_t>(this->get_object_id_hash());

//     public:
//       void setup() override;
//       void control(float value) override;   
//       void add_control_callback(std::function<void(float)> &&cb) { this->control_callback_.add(std::move(cb)); }
//       float get_palevel(void){return this->current_palevel_;}
//       void set_palevel(float value){this->current_palevel_ = value;}
// };

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


class HmHmsNumber : public esphome::number::Number {
    private:
        esphome::CallbackManager<void(float)> control_callback_;   
    public:
        void setup();
        void control(float value) override;
        void add_control_callback(std::function<void(float)> &&cb) { this->control_callback_.add(std::move(cb)); }
};

class HmHmsChannel : public esphome::Component {
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

class HmHmsInverter : public esphome::Component {
    private:
        uint64_t serial_;
        std::vector<HmHmsChannel*> channels_ = {};
        HmHmsChannel *inverter_channel_ = nullptr, *ac_channel_ = nullptr;

        PercentFloatOutput *limit_percent_output_ = nullptr;
        PercentNumber *limit_percent_number_ = nullptr;
        AbsoluteNumber *limit_absolute_number_ = nullptr;
        
        // PalevelNumber *palevel_number_ = nullptr;

        esphome::binary_sensor::BinarySensor *is_reachable_sensor_ = nullptr;
        esphome::binary_sensor::BinarySensor *is_producing_sensor_ = nullptr;
        esphome::sensor::Sensor *rssi_ = nullptr;
 
        std::shared_ptr<InverterAbstract> inverter_ = nullptr;
        // HoymilesRadio* radio_;
        std::unique_ptr<CMT2300A> cmt_radio_;
		std::unique_ptr<RF24> nrf_radio_;

        
        int8_t cmt_palevel_ = 20;
        int8_t nrf_palevel_ = 0;
        
        // int8_t current_palevel_;
        // int8_t former_palevel_ ;
        

        uint32_t system_conf_last_update_ = 0;
        uint32_t dev_info_last_update_ = 0;
        uint32_t stat_last_update_ = 0;

        bool first_ = true;
        bool active_ = true;

    public:
        void setup() override;

        void add_channel(HmHmsChannel* channel) { this->channels_.push_back(channel); }
        void set_ac_channel(HmHmsChannel* channel) { this->ac_channel_ = channel; }
        void set_inverter_channel(HmHmsChannel* channel) { this->inverter_channel_ = channel; }
        
        void set_limit_percent_output(PercentFloatOutput* output);
        void set_limit_percent_number(PercentNumber* number);
        void set_limit_absolute_number(AbsoluteNumber* number);
        
        // void set_palevel_number(PalevelNumber* number);

        void write_float(float value);

        void doretart();

        void set_cmt_palevel(int8_t value) {this->cmt_palevel_ = value;}
        int8_t get_cmt_palevel() {return this->cmt_palevel_ ;}
        void set_nrf_palevel(int8_t value) {this->nrf_palevel_ = value;}
        int8_t get_nrf_palevel() {return this->nrf_palevel_ ;}

        // void set_palevel(int8_t value) {this->current_palevel_ = value;}
        // int8_t get_palevel() {return this->current_palevel_ ;}
        // void set_oldpalevel(int8_t value) {this->former_palevel_ = value;}
        // int8_t get_oldpalevel() {return this->former_palevel_ ;}

       
        // void set_cmt_palevel(int8_t value) {this->current_cmt_palevel_ = value;}
        // int8_t get_cmt_palevel() {return this->current_cmt_palevel_ ;}
        // void set_cmt_oldpalevel(int8_t value) {this->former_cmt_palevel_ = value;}
        // int8_t get_cmt_oldpalevel() {return this->former_cmt_palevel_ ;}

        // void set_nrf_palevel(int8_t value) {this->current_nrf_palevel_ = value;}
        // int8_t get_nrf_palevel() {return this->current_nrf_palevel_ ;}
        // void set_nrf_oldpalevel(int8_t value) {this->former_nrf_palevel_ = value;}
        // int8_t get_nrf_oldpalevel() {return this->former_nrf_palevel_ ;}

        void set_is_reachable_sensor(esphome::binary_sensor::BinarySensor* sensor) { this->is_reachable_sensor_ = sensor; }
        void set_is_producing_sensor(esphome::binary_sensor::BinarySensor* sensor) { this->is_producing_sensor_ = sensor; }
        void set_serial_no(std::string serial) { this->serial_ = std::stoll(serial, nullptr, 16); }
        uint64_t serial() { return this->serial_; }
        void set_inverter(std::shared_ptr<InverterAbstract> inverter) { this->inverter_ = inverter; }
        void set_rssi (esphome::sensor::Sensor* sensor) { this->rssi_ = sensor; }        
        void loop() override;

        void updateConfiguration(bool connected, SystemConfigParaParser* parser);
};

class HmHmsPlatform : public esphome::PollingComponent {
    private:
        HoymilesClass* hoymiles_ = nullptr;
        std::vector<HmHmsInverter*> inverters_ = {};

        esphome::InternalGPIOPin* cmt_sdio_ = nullptr;
        esphome::InternalGPIOPin* cmt_clk_ = nullptr;
        esphome::InternalGPIOPin* cmt_cs_ = nullptr;
        esphome::InternalGPIOPin* cmt_fcs_ = nullptr;
        esphome::InternalGPIOPin* cmt_gpio2_ = nullptr;
        esphome::InternalGPIOPin* cmt_gpio3_ = nullptr;

        esphome::InternalGPIOPin* nrf_mosi_ = nullptr;
		esphome::InternalGPIOPin* nrf_miso_ = nullptr;
        esphome::InternalGPIOPin* nrf_clk_ = nullptr;
        esphome::InternalGPIOPin* nrf_cs_ = nullptr;
		esphome::InternalGPIOPin* nrf_en_ = nullptr;
		esphome::InternalGPIOPin* nrf_irq_ = nullptr;
     public:
        void setup() override;
        void update() override;
        void loop() override;
        void add_inverter(HmHmsInverter* inverter) { this->inverters_.push_back(inverter); }
 
        void set_cmt_sdio(esphome::InternalGPIOPin* pin) {this->cmt_sdio_ = pin;}
        void set_cmt_clk(esphome::InternalGPIOPin* pin) {this->cmt_clk_ = pin;}
        void set_cmt_cs(esphome::InternalGPIOPin* pin) {this->cmt_cs_ = pin;}
        void set_cmt_fcs(esphome::InternalGPIOPin* pin) {this->cmt_fcs_ = pin;}
        void set_cmt_gpio2(esphome::InternalGPIOPin* pin) {this->cmt_gpio2_ = pin;}
        void set_cmt_gpio3(esphome::InternalGPIOPin* pin) {this->cmt_gpio3_ = pin;}


        void set_nrf_mosi(esphome::InternalGPIOPin* pin) {this->nrf_mosi_ = pin;}
		void set_nrf_miso(esphome::InternalGPIOPin* pin) {this->nrf_miso_ = pin;}
        void set_nrf_clk(esphome::InternalGPIOPin* pin) {this->nrf_clk_ = pin;}
        void set_nrf_cs(esphome::InternalGPIOPin* pin) {this->nrf_cs_ = pin;}
		void set_nrf_en(esphome::InternalGPIOPin* pin) {this->nrf_en_ = pin;}
		void set_nrf_irq(esphome::InternalGPIOPin* pin) {this->nrf_irq_ = pin;}
};

}  // hm_hms_inverter

}  // esphome












