#include "web.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Update.h>
#include "config.h"
#include "settings.h"
#include "dmx_out.h"
#include "net_input.h"
#include "network.h"
#include "sync.h"
#include "web_pages.h"

static const char *PASS_MASK = "********";

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");

// Config changes are applied in the loop task to avoid racing with DMX/network code
enum class Pending : uint8_t { None, ApplyLive, ApplyWifi, Reboot, Factory };
static Settings pendingCfg;
static volatile Pending pending = Pending::None;
static uint32_t pendingAtMs = 0;

static String stateJson() {
  JsonDocument d;
  d["fan"] = dmxManualFan();
  d["pump"] = dmxManualPump();
  d["src"] = dmxSourceName(dmxActiveSource());
  d["base"] = cfg.baseAddress;
  d["oFan"] = dmxOutputSlot(cfg.baseAddress);
  d["oPump"] = dmxOutputSlot(cfg.baseAddress + 1);
  d["ver"] = FW_VERSION;
  d["syncMaster"] = cfg.syncMaster;
  d["syncRx"] = syncReceiving();
  JsonArray arr = d["presets"].to<JsonArray>();
  for (const Preset &p : cfg.presets) {
    JsonObject o = arr.add<JsonObject>();
    o["n"] = p.name;
    o["f"] = p.fan;
    o["p"] = p.pump;
  }
  String s;
  serializeJson(d, s);
  return s;
}

static void sendJson(AsyncWebServerRequest *r, const JsonDocument &d) {
  String s;
  serializeJson(d, s);
  r->send(200, "application/json", s);
}

// ---- form helpers -------------------------------------------------------

static bool arg(AsyncWebServerRequest *r, const char *name, String &out) {
  if (!r->hasParam(name, true)) return false;
  out = r->getParam(name, true)->value();
  return true;
}

static void argStr(AsyncWebServerRequest *r, const char *name, char *dst, size_t size) {
  String v;
  if (arg(r, name, v)) strlcpy(dst, v.c_str(), size);
}

static long argNum(AsyncWebServerRequest *r, const char *name, long def, long lo, long hi) {
  String v;
  if (!arg(r, name, v) || v.isEmpty()) return def;
  return constrain(v.toInt(), lo, hi);
}

static bool argBool(AsyncWebServerRequest *r, const char *name) {
  String v;
  return arg(r, name, v) && (v == "1" || v == "on" || v == "true");
}

static bool argIp(AsyncWebServerRequest *r, const char *name, uint32_t &dst) {
  String v;
  IPAddress ip;
  if (!arg(r, name, v) || !ip.fromString(v)) return false;
  dst = (uint32_t)ip;
  return true;
}

// Returns false if the password is invalid; the mask value keeps the stored password
static bool argPass(AsyncWebServerRequest *r, const char *name, char *dst, size_t size, bool apRules) {
  String v;
  if (!arg(r, name, v) || v == PASS_MASK) return true;
  if (v.length() > 63) return false;
  if (apRules && v.length() > 0 && v.length() < 8) return false;
  strlcpy(dst, v.c_str(), size);
  return true;
}

// Builds a DNS-safe hostname: lowercase a-z/0-9/'-', German umlauts transliterated
static void hostnameFromName(const char *name, char *out, size_t size) {
  String h;
  for (const uint8_t *c = (const uint8_t *)name; *c; c++) {
    if (*c == 0xC3 && c[1]) {
      c++;
      switch (*c) {
        case 0xA4: case 0x84: h += "ae"; break;
        case 0xB6: case 0x96: h += "oe"; break;
        case 0xBC: case 0x9C: h += "ue"; break;
        case 0x9F: h += "ss"; break;
        default: if (h.length() && !h.endsWith("-")) h += '-';
      }
    } else if (isalnum(*c)) {
      h += (char)tolower(*c);
    } else if (h.length() && !h.endsWith("-")) {
      h += '-';
    }
  }
  if (h.length() > size - 1) h.remove(size - 1);
  while (h.endsWith("-")) h.remove(h.length() - 1);
  strlcpy(out, h.isEmpty() ? DEFAULT_HOSTNAME : h.c_str(), size);
}

static bool rejectIfBusy(AsyncWebServerRequest *r) {
  if (pending == Pending::None) return false;
  r->send(503, "text/plain", "Busy, try again");
  return true;
}

static void schedule(Pending p) {
  pendingAtMs = millis();
  pending = p;
}

// ---- handlers -----------------------------------------------------------

