#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

// ref:
// https://github.com/usefulsensors/person_sensor_pico_c/blob/main/person_sensor.h

namespace esphome {
namespace sen21231 {
// The I2C address of the person sensor board.
static const uint8_t PERSON_SENSOR_I2C_ADDRESS = 0x62;
static const uint8_t PERSON_SENSOR_REG_MODE = 0x01;
static const uint8_t PERSON_SENSOR_REG_ENABLE_ID = 0x02;
static const uint8_t PERSON_SENSOR_REG_SINGLE_SHOT = 0x03;
static const uint8_t PERSON_SENSOR_REG_CALIBRATE_ID = 0x04;
static const uint8_t PERSON_SENSOR_REG_PERSIST_IDS = 0x05;
static const uint8_t PERSON_SENSOR_REG_ERASE_IDS = 0x06;
static const uint8_t PERSON_SENSOR_REG_DEBUG_MODE = 0x07;

static const uint8_t PERSON_SENSOR_MAX_FACES_COUNT = 4;
static const uint8_t PERSON_SENSOR_MAX_IDS_COUNT = 7;

// The results returned from the sensor have a short header providing
// information about the length of the data packet:
//   reserved: Currently unused bytes.
//   data_size: Length of the entire packet, excluding the header and
//   checksum.
//     For version 1.0 of the sensor, this should be 40.
using person_sensor_results_header_t = struct {
  uint8_t reserved[2];  // Bytes 0-1.
  uint16_t data_size;   // Bytes 2-3.
};

// Each face found has a set of information associated with it:
//   box_confidence: How certain we are we have found a face, from 0 to 255.
//   box_left: X coordinate of the left side of the box, from 0 to 255.
//   box_top: Y coordinate of the top edge of the box, from 0 to 255.
//   box_width: Width of the box, where 255 is the full view port size.
//   box_height: Height of the box, where 255 is the full view port size.
//   id_confidence: How sure the sensor is about the recognition result.
//   id: Numerical ID assigned to this face.
//   is_looking_at: Whether the person is facing the camera, 0 or 1.
using person_sensor_face_t = struct __attribute__((__packed__)) {
  uint8_t box_confidence;  // Byte 1.
  uint8_t box_left;        // Byte 2.
  uint8_t box_top;         // Byte 3.
  uint8_t box_right;       // Byte 4.
  uint8_t box_bottom;      // Byte 5.
  int8_t id_confidence;    // Byte 6.
  int8_t id;               // Byte 7
  uint8_t is_facing;       // Byte 8.
};

// This is the full structure of the packet returned over the wire from the
// sensor when we do an I2C read from the peripheral address.
// The checksum should be the CRC16 of bytes 0 to 38. You shouldn't need to
// verify this in practice, but we found it useful during our own debugging.
using person_sensor_results_t = struct __attribute__((__packed__)) {
  person_sensor_results_header_t header;                      // Bytes 0-4.
  int8_t num_faces;                                           // Byte 5.
  person_sensor_face_t faces[PERSON_SENSOR_MAX_FACES_COUNT];  // Bytes 6-37.
  uint16_t checksum;                                          // Bytes 38-39.
};

class SEN21231Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;
  
  void set_nfaces_sensor(sensor::Sensor *nfaces_sensor) { nfaces_sensor_ = nfaces_sensor; }
  void set_boxconf0_sensor(sensor::Sensor *boxconf0_sensor) { boxconf0_sensor_ = boxconf0_sensor; }
  void set_x0_sensor(sensor::Sensor *x0_sensor) { x0_sensor_ = x0_sensor; }
  void set_y0_sensor(sensor::Sensor *y0_sensor) { y0_sensor_ = y0_sensor; }
  void set_w0_sensor(sensor::Sensor *w0_sensor) { w0_sensor_ = w0_sensor; }
  void set_h0_sensor(sensor::Sensor *h0_sensor) { h0_sensor_ = h0_sensor; }
  void set_idconf0_sensor(sensor::Sensor *idconf0_sensor) { idconf0_sensor_ = idconf0_sensor; }
  void set_id0_sensor(sensor::Sensor *id0_sensor) { id0_sensor_ = id0_sensor; }
  void set_isfacing0_sensor(sensor::Sensor *isfacing0_sensor) { isfacing0_sensor_ = isfacing0_sensor; }
  
  void set_mode_register(uint8_t mode) { mode_ = mode; }
  void write_mode_register(uint8_t mode_value);
  uint8_t read_mode_register(uint8_t mode_register);
  
  void set_enableid_register(uint8_t enableid) { enableid_ = enableid; }
  void write_enableid_register(uint8_t enableid_value);
  uint8_t read_enableid_register(void);
  
  void set_singleshot_register(uint8_t singleshot) { singleshot_ = singleshot; }
  void write_singleshot_register(uint8_t singleshot_value);
  uint8_t read_singleshot_register(void);
  
  void set_labelnext_register(uint8_t labelnext) { labelnext_ = labelnext; }
  void write_labelnext_register(uint8_t labelnext_value);
  uint8_t read_labelnext_register(void);
  
  void set_persistid_register(uint8_t persistid) { persistid_ = persistid; }
  void write_persistid_register(uint8_t persistid_value);
  uint8_t read_persistid_register(void);
  
  void set_eraseid_register(uint8_t eraseid) { eraseid_ = eraseid; }
  void write_eraseid_register(uint8_t eraseid_value);
  uint8_t read_eraseid_register(void);
  
  void set_debug_register(uint8_t debug) { debug_ = debug; }
  void write_debug_register(uint8_t debug_value);
  uint8_t read_debug_register(void);
   
 protected:
  void read_data_();
  
  uint8_t read_register(uint8_t reg);
  void write_register(uint8_t reg, uint8_t bits, uint8_t start_position);

  uint8_t mode_, enableid_, singleshot_, labelnext_, persistid_, eraseid_, debug_;
  
  sensor::Sensor *nfaces_sensor_{nullptr};
  sensor::Sensor *boxconf0_sensor_{nullptr};
  sensor::Sensor *x0_sensor_{nullptr};
  sensor::Sensor *y0_sensor_{nullptr};
  sensor::Sensor *w0_sensor_{nullptr};
  sensor::Sensor *h0_sensor_{nullptr};
  sensor::Sensor *idconf0_sensor_{nullptr};
  sensor::Sensor *id0_sensor_{nullptr};
  sensor::Sensor *isfacing0_sensor_{nullptr};
  
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    ID_REGISTERS,
  } error_code_;
  
};

}  // namespace sen21231
}  // namespace esphome
