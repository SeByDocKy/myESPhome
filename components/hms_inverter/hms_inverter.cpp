#include "hms_inverter.h"

namespace esphome {
namespace hms_inverter {

#define TAG "HMS"

size_t EsphLogPrint::write(uint8_t value) {
    if (value == 10) return 1; // Skip new line
    if (this->index < MAX_PRINT_LEN) {
        if (value != 13) {
            buffer[index++] = value;
            return 1;
        }
    }
    buffer[index] = 0;
    ESP_LOGD(TAG, "HMS: %s", (char *)&buffer[0]);
    index = 0;
    return 1;
}

void HmsButton::press_action(){
    this->parent_->doretart();
}


void PalevelNumber::control(float value){
    this->parent_->set_palevel(value);
}


void PercentFloatOutput::write_state(float value){
     this->parent_->write_float(value);
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

void HmsNumber::control(float value) {
    this->publish_state(value);
    this->control_callback_.call(value);    
}

void HmsInverter::setup() {
}

void HmsInverter::doretart(){
   this->inverter_->sendRestartControlRequest();
   ESP_LOGI("Inverter" , "restart button pressed");
}

// void HmsInverter::set_palevel(float value){
//    // auto radio = this->inverter_->get_radio();
//    // this->inverter_->get_radio()->setPALevel(value);
//     this->inverter_->current_palevel_ = value;
// }

void HmsInverter::write_float(float value){
     if (value != NULL){ //NAN
       this->inverter_->sendActivePowerControlRequest(value*100, PowerLimitControlType::RelativNonPersistent);
       this->active_ = true;
     }
}

void HmsInverter::set_limit_percent_number(PercentNumber* number) {    
    this->limit_percent_number_ = number;
    number->add_control_callback([this](float value) {
        if (this->inverter_ != nullptr) {
            ESP_LOGI(TAG, "set_limit_percent_number(): New limit percent: %.0f", value);
            this->inverter_->sendActivePowerControlRequest(value, PowerLimitControlType::RelativNonPersistent);
            // this->inverter_->sendActivePowerControlRequest(value, PowerLimitControlType::RelativPersistent);   
        }
    });
}

void HmsInverter::set_limit_absolute_number(AbsoluteNumber* number) {     
    this->limit_absolute_number_ = number;
    number->add_control_callback([this](float value) {
        if (this->inverter_ != nullptr) {
            ESP_LOGI(TAG, "set_limit_absolute_number(): New limit absolute: %.0f", value);
            this->inverter_->sendActivePowerControlRequest(value, PowerLimitControlType::AbsolutNonPersistent);
            // this->inverter_->sendActivePowerControlRequest(value, PowerLimitControlType::AbsolutPersistent);
        }
    });
}

void HmsInverter::loop() {
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
        ESP_LOGVV("RADIO", "RSSI code: %d" , radio_->getRssiCode());
       
        if (rssi_ !=nullptr){
            rssi_->publish_state(radio_->getRssiDBm());
        }
        // if (this->inverter_->get_oldpalevel() != this->inverter_->get_palevel()){
        //       radio_->setPalevel(this->inverter_->get_palevel());
        //       this->inverter_->set_oldpalevel(this->inverter_->get_palevel());
        // }
    }

   if (this->first_ && this->inverter_->isReachable()){
     float percent = this->inverter_->SystemConfigPara()->getLimitPercent();
     if (limit_percent_number_ != nullptr) {
        limit_percent_number_->publish_state(percent);
        // limit_percent_number_->publish_state(100);
     }
     if (limit_absolute_number_ != nullptr) {
        auto max_power = this->inverter_->DevInfo()->getMaxPower();
        // limit_absolute_number_->publish_state((connected && (max_power > 0))? percent * max_power / 100.0: NAN);
        limit_absolute_number_->publish_state(max_power);
        // limit_absolute_number_->publish_state(1000); 
     }
     //updateConfiguration(true, this->inverter_->SystemConfigPara());
     this->first_ = false;
   }
    
    if (check_updated(this->inverter_->SystemConfigPara(), system_conf_last_update_)) {
        system_conf_last_update_ = this->inverter_->SystemConfigPara()->getLastUpdate();
        updateConfiguration(this->inverter_->isProducing(), this->inverter_->SystemConfigPara());
        ESP_LOGVV(TAG, "loop(): DevInfo updated: %s, %d, %d", this->inverter_->DevInfo()->getHwVersion(), this->inverter_->isProducing(), this->inverter_->isReachable());
    }
}

void HmsInverter::updateConfiguration(bool connected, SystemConfigParaParser* parser) {
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

void HmsChannel::setup() {
}

void HmsChannel::updateSensors(bool connected, StatisticsParser* stat, ChannelType_t typ, ChannelNum_t num) {
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


void HmsPlatform::set_pins(
    esphome::InternalGPIOPin* sdio,
    esphome::InternalGPIOPin* clk,
    esphome::InternalGPIOPin* cs,
    esphome::InternalGPIOPin* fcs,
    esphome::InternalGPIOPin* gpio2,
    esphome::InternalGPIOPin* gpio3
) {
    ESP_LOGI(TAG, "set_pins(): Setting up HMS instance");
    this->hoymiles_ = &Hoymiles;
    Hoymiles.setMessageOutput(new EsphLogPrint());
    this->hoymiles_->init();
    this->hoymiles_->initCMT(sdio->get_pin(), clk->get_pin(), cs->get_pin(), fcs->get_pin(), gpio2->get_pin(), gpio3->get_pin());
}

void HmsPlatform::setup() {
    ESP_LOGI(TAG, "set_pins(): Setting up HMS instance");
    const int8_t sdio=this->sdio_->get_pin();
    const int8_t clk=this->clk_->get_pin();
    const int8_t cs=this->cs_->get_pin();
    const int8_t fcs=this->fcs_->get_pin();
    int8_t gpio2=this->gpio2_->get_pin();
    int8_t gpio3=this->gpio3_->get_pin();
    if(this->gpio2_ == nullptr){
        gpio2 = -1;   
    }
    if(this->gpio3_ == nullptr){
        gpio3 = -1;
    }

    this->hoymiles_ = &Hoymiles;
    Hoymiles.setMessageOutput(new EsphLogPrint());
    this->hoymiles_->init();
    this->hoymiles_->initCMT(sdio, clk, cs, fcs, gpio2, gpio3);

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

void HmsPlatform::update() {
    ESP_LOGD(TAG, "update(): Polling inverters");
    this->hoymiles_->loop();
}

void HmsPlatform::loop() {
    this->hoymiles_->getRadioCmt()->loop();
}

}
}