static void handleConfig(AsyncWebServerRequest *r) {
  JsonDocument d;
  d["ssid"] = cfg.ssid;
  d["pass"] = cfg.pass[0] ? PASS_MASK : "";
  d["hostname"] = cfg.hostname;
  d["staticIp"] = cfg.staticIp;
  d["ip"] = IPAddress(cfg.ip).toString();
  d["gateway"] = IPAddress(cfg.gateway).toString();
  d["subnet"] = IPAddress(cfg.subnet).toString();
  d["dns"] = IPAddress(cfg.dns).toString();
  d["apSsid"] = cfg.apSsid;
  d["apPass"] = cfg.apPass[0] ? PASS_MASK : "";
  d["apChannel"] = cfg.apChannel;
  d["apHidden"] = cfg.apHidden;

  d["base"] = cfg.baseAddress;
  d["artnet"] = cfg.artnetEnabled;
  d["artNet"] = cfg.artNet;
  d["artSubnet"] = cfg.artSubnet;
  d["artUniverse"] = cfg.artUniverse;
  d["sacn"] = cfg.sacnEnabled;
  d["sacnUniverse"] = cfg.sacnUniverse;
  d["timeout"] = cfg.inputTimeoutS;
  d["shortName"] = cfg.shortName;
  d["longName"] = cfg.longName;
  d["presetCount"] = NUM_PRESETS;
  d["syncRecv"] = cfg.syncReceive;
  d["syncPort"] = cfg.syncPort;
  d["syncRepeat"] = cfg.syncRepeat;
  for (int g = 0; g < 8; g++) {
    d["sg" + String(g + 1)] = (cfg.syncSendGroups >> g) & 1;
    d["rg" + String(g + 1)] = (cfg.syncRecvGroups >> g) & 1;
  }
  for (int i = 0; i < NUM_PRESETS; i++) {
    d["p" + String(i) + "n"] = cfg.presets[i].name;
    d["p" + String(i) + "f"] = cfg.presets[i].fan;
    d["p" + String(i) + "p"] = cfg.presets[i].pump;
  }
  sendJson(r, d);
}

static void handleSaveWifi(AsyncWebServerRequest *r) {
  if (rejectIfBusy(r)) return;
  pendingCfg = cfg;
  Settings &s = pendingCfg;

  argStr(r, "ssid", s.ssid, sizeof(s.ssid));
  argStr(r, "apSsid", s.apSsid, sizeof(s.apSsid));
  s.staticIp = argBool(r, "staticIp");
  s.apHidden = argBool(r, "apHidden");
  s.apChannel = argNum(r, "apChannel", s.apChannel, 1, 13);

  if (!argPass(r, "pass", s.pass, sizeof(s.pass), false)) return r->send(400, "text/plain", "WiFi password too long");
  if (!argPass(r, "apPass", s.apPass, sizeof(s.apPass), true))
    return r->send(400, "text/plain", "AP password must be empty or 8-63 characters");
  if (!s.apSsid[0]) return r->send(400, "text/plain", "AP name must not be empty");
  if (s.staticIp) {
    if (!argIp(r, "ip", s.ip) || !argIp(r, "gateway", s.gateway) || !argIp(r, "subnet", s.subnet) ||
        !argIp(r, "dns", s.dns) || s.ip == 0)
      return r->send(400, "text/plain", "Invalid static IP configuration");
  }

  schedule(Pending::ApplyWifi);
  r->send(200, "text/plain", "Saved. Rebooting...");
}

static void handleSaveDmx(AsyncWebServerRequest *r) {
  if (rejectIfBusy(r)) return;
  pendingCfg = cfg;
  Settings &s = pendingCfg;

  s.baseAddress = argNum(r, "base", s.baseAddress, 1, 511);
  s.artnetEnabled = argBool(r, "artnet");
  s.artNet = argNum(r, "artNet", s.artNet, 0, 127);
  s.artSubnet = argNum(r, "artSubnet", s.artSubnet, 0, 15);
  s.artUniverse = argNum(r, "artUniverse", s.artUniverse, 0, 15);
  s.sacnEnabled = argBool(r, "sacn");
  s.sacnUniverse = argNum(r, "sacnUniverse", s.sacnUniverse, 1, 63999);
  s.inputTimeoutS = argNum(r, "timeout", s.inputTimeoutS, 1, 60);
  argStr(r, "shortName", s.shortName, sizeof(s.shortName));
  argStr(r, "longName", s.longName, sizeof(s.longName));

  for (int i = 0; i < NUM_PRESETS; i++) {
    Preset &p = s.presets[i];
    argStr(r, ("p" + String(i) + "n").c_str(), p.name, sizeof(p.name));
    if (!p.name[0]) snprintf(p.name, sizeof(p.name), "Preset %d", i + 1);
    p.fan = argNum(r, ("p" + String(i) + "f").c_str(), p.fan, 0, 255);
    p.pump = argNum(r, ("p" + String(i) + "p").c_str(), p.pump, 0, 255);
  }

  s.syncReceive = argBool(r, "syncRecv");
  s.syncPort = argNum(r, "syncPort", s.syncPort, 1024, 65535);
  s.syncRepeat = argNum(r, "syncRepeat", s.syncRepeat, 1, 10);
  if (s.syncPort == 6454 || s.syncPort == 5568)
    return r->send(400, "text/plain", "Sync port must differ from the ArtNet (6454) and sACN (5568) ports");
  s.syncSendGroups = s.syncRecvGroups = 0;
  for (int g = 0; g < 8; g++) {
    if (argBool(r, ("sg" + String(g + 1)).c_str())) s.syncSendGroups |= 1 << g;
    if (argBool(r, ("rg" + String(g + 1)).c_str())) s.syncRecvGroups |= 1 << g;
  }

  schedule(Pending::ApplyLive);
  r->send(200, "text/plain", "Saved.");
}

