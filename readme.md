# UniqueWDMX

UniqueWDMX is an ESP32-based DMX512 controller and network-to-DMX bridge for Look Unique 2.0 hazers. It provides a web interface for manual fan and pump control, accepts Art-Net and sACN (E1.31) input, supports passthrough of an external DMX source, and can synchronize fan/pump settings across multiple devices.

The firmware sends DMX at approximately 40 Hz and stores configuration in the ESP32 NVS flash.

## Features

- Manual control of fan and pump values from a responsive web interface
- Five configurable quick-select presets
- Art-Net input on UDP port `6454`
- sACN/E1.31 input on UDP port `5568`
- Configurable DMX base address
- External DMX passthrough using an SPDT switch
- Wi-Fi station mode, access-point mode, and captive-portal configuration
- Static IP and custom mDNS hostname support
- Device-to-device UDP synchronization in 8 selectable groups
- Reboot, factory reset, and web-based `.bin` firmware updates
- Status LED for network and input activity

## Operating Modes

The output source priority is:

1. External DMX
2. Valid Art-Net or sACN input
3. Local manual control
4. Synchronized control from a sync master

External DMX and network input have priority over manual control and sync. When network input expires, the configured input timeout is used to fall back to manual control.

### Manual mode

Set fan and pump values from the Control page or select a preset. In manual mode the firmware sends only the fan and pump channels at the configured DMX base address; all other channels are set to zero.

Each preset can have a custom name and separate fan/pump values.

### External DMX mode

External DMX mode routes the physical DMX input directly through the SPDT switch and pauses the ESP32 UART output.

- Short press the AP/mode button to toggle between external and internal mode.
- Power-on default: external DMX passthrough

### Art-Net mode

Art-Net input is enabled from the **DMX / ArtNet** page. Configure:

- Net: `0-127`
- Subnet: `0-15`
- Universe: `0-15`
- Input timeout: `1-60` seconds

The firmware listens on UDP port `6454`, responds to ArtPoll requests, and passes valid DMX universe data directly to the DMX512 output.

### sACN mode

sACN/E1.31 input is enabled from the **DMX / ArtNet** page. Configure a universe from `1-63999`. The device joins the corresponding multicast group and accepts data using the null start code.

## Wi-Fi Configuration

On first boot or after Wi-Fi is not configured, the ESP32 starts an access point:

| Setting | Default |
|---|---|
| SSID | `UniqueWDMX-AP` |
| Password | `wdmx1234` |
| IP address | `4.3.2.1` |
| Hostname | `uniquewdmx.local` |

Connect to the access point and open `http://4.3.2.1/`. The captive portal redirects other host requests to the configuration page.

Use the **WiFi** page to:

- Scan for and configure a Wi-Fi network
- Set the WiFi password
- Set a static IP, gateway, subnet, and DNS
- Configure the access-point name, password, channel, and visibility
- Change the displayed device name

Changing the hostname or Wi-Fi network settings reboots the device as required. The web server runs on TCP port `80`.

## Sync

UniqueWDMX can synchronize manual fan/pump values between devices on the same IP network.

1. Configure the same sync UDP port on all devices.
2. Assign send and receive groups from the **DMX / ArtNet** page.
3. Enable **Sync Master** on the master device from the Control page.
4. Set the master to manual mode. Sync is paused while network or external DMX input has priority.

The default sync port is `21330`. It differs from the Art-Net and sACN ports. Sync packets are sent to the subnet broadcast address.

- Repeat count: `1-10` UDP packets per change for challanging network environments
- Keepalive interval: 1 second
- Receive timeout: 3 seconds
- Groups: 1-8

## Hardware

The firmware targets a standard ESP32 DevKit  board and defines these pins:

| Function | GPIO |
|---|---:|
| DMX TX | 17 |
| DMX RX | 16 |
| DMX DE / `/RE` | 4 |
| Status LED | 2 |
| Factory-reset / BOOT button | 0 |
| AP and DMX-mode button | 32 |
| SPDT DMX mode control | 33 |

The DMX transmitter/receiver is the RS485 stage in this configuration:

- GPIO 17: RS485 transmitter TX
- GPIO 16: RS485 receiver RX
- GPIO 4: RS485 driver-enable and receiver-enable, with DE and `/RE` tied together

## Controls and Status LED

### AP/mode button

- Short press, `50 ms-1000 ms`: toggle external DMX and internal control
- Hold for 5 seconds: force the Wi-Fi access point on/off

### BOOT/reset button

- Hold for 5 seconds: erase saved settings and reboot in access-point mode

The BOOT button flashes the status LED rapidly while reset is pending.

### Status LED

| State | Meaning |
|---|---|
| Solid | Connected to a Wi-Fi network |
| Slow blink | Access-point mode |
| Fast blink | Wi-Fi connection is unavailable |
| Fast alternating flicker | Art-Net or sACN input is active |

### Signal Switch LED
Is connected to the SPDT DMX mode control pin
| State | Meaning |
|---|---|
| Off | DMX is wired from the physical inputs to the Hazer internal control |
| Solid | DMX is generated by the ESP and wired to the Hazer internal control |

