/**
 * Linker --wrap overrides for ESPHome network utility functions.
 *
 * ESPHome's network/util.cpp only checks USE_WIFI/USE_ETHERNET/etc.
 * We use --wrap to intercept these and add halow as a network provider.
 *
 * The --wrap=SYMBOL linker flag redirects:
 *   calls to SYMBOL -> __wrap_SYMBOL (our implementation)
 *   original SYMBOL -> __real_SYMBOL (available for fallback)
 */

#include "esphome/core/defines.h"

#ifdef USE_HALOW
#ifdef USE_NETWORK

#include "esphome/components/network/ip_address.h"
#include "halow_component.h"
#include <string>

// Original functions (provided by linker via --wrap)
extern "C" {
extern bool __real__ZN7esphome7network12is_connectedEv();
extern esphome::network::IPAddresses __real__ZN7esphome7network16get_ip_addressesEv();
extern const char *__real__ZN7esphome7network15get_use_addressEv();
}

// Wrapped: esphome::network::is_connected()
extern "C" bool __wrap__ZN7esphome7network12is_connectedEv() {
  if (esphome::halow::global_halow_component != nullptr &&
      esphome::halow::global_halow_component->is_connected())
    return true;
  return __real__ZN7esphome7network12is_connectedEv();
}

// Wrapped: esphome::network::get_ip_addresses()
extern "C" esphome::network::IPAddresses __wrap__ZN7esphome7network16get_ip_addressesEv() {
  if (esphome::halow::global_halow_component != nullptr &&
      esphome::halow::global_halow_component->is_connected())
    return esphome::halow::global_halow_component->get_ip_addresses();
  return __real__ZN7esphome7network16get_ip_addressesEv();
}

// Wrapped: esphome::network::get_use_address()
static std::string s_halow_address;
extern "C" const char *__wrap__ZN7esphome7network15get_use_addressEv() {
  if (esphome::halow::global_halow_component != nullptr &&
      esphome::halow::global_halow_component->is_connected()) {
    s_halow_address = esphome::halow::global_halow_component->get_ip_address_str();
    return s_halow_address.c_str();
  }
  return __real__ZN7esphome7network15get_use_addressEv();
}

#endif  // USE_NETWORK
#endif  // USE_HALOW
