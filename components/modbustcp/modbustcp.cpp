#include "modbustcp.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
//#include "esphome/components/network/util.h"

namespace esphome::modbustcp {

static const char *const TAG = "modbustcp";

void ModbusTCP::setup() {
    client_ = new AsyncClient();

    client_->onConnect([](void* arg, AsyncClient* c) {
      auto* self = (ModbusTCP*)arg;
      ESP_LOGI("tcp", "Connected to %s:%d", self->host_.c_str(), self->port_);
      self->connected_ = true;
    }, this);

    client_->onDisconnect([](void* arg, AsyncClient* c) {
      auto* self = (ModbusTCP*)arg;
      ESP_LOGW("tcp", "Disconnected, will retry...");
      self->connected_ = false;
      self->last_attempt_ = 0;
    }, this);

    client_->onError([](void* arg, AsyncClient* c, int8_t err) {
      auto* self = (ModbusTCP*)arg;
      ESP_LOGE("tcp", "Error: %s", c->errorToString(err));
      self->connected_ = false;
    }, this);

    client_->onData([](void* arg, AsyncClient* c, void *data, size_t len) {
      auto* self = (ModbusTCP*)arg;
      uint8_t *byte = reinterpret_cast<uint8_t*>(data);
      self->handle_message(byte);
    }, this);
}

void ModbusTCP::set_host_and_reconnect(const std::string &host) {
  if (this->host_ == host && this->connected_) {
    return; // Pas de changement si l'hôte est identique et qu'on est déjà connecté
  }

  ESP_LOGI(TAG, "Changing host to %s...", host.c_str());
  this->host_ = host;

  if (this->client_ != nullptr) {
    if (this->client_->connected() || this->client_->connecting()) {
      this->client_->close(true); // Fermeture forcée de la connexion active
    }
  }

  this->connected_ = false;
  this->last_attempt_ = 0; // Force la reconnexion immédiate

  this->connect();
}

void ModbusTCP::connect() {
  if (client_ != nullptr && !client_->connecting() && !client_->connected()) {
    if (!client_->connect(host_.c_str(), port_)) {
      ESP_LOGW("tcp", "Connection failed, will retry...");
    }
  }
}

void ModbusTCP::send_message(const uint8_t *send_byte) {
  if (connected_ && client_->canSend()) {
    client_->write(reinterpret_cast<const char*>(send_byte), (send_byte[5] + 6));
      
    std::string res1;
    char buf1[5];
      
    for (size_t i = 12; i < send_byte[5] + 6; i++) {
      sprintf(buf1, "%02X", send_byte[i]);
      res1 += buf1;
      res1 += ":";
    }
    ESP_LOGD("modbus_tcp", ">>> %02X%02X %02X%02X %02X%02X %02X %02X %02X%02X %02X%02X %s",
                 send_byte[0], send_byte[1],  send_byte[2], send_byte[3], send_byte[4], send_byte[5],
                 send_byte[6], send_byte[7],  send_byte[8], send_byte[9], send_byte[10], send_byte[11], res1.c_str());
                
  } else {
    ESP_LOGW("modbus_tcp", "Cannot send, not connected");
  }
}

void ModbusTCP::handle_message(uint8_t byte[256]) {
  uint8_t bytelen_len = 9;
  size_t data_len = byte[8];
  uint8_t address = byte[6];
  uint8_t function_code = byte[7];
  
  std::vector<uint8_t> data(byte + bytelen_len, byte + bytelen_len + bytelen_len + data_len);
  
  std::string res;
  char buf[5];
  for (size_t i = 9; i < data_len + 9; i++) {
      sprintf(buf, "%02X", byte[i]);
      res += buf;
      res += ":"; 
  }
  
  ESP_LOGD("modbus_tcp", " <<< %02X%02X %02X%02X %02X%02X %02X %02X %02X %s ",
                      byte[0], byte[1], byte[2], byte[3], byte[4], 
                      byte[5], byte[6], byte[7], byte[8], res.c_str());
   
  if ((byte[7] & 0x80) == 0x80 || (byte[7] & 0x81) == 0x81) {
    ESP_LOGE(TAG,"Error: "); 
    if (byte[8]  == 0x01) ESP_LOGE(TAG,"Failure Code 0x01 ILLEGAL FUNCTION");
    if (byte[8]  == 0x02) ESP_LOGE(TAG,"Failure Code 0x02 ILLEGAL DATA ADDRESS");
    if (byte[8]  == 0x03) ESP_LOGE(TAG,"Failure Code 0x03 ILLEGAL DATA VALUE");
    if (byte[8]  == 0x04) ESP_LOGE(TAG,"Failure Code 0x04 SERVER FAILURE");
    if (byte[8]  == 0x05) ESP_LOGE(TAG,"Failure Code 0x05 ACKNOWLEDGE");
    if (byte[8]  == 0x06) ESP_LOGE(TAG,"Failure Code 0x06 SERVER BUSY");
    return;
  }
  
  for (auto *device : this->devices_) {
    device->on_modbus_data(data);
  }
}
 
void ModbusTCP::on_shutdown() {
  if (client_) {
    client_->close();
  }
}

void ModbusTCP::loop() {
  if (!connected_) {
    uint32_t nowtcp = millis();
    if (nowtcp - last_attempt_ > 5000) {
      last_attempt_ = nowtcp;
      ESP_LOGW("tcp", "Reconnecting...");
      connect();
    }
  }

  const uint32_t now = App.get_loop_component_start_time();

  if (now - this->last_send_ > send_wait_time_) {
    waiting_for_response = 0;
  }
}

void ModbusTCP::dump_config() {
  ESP_LOGCONFIG(TAG, "Modbus_TCP:");
  ESP_LOGCONFIG(TAG, "  Client: %s \n"
                     "  Port: %d \n"
                     "  Send Wait Time: %d ms\n",
                         host_.c_str(), port_, this->send_wait_time_);
}

float ModbusTCP::get_setup_priority() const { return setup_priority::AFTER_WIFI - 1.0f; }

void ModbusTCP::send(uint8_t address, uint8_t function_code, uint16_t start_address, uint16_t number_of_entities, uint8_t payload_len, const uint8_t *payload) {
  static const size_t MAX_VALUES = 128;
  const uint32_t now = App.get_loop_component_start_time();

  if (number_of_entities > MAX_VALUES && function_code <= 0x10) {
    ESP_LOGE(TAG, "send too many values %d max=%zu", number_of_entities, MAX_VALUES);
    return;
  }
    
  size_t MAX_FRAME_SIZE = 256;
  uint8_t data[MAX_FRAME_SIZE];
  size_t pos = 0;
  data[pos++] = Transaction_Identifier >> 8;
  data[pos++] = Transaction_Identifier >> 0;
  data[pos++] = 0x00;
  data[pos++] = 0x00;
  data[pos++] = 0x00;
  if (payload != nullptr) { 
    data[pos++] = (0x04 + payload_len);
  } else {
    data[pos++] = 0x06;
  }
  data[pos++] = address;
  data[pos++] = function_code;
  data[pos++] = start_address >> 8;
  data[pos++] = start_address >> 0;

  if (function_code != 0x05 && function_code != 0x06) {
    data[pos++] = number_of_entities >> 8;
    data[pos++] = number_of_entities >> 0;
  }

  if (payload != nullptr) {
    if (function_code == 0x10 || function_code == 0x0f) {
      data[pos++] = payload_len;
    } else {
      payload_len = 2;
    }

    if (payload_len + pos > MAX_FRAME_SIZE) {
      ESP_LOGE(TAG, "Payload too large to send: %d bytes", payload_len);
      return;
    }
    for (int i = 0; i < payload_len; i++) {
      data[pos++] = payload[i];
    }
  }
       
  Transaction_Identifier++;
  send_message(data);
 
  waiting_for_response = address;
  last_send_ = millis();
}

void ModbusTCP::send_raw(const std::vector<uint8_t> &payload) {
  if (payload.empty()) {
    return;
  }

  client_->write(reinterpret_cast<const char*>(payload.data()), sizeof(payload));
  waiting_for_response = payload[0];
  ESP_LOGV(TAG, "Modbus write raw: %s", format_hex_pretty(payload).c_str());
  last_send_ = millis();
}

}  // namespace esphome::modbustcp