static void handleSaveName(AsyncWebServerRequest *r) {
  if (rejectIfBusy(r)) return;
  pendingCfg = cfg;
  argStr(r, "deviceName", pendingCfg.deviceName, sizeof(pendingCfg.deviceName));
  hostnameFromName(pendingCfg.deviceName, pendingCfg.hostname, sizeof(pendingCfg.hostname));
  // Hostname is only announced via DHCP/mDNS at connect time, so a change needs a reboot
  if (strcmp(pendingCfg.hostname, cfg.hostname) != 0) {
    schedule(Pending::ApplyWifi);
    r->send(200, "text/plain", String("Saved. New hostname '") + pendingCfg.hostname + "', rebooting...");
    return;
  }
  schedule(Pending::ApplyLive);
  r->send(200, "text/plain", "Saved.");
}

static void handleScan(AsyncWebServerRequest *r) {
  JsonDocument d;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
    d["scanning"] = true;
  } else if (n == WIFI_SCAN_RUNNING) {
    d["scanning"] = true;
  } else {
    JsonArray arr = d["networks"].to<JsonArray>();
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i).isEmpty()) continue;
      JsonObject o = arr.add<JsonObject>();
      o["ssid"] = WiFi.SSID(i);
      o["rssi"] = WiFi.RSSI(i);
      o["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    }
    WiFi.scanDelete();
  }
  sendJson(r, d);
}

static void handleInfo(AsyncWebServerRequest *r) {
  JsonDocument d;
  d["Firmware"] = FW_VERSION;
  d["MAC"] = WiFi.macAddress();
  d["WiFi"] = netStaConnected() ? String(cfg.ssid) + " (" + WiFi.RSSI() + " dBm)" : String("not connected");
  d["IP"] = netStaConnected() ? WiFi.localIP().toString() : String("-");
  d["Access point"] = netApActive() ? String(cfg.apSsid) + " @ " + WiFi.softAPIP().toString() +
                                         (netApForced() ? " (manual)" : "")
                                   : String("off");
  d["Hostname"] = String(cfg.hostname) + ".local";
  d["Input source"] = dmxIsExternal() ? "External DMX" : dmxSourceName(dmxActiveSource());
  d["Network packets"] = inputPacketCount();
  d["Uptime (s)"] = millis() / 1000;
  d["Free heap"] = ESP.getFreeHeap();
  sendJson(r, d);
}

static void handleOtaDone(AsyncWebServerRequest *r) {
  bool ok = !Update.hasError();
  AsyncWebServerResponse *resp =
      r->beginResponse(ok ? 200 : 500, "text/plain", ok ? String("Update successful. Rebooting...") :
                                                          String("Update failed: ") + Update.errorString());
  resp->addHeader("Connection", "close");
  r->send(resp);
  if (ok) schedule(Pending::Reboot);
  else dmxResume();
}

static void handleOtaChunk(AsyncWebServerRequest *r, const String &filename, size_t index, uint8_t *data, size_t len,
                           bool final) {
  if (index == 0) {
    Serial.printf("[OTA] Start: %s\n", filename.c_str());
    dmxPause();
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) Update.printError(Serial);
  }
  if (!Update.hasError() && Update.write(data, len) != len) Update.printError(Serial);
  if (final) {
    if (Update.end(true)) Serial.printf("[OTA] Success: %u bytes\n", (unsigned)(index + len));
    else Update.printError(Serial);
  }
}

