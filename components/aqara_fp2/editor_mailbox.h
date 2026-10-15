#pragma once
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

namespace esphome::aqara_fp2 {
enum class EditorAction { SAVE, CREATE, REMOVE, RENAME };
struct EditorCommand {
  EditorAction action{EditorAction::SAVE};
  uint8_t zone_id{0};
  uint8_t sensitivity{2};
  int zone_type{-1};
  std::string grid;
  std::string name;
};
struct EditorSnapshot {
  std::string map{"{\"zones\":[]}"};
  std::string free_slots{"{\"free_slots\":[],\"full\":false}"};
  bool pending{false};
  bool ok{true};
  std::string error;
};
// HTTP only exchanges owned values here. All live entities, preferences and
// radar operations remain on the main loop; no request pointer crosses tasks.
class EditorMailbox {
 public:
  bool submit(EditorCommand command) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (snapshot_.pending || command_) return false;
    command_ = std::move(command);
    snapshot_.pending = true;
    snapshot_.ok = false;
    snapshot_.error.clear();
    return true;
  }
  std::optional<EditorCommand> take() {
    std::lock_guard<std::mutex> guard(mutex_);
    auto command = std::move(command_);
    command_.reset();
    return command;
  }
  EditorSnapshot snapshot() {
    std::lock_guard<std::mutex> guard(mutex_);
    refresh_ = true;
    return snapshot_;
  }
  bool needs_refresh() {
    std::lock_guard<std::mutex> guard(mutex_);
    bool value = refresh_;
    refresh_ = false;
    return value;
  }
  void publish(EditorSnapshot snapshot) {
    std::lock_guard<std::mutex> guard(mutex_);
    // A command may arrive while the main loop is preparing this snapshot.
    if (command_) {
      snapshot.pending = true;
      snapshot.ok = false;
      snapshot.error.clear();
    }
    snapshot_ = std::move(snapshot);
  }
 private:
  std::mutex mutex_;
  std::optional<EditorCommand> command_;
  EditorSnapshot snapshot_;
  bool refresh_{true};
};
}  // namespace esphome::aqara_fp2
