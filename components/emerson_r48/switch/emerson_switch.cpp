#include "esphome/core/log.h"
#include "emerson_switch.h"
#include "esphome/core/preferences.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace emerson_r48 {

static const char *TAG = "emerson_switch.switch";

static const int8_t SET_AC_FUNCTION = 0x0;
static const int8_t SET_DC_FUNCTION = 0x1;
static const int8_t SET_FAN_FUNCTION = 0x2;
static const int8_t SET_LED_FUNCTION = 0x3;



void EmersonR48Switch::setup() {

}

void EmersonR48Switch::write_state(bool state) {
    ESP_LOGD(TAG, "-> new switch state: %d", state);
    uint8_t msgv = 0;

    switch (this->functionCode_) {
        case SET_AC_FUNCTION:
           // this->pref_ = global_preferences->make_preference<bool>(this->get_object_id_hash());
           // if (!this->pref_.load(&state)) {
           //   state = this->parent_->get_current_ac_switch(); 
           // }
           // this->parent_->set_current_ac_switch(state);
            
            parent_->acOff_ = state;
            msgv = parent_->dcOff_ << 7 | parent_->fanFull_ << 4 | parent_->flashLed_ << 3 | parent_->acOff_ << 2 | 1;
            parent_->set_control(msgv);
            this->publish_state(state);
            break;
        case SET_DC_FUNCTION:
            parent_->dcOff_ = state;
            msgv = parent_->dcOff_ << 7 | parent_->fanFull_ << 4 | parent_->flashLed_ << 3 | parent_->acOff_ << 2 | 1;
            parent_->set_control(msgv);
            this->publish_state(state);
            break;
        case SET_FAN_FUNCTION:
            parent_->fanFull_ = state;
            msgv = parent_->dcOff_ << 7 | parent_->fanFull_ << 4 | parent_->flashLed_ << 3 | parent_->acOff_ << 2 | 1;
            parent_->set_control(msgv);
            this->publish_state(state);
            break;
        case SET_LED_FUNCTION:
            parent_->flashLed_ = state;
            msgv = parent_->dcOff_ << 7 | parent_->fanFull_ << 4 | parent_->flashLed_ << 3 | parent_->acOff_ << 2 | 1;
            parent_->set_control(msgv);
            this->publish_state(state);
            break;

        default:
        break;
    }
}

void EmersonR48Switch::dump_config(){
    ESP_LOGCONFIG(TAG, "Emerson custom switch");
}

} //namespace emerson_r48
} //namespace esphome
