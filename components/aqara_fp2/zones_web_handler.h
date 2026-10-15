#pragma once
#include "fp2_component.h"
#ifdef AQARA_FP2_HAS_DEVICE_WEBUI
#include "esphome/components/web_server_idf/web_server_idf.h"
#include "zones_page_bundle.h"
#include <ArduinoJson.h>
#include <cerrno>
#include <cstdlib>
#include <limits>

class ZonesPageHandler : public esphome::web_server_idf::AsyncWebHandler {
 public:
  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
    char url[esphome::web_server_idf::AsyncWebServerRequest::URL_BUF_SIZE];
    return request->method() == HTTP_GET && request->url_to(url) == "/zones";
  }
  void handleRequest(esphome::web_server_idf::AsyncWebServerRequest *request) override {
    request->send(request->beginResponse(200, "text/html", reinterpret_cast<const uint8_t *>(ZONES_PAGE_HTML), ZONES_PAGE_HTML_SIZE));
  }
};

// No live radar, entity, preference or save-state access from the HTTP task.
// Readers copy a main-loop snapshot. Writers enqueue an owned command atomically
// with its pending status; request lifetimes never extend past this handler.
class ZonesApiHandler : public esphome::web_server_idf::AsyncWebHandler {
 public:
  explicit ZonesApiHandler(esphome::aqara_fp2::FP2Component *fp2) : fp2_(fp2) {}
  bool canHandle(esphome::web_server_idf::AsyncWebServerRequest *request) const override {
    char url[esphome::web_server_idf::AsyncWebServerRequest::URL_BUF_SIZE];
    return request->method() == HTTP_POST && request->url_to(url) == "/api/zones";
  }
  void handleRequest(esphome::web_server_idf::AsyncWebServerRequest *request) override {
    using namespace esphome::aqara_fp2;
    const auto action = request->arg("action");
    if (action == "list" || action == "free_slots" || action == "status") {
      auto snapshot = fp2_->editor_snapshot();
      if (action == "list" || action == "free_slots") {
        request->send(200, "application/json", (action == "list" ? snapshot.map : snapshot.free_slots).c_str());
      } else {
        JsonDocument doc;
        doc["pending"] = snapshot.pending;
        doc["ok"] = snapshot.ok;
        doc["error"] = snapshot.error;
        std::string out;
        serializeJson(doc, out);
        request->send(200, "application/json", out.c_str());
      }
      return;
    }
    if (action != "save" && action != "create" && action != "delete" && action != "rename") {
      reject_(request, "unknown or missing action"); return;
    }
    auto csrf = request->get_header("X-FP2-CSRF");
    if (!csrf || *csrf != "1") { reject_(request, "missing X-FP2-CSRF header"); return; }
    EditorCommand command;
    int zone_id;
    if (!integer_(request, "zone_id", 0, 31, zone_id)) { reject_(request, "zone_id must be 0-31"); return; }
    command.zone_id = zone_id;
    if (action == "save" || action == "create") {
      int sensitivity;
      if (!integer_(request, "sensitivity", 1, 3, sensitivity)) { reject_(request, "sensitivity must be 1-3"); return; }
      command.sensitivity = sensitivity;
      if (request->getParam("zone_type") &&
          (!integer_(request, "zone_type", -1, 255, command.zone_type) || !fp2_is_valid_zone_type_(command.zone_type))) {
        reject_(request, "invalid zone_type"); return;
      }
      command.action = action == "save" ? EditorAction::SAVE : EditorAction::CREATE;
      if (request->getParam("grid_hex")) {
        command.grid = request->arg("grid_hex");
        if (command.grid.size() != 80 || command.grid.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) {
          reject_(request, "grid_hex must be exactly 80 hex characters"); return;
        }
      }
    } else if (action == "delete") {
      command.action = EditorAction::REMOVE;
    } else {
      command.action = EditorAction::RENAME;
      command.name = request->arg("name");
      if (command.name.empty() || command.name.size() > 31 ||
          std::any_of(command.name.begin(), command.name.end(), [](unsigned char ch) { return ch < 0x20 || ch == 0x7f; })) {
        reject_(request, "name must be 1-31 printable characters"); return;
      }
    }
    if (!fp2_->submit_editor_command(std::move(command))) {
      request->send(409, "application/json", R"({"error":"a save is already in progress"})"); return;
    }
    request->send(200, "application/json", R"({"status":"pending"})");
  }
 private:
  static bool integer_(esphome::web_server_idf::AsyncWebServerRequest *request, const char *key, int low, int high, int &out) {
    const auto value = request->arg(key);
    char *end = nullptr;
    errno = 0;
    long parsed = strtol(value.c_str(), &end, 10);
    if (errno == ERANGE || end == value.c_str() || *end || parsed < low || parsed > high) return false;
    out = static_cast<int>(parsed);
    return true;
  }
  static void reject_(esphome::web_server_idf::AsyncWebServerRequest *request, const char *message) {
    JsonDocument doc;
    doc["error"] = message;
    std::string out;
    serializeJson(doc, out);
    request->send(400, "application/json", out.c_str());
  }
  esphome::aqara_fp2::FP2Component *fp2_;
};
#endif  // AQARA_FP2_HAS_DEVICE_WEBUI
