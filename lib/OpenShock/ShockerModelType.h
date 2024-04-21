#pragma once

#include <cstdint>
#include <cstring>

namespace OpenShock {
  enum class ShockerModelType : std::uint8_t {
    CaiXianlin,
    Petrainer,
    Petrainer998DR,
  };

  inline bool ShockerModelTypeFromString(const char* str, ShockerModelType& out, bool allowTypo = false) {
    if (strcasecmp(str, "caixianlin") == 0 || strcasecmp(str, "cai-xianlin") == 0) {
      out = ShockerModelType::CaiXianlin;
      return true;
    }

    if (strcasecmp(str, "petrainer") == 0) {
      out = ShockerModelType::Petrainer;
      return true;
    }

    if (allowTypo && strcasecmp(str, "pettrainer") == 0) {
      out = ShockerModelType::Petrainer;
      return true;
    }

    if (strcasecmp(str, "petrainer998dr") == 0) {
      out = ShockerModelType::Petrainer998DR;
      return true;
    }

    if (allowTypo && strcasecmp(str, "pettrainer998dr") == 0) {
      out = ShockerModelType::Petrainer998DR;
      return true;
    }

    return false;
  }
}  // namespace OpenShock
