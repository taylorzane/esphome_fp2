#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>
namespace esphome::aqara_fp2 {
inline uint16_t fp2_crc16(const uint8_t *data, size_t size) {
  uint16_t crc = 0xffff;
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xa001 : 0);
  }
  return crc;
}
inline std::vector<uint8_t> encode_fp2_frame(uint8_t opcode, uint8_t sequence,
                                           const std::vector<uint8_t> &payload) {
  const uint16_t size = payload.size();
  std::vector<uint8_t> frame{0x55, 0, 1, sequence, opcode, uint8_t(size >> 8), uint8_t(size)};
  uint8_t sum = 0;
  for (auto byte : frame) sum += byte;
  frame.push_back(uint8_t(~(sum - 1)));
  frame.insert(frame.end(), payload.begin(), payload.end());
  const auto crc = fp2_crc16(frame.data(), frame.size());
  frame.push_back(uint8_t(crc));
  frame.push_back(uint8_t(crc >> 8));
  return frame;
}
}  // namespace esphome::aqara_fp2
