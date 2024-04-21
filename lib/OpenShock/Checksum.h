#pragma once

#include <cstdint>

namespace OpenShock::Checksum {
  constexpr std::uint8_t CRC8(const std::uint8_t* data, std::size_t size, std::uint8_t checksum = 0) {
     return size == 0 ? checksum : CRC8(data + 1, size - 1, checksum + *data);
   }
  template<typename T>
  constexpr std::uint8_t CRC8(T data) {
    return CRC8(reinterpret_cast<std::uint8_t*>(&data), sizeof(T));
  }
}  // namespace OpenShock::Checksum