static void onWsEvent(AsyncWebSocket *, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data,
                      size_t len) {
  if (type == WS_EVT_CONNECT) {
    client->text(stateJson());
    return;
  }
  if (type != WS_EVT_DATA) return;
  auto *info = static_cast<AwsFrameInfo *>(arg);
  if (!info->final || info->index != 0 || info->len != len || info->opcode != WS_TEXT) return;

  JsonDocument d;
  if (deserializeJson(d, data, len)) return;
  if (d["preset"].is<int>()) {
    int i = d["preset"];
    if (i >= 0 && i < NUM_PRESETS) dmxSetManual(cfg.presets[i].fan, cfg.presets[i].pump);
  }
  dmxSetManual(d["fan"] | -1, d["pump"] | -1);
  if (d["syncMaster"].is<bool>()) syncRequestMaster(d["syncMaster"]);
}

static void handleNotFound(AsyncWebServerRequest *r) {
  // Captive portal: redirect any foreign host requested via the AP interface
  if (netApActive() && r->client()->localIP() == WiFi.softAPIP()) {
    String apIp = WiFi.softAPIP().toString();
    if (r->host() != apIp) {
      r->redirect("http://" + apIp + "/");
      return;
    }
  }
  r->send(404, "text/plain", "Not found");
}

static void page(const char *path, const char *html) {
  server.on(path, HTTP_GET, [html](AsyncWebServerRequest *r) { r->send(200, "text/html", html); });
}

void webBegin() {
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  page("/", INDEX_HTML);
  page("/wifi", WIFI_HTML);
  page("/dmx", DMX_HTML);
  page("/system", SYSTEM_HTML);
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *r) {
    AsyncWebServerResponse *resp = r->beginResponse(200, "text/css", STYLE_CSS);
    resp->addHeader("Cache-Control", "max-age=3600");
    r->send(resp);
  });
  server.on("/common.js", HTTP_GET, [](AsyncWebServerRequest *r) {
    AsyncWebServerResponse *resp = r->beginResponse(200, "application/javascript", COMMON_JS);
    resp->addHeader("Cache-Control", "max-age=3600");
    r->send(resp);
  });

  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *r) { r->send(200, "application/json", stateJson()); });
  server.on("/api/config", HTTP_GET, handleConfig);
  server.on("/api/info", HTTP_GET, handleInfo);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/wifi", HTTP_POST, handleSaveWifi);
  server.on("/api/dmx", HTTP_POST, handleSaveDmx);
  server.on("/api/name", HTTP_GET, [](AsyncWebServerRequest *r) {
    JsonDocument d;
    d["name"] = cfg.deviceName;
    d["hostname"] = cfg.hostname;
    sendJson(r, d);
  });
  server.on("/api/name", HTTP_POST, handleSaveName);
  server.on("/api/reboot", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (rejectIfBusy(r)) return;
    schedule(Pending::Reboot);
    r->send(200, "text/plain", "Rebooting...");
  });
  server.on("/api/factory", HTTP_POST, [](AsyncWebServerRequest *r) {
    if (rejectIfBusy(r)) return;
    schedule(Pending::Factory);
    r->send(200, "text/plain", "Factory reset. Rebooting into AP mode...");
  });
  server.on("/update", HTTP_POST, handleOtaDone, handleOtaChunk);

  server.onNotFound(handleNotFound);
  server.begin();
}

void webLoop() {
  static uint32_t lastCheckMs = 0;
  static uint32_t lastSentMs = 0;
  static String lastState;

  // Give the HTTP response time to go out before rebooting
  bool due = millis() - pendingAtMs > 500;
  switch (pending) {
    case Pending::ApplyLive:
      pendingCfg.syncMaster = cfg.syncMaster;  // toggled from the Control page in the meantime
      cfg = pendingCfg;
      settingsSave();
      inputBegin();
      syncBegin();
      pending = Pending::None;
      break;
    case Pending::ApplyWifi:
      if (due) {
        pendingCfg.syncMaster = cfg.syncMaster;
        cfg = pendingCfg;
        settingsSave();
        ESP.restart();
      }
      break;
    case Pending::Reboot:
      if (due) ESP.restart();
      break;
    case Pending::Factory:
      if (due) settingsFactoryReset();
      break;
    default:
      break;
  }

  if (millis() - lastCheckMs < 100) return;
  lastCheckMs = millis();
  ws.cleanupClients();
  if (ws.count() == 0) return;
  String s = stateJson();
  if (s != lastState || millis() - lastSentMs > 2000) {
    ws.textAll(s);
    lastState = s;
    lastSentMs = millis();
  }
}
