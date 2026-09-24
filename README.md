# TerraSmart with FDRS

This project uses the [Farm Data Relay System](https://github.com/timmbogner/Farm-Data-Relay-System) library to exchange sensor data over ESP-NOW.

## Boards

The supported PlatformIO environments are:

- `d1_mini_node`: Wemos D1 mini sensor node based on ESP8266.
- `d1_mini_float_switch_node`: Wemos D1 mini node for two reservoir float switches.
- `d1_mini_relay_node`: Wemos D1 mini two-channel relay node based on ESP8266.
- `wemos_d1_mini32_node`: Wemos D1 mini32 sensor node based on ESP32.
- `esp32dev_gateway`: ESP32 DevKit V1 ESP-NOW gateway.
- `d1_mini_mqtt_gateway`: Wemos D1 mini Wi-Fi/MQTT front-end gateway.
- `esp32dev_mqtt_gateway`: ESP32 DevKit V1 Wi-Fi/MQTT front-end gateway.

The ESP32 MQTT gateway is the current deployment. The D1 mini MQTT gateway
remains supported as an alternative, and all MQTT gateway documentation below
applies to both environments.

## First test

1. Connect one ESP32 DevKit as the ESP-NOW gateway and upload `esp32dev_gateway`.
2. Open the serial monitor at `115200` baud.
3. Set `GTWY_MAC` in `include/terrasmart_node_config.h` to the gateway address, currently `0x01`.
4. Connect a second board and upload `d1_mini_node` (or the matching ESP32 node environment).
5. Check the gateway serial monitor for the received FDRS JSON readings.

### Wemos D1 mini node

For the Wemos sensor node, select `d1_mini_node` and upload through its USB connection:

```bash
pio run -e d1_mini_node -t upload
pio device monitor -b 115200
```

The node is already configured with `GTWY_MAC 0x01`, matching the ESP32
ESP-NOW gateway. Keep the ESP32 gateway powered during this test. The node
sends simulated temperature and humidity readings every 10 seconds. Do not
connect this node's TX/RX to the gateway UART; its radio link to the ESP32
gateway is ESP-NOW.

The relay node and the ESP32 ESP-NOW gateway are both fixed to radio channel 1
(`FDRS_ESPNOW_CHANNEL`). Upload both firmwares after changing this setting. The
relay monitor must show `ESP-NOW channel: 1`, and the gateway monitor must show
the same line before testing registration.

## Wi-Fi and MQTT gateways

The firmware in `src/gateway.cpp` is intentionally an ESP-NOW-only gateway. It does not connect to Wi-Fi, so there is no active SSID/password in `include/terrasmart_gateway_config.h`.

The radio gateway and MQTT gateway are separate devices. Upload
`esp32dev_gateway` to the ESP32 handling ESP-NOW. Upload exactly one of the
following MQTT environments to the second device:

- `esp32dev_mqtt_gateway` for the current ESP32 DevKit deployment.
- `d1_mini_mqtt_gateway` for the Wemos D1 mini alternative.

Do not upload either MQTT environment to the radio gateway. Both MQTT
environments intentionally define only `USE_WIFI` and cannot accept ESP-NOW
node registration requests.

In FDRS, do not enable `USE_WIFI` together with `USE_ESPNOW` in this gateway. To send data to Wi-Fi/MQTT, use a second gateway configured as an MQTT gateway and connect the two gateways through UART with crossed RX/TX pins:

```cpp
#define UNIT_MAC 0x00
#define USE_WIFI
#define WIFI_SSID "your-wifi-name"
#define WIFI_PASS "your-wifi-password"
#define MQTT_ADDR "192.168.1.10"
#define MQTT_PORT 1883
```

The ESP-NOW gateway forwards received readings through its serial interface;
the selected MQTT gateway publishes them to `terrasmart/data`.

The MQTT data payload is a JSON array, for example:

```json
[{"id":1,"type":1,"data":23.4},{"id":1,"type":3,"data":65.2}]
```

Subscribe to `terrasmart/data` in Home Assistant. `terrasmart/status` reports gateway status and `terrasmart/command` is used for commands.

The MQTT gateway publishes a retained JSON status message to `terrasmart/status`:

```json
{"event":"online","uptime_s":12,"wifi_connected":true,"ip":"10.0.0.50","rssi":-55,"mqtt_connected":true,"configured":true,"ap_ip":"192.168.4.1","mqtt_host":"10.0.0.216","mqtt_port":1883}
```

The `online` event is published after the gateway successfully connects to MQTT. A `heartbeat` event is published every 30 seconds. The MQTT Last Will replaces the retained message with `{"event":"offline"}` when the broker detects an unexpected disconnect. The retained message means a subscriber can connect after startup and still receive the latest status.

To monitor the status from a terminal:

```bash
mosquitto_sub -h 10.0.0.216 -p 1883 -u mqtt-user -P 'your-mqtt-password' -t terrasmart/status -v
```

The previous one-time `FDRS initialized` message was removed because it could be lost when no subscriber was connected during boot.

Deployment credentials belong in the ignored
`include/terrasmart_mqtt_gateway_secrets.h` file. Start with
`include/terrasmart_mqtt_gateway_secrets.h.example` and replace the placeholders
before building a deployment firmware. The ESP8266 Wemos D1 mini cannot
connect to a 5 GHz-only network; the ESP32 gateway also requires a compatible
2.4 GHz network for this configuration.

The built-in LED on an MQTT gateway is a hardware status indicator. It flashes
three times at startup, stays solid on when Wi-Fi and MQTT are connected,
blinks slowly when Wi-Fi is connected but MQTT is not, and blinks quickly when
Wi-Fi is not connected. On the ESP32 MQTT gateway, a medium blink indicates
that the setup AP is active.

### ESP32 MQTT gateway setup AP

The `esp32dev_mqtt_gateway` stores Wi-Fi and MQTT settings in ESP32 NVS using
`Preferences`. On the first boot, the values in
`include/terrasmart_mqtt_gateway_config.h` are migrated to NVS. Subsequent firmware
uploads keep the saved values until they are changed through the web page or
reset.

The ESP32 keeps a setup/status access point active while also trying to connect
to the configured Wi-Fi network:

```text
SSID: TerraSmart-Setup-XXXX
Password: the value configured by `FDRS_AP_PASSWORD`
Address: http://192.168.4.1/
```

The AP also provides captive-portal behavior: DNS requests are directed to
the gateway and common connectivity-check URLs are redirected to the setup
page. Compatible phones and computers should open the page automatically after
joining the AP. This is not guaranteed for every operating system, and HTTPS
URLs cannot be transparently redirected; use `http://192.168.4.1/` manually if
the notification does not appear.

`XXXX` is the last four hexadecimal characters of the ESP32 MAC address. The
page remains available at `http://192.168.4.1/` after the station connects. It
is also available at the STA IP shown on the page and, when mDNS is supported
by the client, at `http://terrasmart-gateway.local/`.

The ESP32 has one 2.4 GHz radio, so the permanent AP follows the Wi-Fi channel
used by the STA connection. This is expected in `WIFI_AP_STA` mode; the AP is
intended for status and configuration, not high-throughput traffic.

The ESP32 has one 2.4 GHz radio, so the permanent AP follows the Wi-Fi channel
used by the STA connection. This is expected in `WIFI_AP_STA` mode; the AP is
intended for status and configuration, not high-throughput traffic.
The page allows changing:

- Wi-Fi network selected from a scan with signal strength and password;
- MQTT broker address and port;
- MQTT username and password.

The initial MQTT defaults are the values currently used by this project:
`10.0.0.216`, port `1883`, user `mqtt-user`, and the configured MQTT password.
The Wi-Fi network must still be selected and its password entered during setup.

After saving, the gateway keeps the AP available, disconnects the old STA
connection, and tries the new settings in the background. The status page
reports the AP IP, STA IP, RSSI, mDNS, and MQTT state.

To erase the saved configuration, use **Apagar configuracao e voltar ao AP**
on the page. The ESP32 MQTT board also supports a hardware reset: with the
firmware already running, hold GPIO0 (`BOOT`) low for at least three seconds,
then release it. The gateway clears the NVS and starts the setup AP without
requiring a new firmware upload. Do not hold GPIO0 low while resetting the
board if that would put the ESP32 into its serial bootloader.

The built-in LED behavior for the ESP32 MQTT gateway is:

- solid: Wi-Fi and MQTT connected;
- slow blink: Wi-Fi connected but MQTT disconnected;
- medium blink: setup AP active;
- fast blink: no Wi-Fi and no setup AP.

The AP password is configured as `FDRS_AP_PASSWORD` in the ignored secrets
file. Do not commit that file.

For the D1 mini MQTT gateway, disconnect the TX/RX wires to the ESP32 first.
Select the serial port that disappears when the Wemos is unplugged, then
upload at 115200 baud. `/dev/ttyUSB0` is an FT232 adapter in the current setup,
while `/dev/ttyACM0` is another USB serial device; do not assume either one is
the Wemos without checking. The ESP32 MQTT gateway uses its USB serial port
for monitoring, so its UART link can remain connected during normal monitoring.

Connect UART lines crossed and connect grounds together:

```text
ESP-NOW gateway TX -> MQTT gateway RX
ESP-NOW gateway RX <- MQTT gateway TX
ESP-NOW gateway GND -> MQTT gateway GND
```

For the current ESP32-to-ESP32 installation, upload `esp32dev_gateway` to the
ESP-NOW ESP32 and `esp32dev_mqtt_gateway` to the MQTT ESP32. Both boards use a
dedicated link on GPIO16/GPIO17, leaving each USB serial monitor available.

The D1 mini MQTT gateway is also supported. In that alternative, upload
`d1_mini_mqtt_gateway` to the Wemos D1 mini. Its main serial interface is used
for the UART link, so its FDRS debug output is not available while the link is
connected.

Current ESP32-to-ESP32 wiring:

```text
ESP-NOW ESP32 GPIO17 (TX) -> MQTT ESP32 GPIO16 (RX)
ESP-NOW ESP32 GPIO16 (RX) <- MQTT ESP32 GPIO17 (TX)
ESP-NOW ESP32 GND         -> MQTT ESP32 GND
```

Alternative ESP32-to-D1-mini wiring:

```text
ESP-NOW ESP32 GPIO17 (TX) -> D1 mini RX (GPIO3/RX)
ESP-NOW ESP32 GPIO16 (RX) <- D1 mini TX (GPIO1/TX)
ESP-NOW ESP32 GND         -> D1 mini GND
```

Disconnect the TX/RX wires from the D1 mini while uploading firmware to it,
because its main UART is also the USB upload/monitor interface. Reconnect them
after the upload. The ESP32 MQTT gateway does not require this disconnection.
Use only 3.3 V logic levels.

If automatic reset does not enter bootloader mode, hold the Wemos `FLASH/BOOT` button, start the upload, release it when `Connecting...` appears, and press `RST` once. If the board has no BOOT button, briefly connect GPIO0 to GND while resetting, then remove the connection after upload begins.

In VS Code, select the environment from the PlatformIO status bar, then use **Upload**. The serial monitor is available with **Monitor**.

## Network addresses

FDRS derives the ESP-NOW MAC addresses from the prefix in its `fdrs_globals.h` and the final byte configured here. Every device in this test network must use the same prefix and each gateway must have a unique `UNIT_MAC`.

The `d1_mini_node` reads an A02YYUW ultrasonic sensor every 10 seconds. The sensor frame checksum is validated before sending. The distance is published as `LEVEL_T` in millimeters with the node's `READING_ID`.

Connect the A02YYUW to the Wemos node as follows:

```text
A02YYUW TX -> Wemos D6 (GPIO12)
A02YYUW RX -> Wemos D7 (GPIO13)
A02YYUW GND -> Wemos GND
A02YYUW VCC -> suitable sensor supply
```

The sensor UART uses `9600 8N1`. If the sensor is powered at 5 V, confirm that its TX signal is safe for the Wemos 3.3 V input; use a level shifter or divider if necessary. The sensor RX wire is only needed if you configure the sensor through UART, but it is included above for the D6/D7 connection requested.

## Two float switches node

For the reservoir minimum and maximum switches, select `d1_mini_float_switch_node`:

```bash
pio run -e d1_mini_float_switch_node -t upload
pio device monitor -b 115200
```

Wire one terminal of each switch to GND and the other terminals as follows:

```text
Minimum level switch -> D6 (GPIO12)
Maximum level switch -> D7 (GPIO14)
```

The inputs use the Wemos internal pull-up. An open switch is HIGH and an
activated (closed) switch is LOW. The node sends both values every 10 seconds
as `LEVEL_T`: reading ID `2` is the minimum switch and reading ID `3` is the
maximum switch. Each value is `1.0` when activated and `0.0` when open.

## Deep sleep

Do not enable `DEEP_SLEEP` for the first test. On ESP8266, deep sleep needs GPIO16 connected to RST; otherwise the node will not wake up.
