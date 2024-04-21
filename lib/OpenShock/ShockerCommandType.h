#pragma once

#include <cstdint>
#include <cstring>


namespace OpenShock {
  enum ShockerCommandType : std::uint8_t {
    Stop = 0,
    Shock,
    Vibrate,
    Sound,
  };

  inline bool ShockerCommandTypeFromString(const char* str, ShockerCommandType& out) {
    if (strcasecmp(str, "stop") == 0) {
      out = ShockerCommandType::Stop;
      return true;
    } else if (strcasecmp(str, "shock") == 0) {
      out = ShockerCommandType::Shock;
      return true;
    } else if (strcasecmp(str, "vibrate") == 0) {
      out = ShockerCommandType::Vibrate;
      return true;
    } else if (strcasecmp(str, "sound") == 0) {
      out = ShockerCommandType::Sound;
      return true;
    } else {
      return false;
    }
  }
}  // namespace OpenShock
