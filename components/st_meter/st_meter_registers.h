#pragma once

namespace esphome {
namespace st_meter {

/* PHASE STATUS REGISTERS */
static const uint16_t ST_PHASE_1_VOLTAGE = 0x0000;
static const uint16_t ST_PHASE_2_VOLTAGE = 0x0002;
static const uint16_t ST_PHASE_3_VOLTAGE = 0x0004;
static const uint16_t ST_PHASE_1_CURRENT = 0x0008;
static const uint16_t ST_PHASE_2_CURRENT = 0x000A;
static const uint16_t ST_PHASE_3_CURRENT = 0x000C;

static const uint16_t ST_TOTAL_ACTIVE_POWER = 0x0010;
static const uint16_t ST_PHASE_1_ACTIVE_POWER = 0x0012;
static const uint16_t ST_PHASE_2_ACTIVE_POWER = 0x0014;
static const uint16_t ST_PHASE_3_ACTIVE_POWER = 0x0016;

static const uint16_t ST_TOTAL_REACTIVE_POWER = 0x0018;
static const uint16_t ST_PHASE_1_REACTIVE_POWER = 0x001A;
static const uint16_t ST_PHASE_2_REACTIVE_POWER = 0x001C;
static const uint16_t ST_PHASE_3_REACTIVE_POWER = 0x001E;
static const uint16_t ST_PHASE_1_POWER_FACTOR = 0x002A;
static const uint16_t ST_PHASE_2_POWER_FACTOR = 0x002C;
static const uint16_t ST_PHASE_3_POWER_FACTOR = 0x002E;

static const uint16_t ST_FREQUENCY = 0x0036;
static const uint16_t ST_TOTAL_ACTIVE_ELECTRICITY_POWER = 0x0100;
static const uint16_t ST_TOTAL_REACTIVE_ELECTRICITY_POWER = 0x0400;

}  // namespace st_meter
}  // namespace esphome
