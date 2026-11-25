#pragma once

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

// RS485 transceiver (MAX485, DE and /RE tied together)
#define PIN_DMX_TX 17
#define PIN_DMX_RX 16
#define PIN_DMX_DE 4

#define PIN_STATUS_LED 2
#define PIN_RESET_BUTTON 0          // BOOT button, active low
#define FACTORY_RESET_HOLD_MS 5000
#define PIN_AP_BUTTON 32            // to GND, active low; short press = DMX mode, hold = AP
#define AP_BUTTON_HOLD_MS 5000
#define SHORT_PRESS_MIN_MS 50       // debounce
#define SHORT_PRESS_MAX_MS 1000

// SPDT DMX switch: LOW = external DMX passthrough (power-on default), HIGH = internal UART output
#define PIN_SPDT_CTRL 33

#define DMX_REFRESH_MS 25           // ~40 Hz frame rate

#define DEFAULT_HOSTNAME "uniquewdmx"
#define DEFAULT_AP_SSID "UniqueWDMX-AP"
#define DEFAULT_AP_PASS "wdmx1234"
#define AP_IP_ADDR 4, 3, 2, 1

#define WIFI_CONNECT_TIMEOUT_MS 15000   // open AP after this long without STA connection
#define WIFI_RETRY_INTERVAL_MS 60000    // retry STA while in AP mode (only if no AP clients)

#define DEFAULT_INPUT_TIMEOUT_S 5

#define NUM_PRESETS 5

#define DEFAULT_SYNC_PORT 21330  // UDP; differs from WLED's 21324 to avoid clashes
