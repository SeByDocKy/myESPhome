#pragma once

namespace esphome {
namespace hmsw {

// Warning-code (wcode1, i.e. WInfoMO.WCode & 0xFF) -> human-readable label.
// Ported from ohAnd/dtuGateway's warningCodeMap (src/dtuInterface.cpp,
// readRespCommandGetAlarms()) -- community-sourced from real-hardware
// observation, not an official Hoymiles document (that project itself
// marks several nearby/legacy codes "[not approved]"/"[Unknown]"), so
// treat unfamiliar codes with some skepticism. A code not in this table
// falls through to "Unknown warning code" (the numeric code is still
// appended by the caller), so nothing is silently dropped. See README.md.
inline const char *hmsw_warning_label(int wcode1) {
  switch (wcode1) {
    case 72:
      return "Power grid over-frequency load reduction (FW) function enabled";
    case 121:
      return "Over temperature";
    case 124:
      return "Shut down by remote control";
    case 125:
      return "Grid configuration parameter error";
    case 127:
      return "Firmware error";
    case 129:
      return "Abnormal bias";
    case 130:
      return "Offline";
    case 141:
      return "[Grid] Grid overvoltage";
    case 142:
      return "[Grid] 10 min value grid overvoltage";
    case 143:
      return "[Grid] Grid undervoltage";
    case 144:
      return "[Grid] Grid overfrequency";
    case 145:
      return "[Grid] Grid underfrequency";
    case 146:
      return "[Grid] Rapid grid frequency change rate";
    case 147:
      return "[Grid] Power grid outage";
    case 148:
      return "[Grid] Grid disconnection";
    case 149:
      return "[Grid] Island detected";
    case 205:
      return "[MPPT-A] Input overvoltage";
    case 206:
      return "[MPPT-B] Input overvoltage";
    case 207:
      return "[MPPT-A] Input undervoltage";
    case 208:
      return "[MPPT-B] Input undervoltage";
    case 209:
      return "[PV-1] No input";
    case 210:
      return "[PV-2] No input";
    case 213:
      return "[MPPT-A] PV-1 & PV-2 abnormal wiring";
    case 214:
      return "[MPPT-B] PV-3 & PV-4 abnormal wiring";
    case 215:
      return "[PV-1] Input overvoltage";
    case 216:
      return "[PV-1] Input undervoltage";
    case 217:
      return "[PV-2] Input overvoltage";
    case 218:
      return "[PV-2] Input undervoltage";
    case 301:
      return "[Device] Device failure 301";
    case 302:
      return "[Device] Device failure 302";
    case 303:
      return "[Device] Device failure 303";
    case 304:
      return "[Device] Device failure 304";
    case 305:
      return "[Device] Device failure 305";
    case 306:
      return "[Device] Device failure 306";
    case 307:
      return "[Device] Device failure 307";
    case 308:
      return "[Device] Device failure 308";
    case 309:
      return "[Device] Device failure 309";
    case 310:
      return "[Device] Device failure 310";
    case 311:
      return "[Device] Device failure 311";
    case 312:
      return "[Device] Device failure 312";
    case 313:
      return "[Device] Device failure 313";
    case 314:
      return "[Device] Device failure 314";
    default:
      return "Unknown warning code";
  }
}

}  // namespace hmsw
}  // namespace esphome
