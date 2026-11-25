#include "network.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include "config.h"
#include "settings.h"

static DNSServer dns;
static bool apActive = false;
static bool apForced = false;
static volatile bool apToggleRequested = false;
static bool staConnected = false;
static uint32_t staLostMs = 0;
static uint32_t lastRetryMs = 0;
static const IPAddress apIp(AP_IP_ADDR);

static bool hasSsid() { return cfg.ssid[0] != '\0'; }

static void startSta() {
  if (cfg.staticIp && cfg.ip != 0) {
    WiFi.config(IPAddress(cfg.ip), IPAddress(cfg.gateway), IPAddress(cfg.subnet), IPAddress(cfg.dns));
  }
  WiFi.begin(cfg.ssid, cfg.pass);
  Serial.printf("[NET] Connecting to '%s'\n", cfg.ssid);
}

static void startAp() {
  WiFi.mode(hasSsid() ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
  const char *pass = strlen(cfg.apPass) >= 8 ? cfg.apPass : nullptr;
  WiFi.softAP(cfg.apSsid, pass, cfg.apChannel, cfg.apHidden);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", apIp);
  apActive = true;
  lastRetryMs = millis();
  Serial.printf("[NET] AP '%s' started, IP %s\n", cfg.apSsid, apIp.toString().c_str());
}

static void stopAp() {
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apActive = false;
  Serial.println(F("[NET] AP stopped"));
}

void netBegin() {
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(cfg.hostname);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // lower latency for ArtNet

  staLostMs = millis();
  if (hasSsid()) startSta();
  else startAp();

  if (MDNS.begin(cfg.hostname)) {
    MDNS.addService("http", "tcp", 80);
  }
}

bool netLoop() {
  bool changed = false;
  bool sta = WiFi.status() == WL_CONNECTED;

  if (sta != staConnected) {
    staConnected = sta;
    changed = true;
    if (sta) {
      Serial.printf("[NET] Connected, IP %s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println(F("[NET] Connection lost"));
      staLostMs = millis();
    }
  }

  if (apToggleRequested) {
    apToggleRequested = false;
    apForced = !apForced;
    Serial.printf("[NET] Manual AP %s\n", apForced ? "on" : "off");
    if (apForced && !apActive) {
      startAp();
      changed = true;
    } else if (!apForced && apActive && sta) {
      stopAp();
      changed = true;
    }
  }

  if (apActive) {
    dns.processNextRequest();
    bool apClients = WiFi.softAPgetStationNum() > 0;
    if (sta && !apClients && !apForced) {
      stopAp();
      changed = true;
    } else if (!sta && hasSsid() && !apClients && millis() - lastRetryMs > WIFI_RETRY_INTERVAL_MS) {
      // Channel hopping while scanning would disturb AP clients, so only retry when none are connected
      lastRetryMs = millis();
      WiFi.disconnect();
      startSta();
    }
  } else if (!sta && millis() - staLostMs > WIFI_CONNECT_TIMEOUT_MS) {
    startAp();
    changed = true;
  }
  return changed;
}

bool netStaConnected() { return staConnected; }
bool netApActive() { return apActive; }
bool netApForced() { return apForced; }
void netToggleForcedAp() { apToggleRequested = true; }
