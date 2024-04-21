#pragma once

#include "ShockerCommandType.h"
#include "ShockerModelType.h"

#include <cstdint>

// TODO: This is horrible architecture. Fix it.

enum class SetRfPinResultCode : uint8_t {
  Success = 0,
  InvalidPin = 1,
  InternalError = 2,
  MIN = Success,
  MAX = InternalError
};

inline const SetRfPinResultCode (&EnumValuesSetRfPinResultCode())[3] {
  static const SetRfPinResultCode values[] = {
    SetRfPinResultCode::Success,
    SetRfPinResultCode::InvalidPin,
    SetRfPinResultCode::InternalError
  };
  return values;
}

namespace OpenShock::CommandHandler {
  bool Init();
  bool Ok();

  SetRfPinResultCode SetRfTxPin(std::uint8_t txPin);
  std::uint8_t GetRfTxPin();

  bool SetKeepAliveEnabled(bool enabled);
  bool SetKeepAlivePaused(bool paused);

  bool HandleCommand(ShockerModelType shockerModel, std::uint16_t shockerId, ShockerCommandType type, std::uint8_t intensity, std::uint16_t durationMs);
}  // namespace OpenShock::CommandHandler
