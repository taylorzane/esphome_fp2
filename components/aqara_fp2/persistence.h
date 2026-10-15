#pragma once
#include <cstdint>
#include <string>
namespace esphome::aqara_fp2 {
template<typename Preference, typename Value, typename Sync>
bool persist_and_sync(Preference &preference, const Value &value, Sync sync) {
  return preference.save(&value) && sync();
}
inline bool save_completed(bool pending, bool failed) { return !pending && !failed; }
inline std::string zone_preference_key(uint8_t id, const std::string &identity) {
  // Numeric keys remain valid for runtime-created zones. Compiled zones start
  // a new namespace: old numeric overrides cannot be safely assigned a name.
  return identity.empty() ? "fp2_zone_override_" + std::to_string(id) : "fp2_zone_v2_" + identity;
}
}  // namespace esphome::aqara_fp2
