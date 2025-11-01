#include "hm_hms_inverter.h"


#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
#define SPI_NRF HSPI
#elif CONFIG_IDF_TARGET_ESP32C3
#define SPI_NRF FSPI
#else
#define SPI_NRF VSPI
#endif

namespace esphome {
namespace hm_hms_inverter {

#define TAG "HM/HMS"

size_t EsphLogPrint::write(uint8_t value) {
    if (value == 10) return 1; // Skip new line
    if (this->index < MAX_PRINT_LEN) {
        if (value != 13) {
            buffer[index++] = value;
            return 1;
        }
    }
    buffer[index] = 0;
    ESP_LOGD(TAG, "HM/HMS: %s", (char *)&buffer[0]);
    index = 0;
    return 1;
}

void HmHmsButton::press_action(){
    this->parent_->doretart();
}

void PercentFloatOutput::write_state(float value){
     this->parent_->write_float(value);
}

void PalevelNumber::setup(){
    
}
void PalevelNumber::control(float value){
    this->publish_state(value);
    this->control_callback_.call(value);
    //this->parent_->set_palevel(value);
}


void PercentNumber::setup() {
    float value;
    this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    if (!this->pref_.load(&value)) value = this->get_percent_power();
    this->set_percent_power(value);
    this->publish_state(value);
}
void PercentNumber::control(float value) {
    this->publish_state(value);
    this->control_callback_.call(value);
}

void AbsoluteNumber::setup() {
    float value;
    this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    if (!this->pref_.load(&value)) value = this->get_absolute_power();
    this->set_absolute_power(value);
    this->publish_state(value);
}
void AbsoluteNumber::control(float value) {
    this->publish_state(value);
    this->control_callback_.call(value);
}

void HmHmsNumber::control(float value) {
    this->publish_state(value);
    this->control_callback_.call(value);    
}

void HmHmsInverter::setup() {
}

void HmHmsInverter::doretart(){
   this->inverter_->sendRestartControlRequest();
   ESP_LOGI("Inverter" , "restart button pressed");
}

// void HmsInverter::set_palevel(float value){
//    // auto radio = this->inverter_->get_radio();
//    // this->inverter_->get_radio()->setPALevel(value);
//     this->inverter_->current_palevel_ = value;
// }

void HmHmsInverter::write_float(float value){
     if (value != NULL){ //NAN
       this->inverter_->sendActivePowerControlRequest(value*100, PowerLimitControlType::RelativNonPersistent);
       this->active_ = true;
     }
}

void HmHmsInverter::set_palevel_number(PalevelNumber* number) {    
    this->palevel_number_ = number;
    number->add_control_callback([this](int8_t value) {
        if (this->inverter_ != nullptr) {
            ESP_LOGI(TAG, "set_palevel_number(): New palevel: %.0d", value);
            this->radio_->setPALevel(value);
            this->set_palevel(value);
        }
    });
}


void HmHmsInverter::set_limit_percent_number(PercentNumber* number) {    
    this->limit_percent_number_ = number;
    number->add_control_callback([this](float value) {
        if (this->inverter_ != nullptr) {
            ESP_LOGI(TAG, "set_limit_percent_number(): New limit percent: %.0f", value);
            this->inverter_->sendActivePowerControlRequest(value, PowerLimitControlType::RelativNonPersistent);
            // this->inverter_->sendActivePowerControlRequest(value, PowerLimitControlType::RelativPersistent);   
        }
    });
}

void HmHmsInverter::set_limit_absolute_number(AbsoluteNumber* number) {     
    this->limit_absolute_number_ = number;
    number->add_control_callback([this](float value) {
        if (this->inverter_ != nullptr) {
            ESP_LOGI(TAG, "set_limit_absolute_number(): New limit absolute: %.0f", value);
            this->inverter_->sendActivePowerControlRequest(value, PowerLimitControlType::AbsolutNonPersistent);
            // this->inverter_->sendActivePowerControlRequest(value, PowerLimitControlType::AbsolutPersistent);
        }
    });
}

void HmHmsInverter::loop() {
    if (this->inverter_ == nullptr) return;
    auto check_updated = [](Parser* parser, uint32_t value) {
        return (parser->getLastUpdate() > 0) && (parser->getLastUpdate() != value); 
    };
    if (is_reachable_sensor_ != nullptr) {
        is_reachable_sensor_->publish_state(this->inverter_->isReachable());
    }
    // if (check_updated(this->inverter_->DevInfo(), dev_info_last_update_)) {
    //     dev_info_last_update_ = this->inverter_->DevInfo()->getLastUpdate();
    //     ESP_LOGD(TAG, "loop(): DevInfo updated: %s, %d, %d", this->inverter_->DevInfo()->getHwVersion(), this->inverter_->isProducing(), this->inverter_->isReachable());
    // }
    if (check_updated(this->inverter_->Statistics(), stat_last_update_)) {
        stat_last_update_ = this->inverter_->Statistics()->getLastUpdate();
        auto dc_channels = this->inverter_->Statistics()->getChannelsByType(ChannelType_t::TYPE_DC);
        auto ac_channels = this->inverter_->Statistics()->getChannelsByType(ChannelType_t::TYPE_AC);
        auto inv_channels = this->inverter_->Statistics()->getChannelsByType(ChannelType_t::TYPE_INV);
        uint8_t i = 0;
        for (auto &it : dc_channels) {
            if (this->channels_.size() > i) {
                this->channels_[i++]->updateSensors(
                    this->inverter_->isProducing(), 
                    this->inverter_->Statistics(), 
                    ChannelType_t::TYPE_DC, 
                    it);
            }
        }
        if ((ac_channels.size() > 0) && (this->ac_channel_ != nullptr)) {
            this->ac_channel_->updateSensors(
                this->inverter_->isProducing(), 
                this->inverter_->Statistics(), 
                ChannelType_t::TYPE_AC,
                ac_channels.front());
        }
        if ((inv_channels.size() > 0) && (this->inverter_channel_ != nullptr)) {
            this->inverter_channel_->updateSensors(
                this->inverter_->isProducing(), 
                this->inverter_->Statistics(), 
                ChannelType_t::TYPE_INV,
                inv_channels.front());
        }
        
       
        // if (rssi_ !=nullptr){
        //     rssi_->publish_state(this->radio_->getRssiDBm());
        //     ESP_LOGVV("RADIO", "NRF24 RSSI %d" , this->radio_->getRssiCode());
        // }
    }

        
    // if (this->get_oldpalevel() != this->get_palevel()){
    //     // this->palevel_number_->publish_state(this->get_palevel());
    //     this->radio_->setPALevel(this->get_palevel());
    //     ESP_LOGI("HM" , "Set PALevel: %d dBm" , this->get_palevel());
    //     this->set_oldpalevel(this->get_palevel());
    // }   
    
   if (this->first_ && this->inverter_->isReachable()){

     // if (this->palevel_number_ != nullptr) {
     //   int8_t level = this->get_palevel();
     //   this->palevel_number_->publish_state(level);
     // }
     if (this->limit_percent_number_ != nullptr) {
        float percent = this->inverter_->SystemConfigPara()->getLimitPercent();
        this->limit_percent_number_->publish_state(percent);
     }
     if (this->limit_absolute_number_ != nullptr) {
        auto max_power = this->inverter_->DevInfo()->getMaxPower();
         this->limit_absolute_number_->publish_state(max_power);
      }
     this->first_ = false;
   }
    
    if (check_updated(this->inverter_->SystemConfigPara(), system_conf_last_update_)) {
        system_conf_last_update_ = this->inverter_->SystemConfigPara()->getLastUpdate();
        updateConfiguration(this->inverter_->isProducing(), this->inverter_->SystemConfigPara());
        ESP_LOGVV(TAG, "loop(): DevInfo updated: %s, %d, %d", this->inverter_->DevInfo()->getHwVersion(), this->inverter_->isProducing(), this->inverter_->isReachable());
    }
}

void HmHmsInverter::updateConfiguration(bool connected, SystemConfigParaParser* parser) {
    ESP_LOGD(TAG, "updateConfiguration(): SystemConfigPara() updated");
    float percent = parser->getLimitPercent();
    ESP_LOGI(TAG, "updateConfiguration(): Limit percent received: %.0f", percent);
    if (limit_percent_number_ != nullptr) {
        //limit_percent_number_->publish_state(connected? percent: NAN);
        limit_percent_number_->publish_state(percent);
    }
    if (limit_absolute_number_ != nullptr) {
        auto max_power = this->inverter_->DevInfo()->getMaxPower();
        // limit_absolute_number_->publish_state((connected && (max_power > 0))? percent * max_power / 100.0: NAN);
        limit_absolute_number_->publish_state(percent * max_power / 100.0);
    }
}

void HmHmsChannel::setup() {
}

void HmHmsChannel::updateSensors(bool connected, StatisticsParser* stat, ChannelType_t typ, ChannelNum_t num) {
    if (this->power_ != nullptr) {
        auto field = typ == ChannelType_t::TYPE_AC? FieldId_t::FLD_PAC: FieldId_t::FLD_PDC;
        this->power_->publish_state(connected? stat->getChannelFieldValue(typ, num, field): 0.0);
    }
    if (this->energy_ != nullptr) {
        auto field = FieldId_t::FLD_YD;
        this->energy_->publish_state(connected? stat->getChannelFieldValue(typ, num, field): NAN);
    }
    if (this->voltage_ != nullptr) {
        auto field = typ == ChannelType_t::TYPE_AC? FieldId_t::FLD_UAC: FieldId_t::FLD_UDC;
        this->voltage_->publish_state(connected? stat->getChannelFieldValue(typ, num, field): 0.0);
    }
    if (this->current_ != nullptr) {
        auto field = typ == ChannelType_t::TYPE_AC? FieldId_t::FLD_IAC: FieldId_t::FLD_IDC;
        this->current_->publish_state(connected? stat->getChannelFieldValue(typ, num, field): 0.0);
    }
    if (this->temperature_ != nullptr) {
        auto field = FieldId_t::FLD_T;
        this->temperature_->publish_state(stat->getChannelFieldValue(typ, num, field));
    }
}


void HmHmsPlatform::setup() {
    ESP_LOGI(TAG, "set_pins(): Setting up HM/HMS instance");
    const int8_t nrf_mosi=this->nrf_mosi_->get_pin();
	const int8_t nrf_miso=this->nrf_miso_->get_pin();
    const int8_t nrf_clk=this->nrf_clk_->get_pin();
    const int8_t nrf_cs=this->nrf_cs_->get_pin();

    ESP_LOGI(TAG, "NRF: nrf_mosi:%d, nrf_miso:%d, nrf_clk:%d, nrf_cs:%d",nrf_mosi,nrf_miso,nrf_clk,nrf_cs);
    
    this->hoymiles_ = &Hoymiles;
    Hoymiles.setMessageOutput(new EsphLogPrint());

    this->hoymiles_->init();
	
/*     this->hoymiles_->init();
    this->hoymiles_->initCMT(sdio, clk, cs, fcs, gpio2, gpio3);
 */	

	
	SPIClass* spiClass = new SPIClass(SPI_NRF);
    spiClass->begin(pin.nrf24_clk, pin.nrf24_miso, pin.nrf24_mosi, pin.nrf24_cs);
    this->hoymiles_->initNRF(spiClass, pin.nrf24_en, pin.nrf24_irq);

    //  Original part //
    
    for (uint8_t i = 0; i < this->inverters_.size(); i++) {
        auto inv = this->inverters_[i];
        auto name = "Inv_" + std::to_string(i);
        auto invp = this->hoymiles_->addInverter(name.c_str(), inv->serial());
        
        if (invp != nullptr) {
            inv->set_inverter(invp);
            ESP_LOGI(TAG, "Added inverter model: %s", invp->typeName().c_str());
            invp->init();
        } else {
            ESP_LOGW(TAG, "Invalid inverter serial# %" PRIu64, inv->serial());
        }
    }
}

void HmPlatform::update() {
    ESP_LOGD(TAG, "update(): Polling inverters");
    this->hoymiles_->loop();
}

void HmPlatform::loop() {
    this->hoymiles_->getRadioCmt()->loop();
}

}

}
