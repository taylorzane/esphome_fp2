#include <cassert>
#include <thread>
#include <vector>
#include "components/aqara_fp2/editor_mailbox.h"
#include "components/aqara_fp2/protocol.h"
#include "components/aqara_fp2/persistence.h"
using namespace esphome::aqara_fp2;
int main() {
  // A submitted mutation is visible as pending before the main loop sees it.
  EditorMailbox mailbox;
  EditorCommand command;
  command.action = EditorAction::SAVE;
  command.zone_id = 2;
  assert(mailbox.submit(command));
  assert(mailbox.snapshot().pending);
  assert(!mailbox.snapshot().ok);
  assert(!mailbox.submit(command));
  auto work = mailbox.take();
  assert(work && work->zone_id == 2);
  assert(mailbox.snapshot().pending);
  EditorSnapshot completed;
  completed.map = "room-v2";
  completed.ok = true;
  mailbox.publish(completed);
  assert(mailbox.snapshot().map == "room-v2");
  assert(mailbox.snapshot().ok);
  assert(mailbox.submit(command));
  mailbox.take();
  completed.ok = false;
  completed.error = "flash sync failed";
  mailbox.publish(completed);
  assert(!mailbox.snapshot().ok);
  assert(mailbox.snapshot().error == "flash sync failed");
  // Readers see whole copied snapshots while the producer replaces strings.
  std::thread producer([&] {
    for (int i = 0; i < 10000; ++i) {
      EditorSnapshot s;
      s.map = std::string(128, i % 2 ? 'a' : 'b');
      mailbox.publish(s);
    }
  });
  for (int i = 0; i < 10000; ++i) {
    auto s = mailbox.snapshot();
    if (s.map.size() == 128) assert(s.map == std::string(128, s.map[0]));
  }
  producer.join();
  // Replies echo the incoming sequence; the payload and CRC remain intact.
  const std::vector<uint8_t> payload{0x01, 0x20, 0x01, 0x00, 0x5a};
  assert(fp2_crc16(reinterpret_cast<const uint8_t *>("123456789"), 9) == 0x4b37);
  auto reply = encode_fp2_frame(4, 137, payload);
  assert(reply[7] == 0x18);
  assert(reply[3] == 137 && reply[4] == 4 && reply[6] == payload.size());
  assert(std::vector<uint8_t>(reply.begin() + 8, reply.end() - 2) == payload);
  auto crc = fp2_crc16(reply.data(), reply.size() - 2);
  assert(reply[reply.size()-2] == (crc & 255) && reply.back() == (crc >> 8));
  // Preferences must report both staging and durable flush failures.
  struct Preference { bool accepted; bool save(const int *) { return accepted; } } pref{false};
  bool flushed = false;
  assert(!persist_and_sync(pref, 1, [&] { flushed = true; return true; }));
  assert(!flushed);
  pref.accepted = true;
  assert(!persist_and_sync(pref, 1, [] { return false; }));
  assert(persist_and_sync(pref, 1, [] { return true; }));
  // Compiled zone identities survive YAML reordering, without trusting legacy IDs.
  assert(zone_preference_key(1, "sofa") == zone_preference_key(8, "sofa"));
  assert(zone_preference_key(1, "sofa") != zone_preference_key(1, "desk"));
  assert(zone_preference_key(1, "") != zone_preference_key(2, ""));
  assert(!save_completed(true, false));
  assert(!save_completed(false, true));
  assert(save_completed(false, false));
}
