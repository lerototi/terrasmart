from pathlib import Path
import re


Import("env")


def patch_fdrs_time(*_args, **_kwargs) -> None:
    project_dir = Path(env.subst("$PROJECT_DIR"))
    for header in project_dir.glob(".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_time.h"):
        source = header.read_text()
        original = source
        source = source.replace(
            "if(timeinfo.tm_mon == 2) {\n      struct tm dstBegin;",
            "if(timeinfo.tm_mon == 2) {\n      time_t tdstBegin = -1;\n      struct tm dstBegin;",
        )
        source = source.replace(
            "time_t tdstBegin = mktime(&dstBegin);",
            "tdstBegin = mktime(&dstBegin);",
        )
        if "#ifdef FDRS_NO_DST" not in source:
            source = source.replace(
                "void checkDST() {\n",
                "void checkDST() {\n"
                "#ifdef FDRS_NO_DST\n"
                "  isDST = false;\n"
                "  return;\n"
                "#endif\n",
                1,
            )
        if source != original:
            header.write_text(source)


def patch_fdrs_espnow(*_args, **_kwargs) -> None:
    project_dir = Path(env.subst("$PROJECT_DIR"))
    for header in project_dir.glob(".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_gateway_espnow.h"):
        source = header.read_text()
        original = source
        source = source.replace(
            "esp_now_peer_info_t peerInfo;",
            "esp_now_peer_info_t peerInfo = {};",
        )
        source = re.sub(
            r"(peerInfo\.ifidx = WIFI_IF_STA;\n)(\s*peerInfo\.channel = 0;)",
            r"\1   peerInfo.channel = 0;",
            source,
        )
        source = source.replace(
            "#endif\n  peerInfo.channel = 0;\n  peerInfo.encrypt = false;",
            "#endif\n  peerInfo.ifidx = WIFI_IF_STA;\n  peerInfo.channel = 0;\n  peerInfo.encrypt = false;",
            1,
        )
        if "void sendRegistrationReply" not in source:
            source = source.replace(
                "extern time_t now;\n",
                "extern time_t now;\n\n"
                "void sendRegistrationReply(uint8_t *mac)\n"
                "{\n"
                "  SystemPacket sys_packet = {.cmd = cmd_add, .param = PEER_TIMEOUT};\n"
                "  for (uint8_t attempt = 0; attempt < 3; ++attempt)\n"
                "  {\n"
                "    const esp_err_t sendResult = esp_now_send(mac, (uint8_t *)&sys_packet, sizeof(SystemPacket));\n"
                "    DBG(sendResult == ESP_OK ? \"Registration reply queued.\" : \"Registration reply failed.\");\n"
                "    delay(30);\n"
                "  }\n"
                "}\n",
                1,
            )
        source = source.replace(
            "esp_now_sent_flag = true;\n}",
            "esp_now_sent_flag = true;\n#if defined(ESP32)\n  DBG(status == ESP_NOW_SEND_SUCCESS ? \"ESP-NOW send succeeded.\" : \"ESP-NOW send failed.\");\n#endif\n}",
        )
        source = re.sub(
            r"    SystemPacket sys_packet = \{\.cmd = cmd_add, \.param = PEER_TIMEOUT\};\n"
            r"    (?:const esp_err_t sendResult = )?esp_now_send\(incMAC, \(uint8_t \*\)&sys_packet, sizeof\(SystemPacket\)\);\n"
            r"    (?:DBG\(sendResult == ESP_OK \? \"Registration (?:reply|refresh reply) queued\.\" : \"Registration (?:reply|refresh reply) failed\.\"\);\n)?",
            "    sendRegistrationReply(incMAC);\n",
            source,
        )
        if source != original:
            header.write_text(source)


def patch_fdrs_espnow_packet_lengths(*_args, **_kwargs) -> None:
    """Validate packet sizes before copying ESP-NOW payloads."""
    project_dir = Path(env.subst("$PROJECT_DIR"))
    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_gateway_espnow.h"
    ):
        source = header.read_text()
        original = source
        source = source.replace(
            """  if (len < sizeof(DataReading))
  {
    DBG1("Incoming ESP-NOW System Packet from 0x" + String(incMAC[5], HEX));
    memcpy(&theCmd, incomingData, sizeof(theCmd));
    // processing is handled in the handlecommands() function in gateway.h - do not process here
    return;
  }
  else {
    memcpy(&theData, incomingData, sizeof(theData));
    DBG("Incoming ESP-NOW DataReading from 0x" + String(incMAC[5], HEX));
    ln = len / sizeof(DataReading);
""",
            """  if (len == sizeof(SystemPacket))
  {
    DBG1("Incoming ESP-NOW System Packet from 0x" + String(incMAC[5], HEX));
    memcpy(&theCmd, incomingData, sizeof(theCmd));
    // processing is handled in the handlecommands() function in gateway.h - do not process here
    return;
  }
  else if (len == 0 || len > sizeof(theData) || len % sizeof(DataReading) != 0)
  {
    DBG("Ignoring invalid ESP-NOW packet length: " + String(len));
    return;
  }
  else {
    memcpy(theData, incomingData, len);
    DBG("Incoming ESP-NOW DataReading from 0x" + String(incMAC[5], HEX));
    ln = len / sizeof(DataReading);
""",
            1,
        )
        if source != original:
            header.write_text(source)

    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_node_espnow.h"
    ):
        source = header.read_text()
        original = source
        source = source.replace(
            """    else if((len == sizeof(DataReading)))
    {
        memcpy(&theData, incomingData, len);
        ln = len / sizeof(DataReading);
        DBG2("Incoming ESP-NOW DataReading from 0x" + String(incMAC[5], HEX));
        newData = event_espnowg;
        // Processing done by handleIncoming() in fdrs_node.h
    }
""",
            """    else if (len > 0 && len <= sizeof(theData) && len % sizeof(DataReading) == 0)
    {
        memcpy(theData, incomingData, len);
        ln = len / sizeof(DataReading);
        DBG2("Incoming ESP-NOW DataReading from 0x" + String(incMAC[5], HEX));
        newData = event_espnowg;
        // Processing done by handleIncoming() in fdrs_node.h
    }
""",
            1,
        )
        if source != original:
            header.write_text(source)


def patch_fdrs_node_send_timeout(*_args, **_kwargs) -> None:
    """Reset the ACK before sending and bound the wait for its callback."""
    project_dir = Path(env.subst("$PROJECT_DIR"))
    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_node.h"
    ):
        source = header.read_text()
        original = source
        source = source.replace(
            """  esp_now_send(gatewayAddress, (uint8_t *)&fdrsData, data_count * sizeof(DataReading));
  esp_now_ack_flag = CRC_NULL;
  while (esp_now_ack_flag == CRC_NULL)
""",
            """  // Clear the previous result before the asynchronous send callback can run.
  esp_now_ack_flag = CRC_NULL;
  const unsigned long ackStart = millis();
  esp_now_send(gatewayAddress, (uint8_t *)&fdrsData, data_count * sizeof(DataReading));
  while (esp_now_ack_flag == CRC_NULL && millis() - ackStart < 1000)
""",
            1,
        )
        source = source.replace(
            """  esp_now_send(gatewayAddress, (uint8_t *)&fdrsData, data_count * sizeof(DataReading));
  esp_now_ack_flag = CRC_NULL;
  const unsigned long ackStart = millis();
  while (esp_now_ack_flag == CRC_NULL && millis() - ackStart < 1000)
""",
            """  // Clear the previous result before the asynchronous send callback can run.
  esp_now_ack_flag = CRC_NULL;
  const unsigned long ackStart = millis();
  esp_now_send(gatewayAddress, (uint8_t *)&fdrsData, data_count * sizeof(DataReading));
  while (esp_now_ack_flag == CRC_NULL && millis() - ackStart < 1000)
""",
            1,
        )
        if source != original:
            header.write_text(source)


def patch_fdrs_espnow_channel(*_args, **_kwargs) -> None:
    project_dir = Path(env.subst("$PROJECT_DIR"))
    headers = list(project_dir.glob(".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_*espnow.h"))
    headers.extend(project_dir.glob(".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_node.h"))
    for header in headers:
        source = header.read_text()
        original = source
        channel_setup = (
            "WiFi.disconnect();\n"
            "#ifdef FDRS_ESPNOW_CHANNEL\n"
            "#if defined(ESP8266)\n"
            "wifi_set_channel(FDRS_ESPNOW_CHANNEL);\n"
            "#elif defined(ESP32)\n"
            "esp_wifi_set_channel(FDRS_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);\n"
            "#endif\n"
            "DBG(\"ESP-NOW channel: \" + String(FDRS_ESPNOW_CHANNEL));\n"
            "#endif"
        )
        if "FDRS_ESPNOW_CHANNEL" not in source:
            source = source.replace("WiFi.disconnect();", channel_setup, 1)
        if source != original:
            header.write_text(source)


def patch_fdrs_node_espnow(*_args, **_kwargs) -> None:
    project_dir = Path(env.subst("$PROJECT_DIR"))
    for header in project_dir.glob(".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_node_espnow.h"):
        source = header.read_text()
        original = source
        source = source.replace(
            "memcpy(&incMAC, mac, sizeof(incMAC));\n    if (len == sizeof(SystemPacket))",
            "memcpy(&incMAC, mac, sizeof(incMAC));\n    DBG2(\"Incoming ESP-NOW packet length: \" + String(len));\n    if (len == sizeof(SystemPacket))",
            1,
        )
        if "System command received:" not in source:
            source = source.replace(
                "DBG2(\"Incoming ESP-NOW System Packet from 0x\" + String(incMAC[5], HEX));",
                "DBG2(\"Incoming ESP-NOW System Packet from 0x\" + String(incMAC[5], HEX));\n         DBG(\"System command received: \" + String(command.cmd) + \" param: \" + String(command.param));",
                1,
            )
        source = re.sub(
            r"(\s*DBG\(\"System command received: \" \+ String\(command\.cmd\) \+ \" param: \" \+ String\(command\.param\)\);\n)\s*DBG\(\"System command received: \" \+ String\(command\.cmd\) \+ \" param: \" \+ String\(command\.param\)\);",
            r"\1",
            source,
        )
        if source != original:
            header.write_text(source)


def patch_fdrs_topics(*_args, **_kwargs) -> None:
    project_dir = Path(env.subst("$PROJECT_DIR"))
    for header in project_dir.glob(".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_globals.h"):
        source = header.read_text()
        original = source
        source = source.replace(
            '#define TOPIC_DATA    "fdrs/data"',
            '#ifndef TOPIC_DATA\n#define TOPIC_DATA    "fdrs/data"\n#endif',
        )
        source = source.replace(
            '#define TOPIC_STATUS  "fdrs/status"',
            '#ifndef TOPIC_STATUS\n#define TOPIC_STATUS  "fdrs/status"\n#endif',
        )
        source = source.replace(
            '#define TOPIC_COMMAND "fdrs/command"',
            '#ifndef TOPIC_COMMAND\n#define TOPIC_COMMAND "fdrs/command"\n#endif',
        )
        if source != original:
            header.write_text(source)


def patch_runtime_gateway(*_args, **_kwargs) -> None:
    """Make the FDRS Wi-Fi/MQTT startup usable with runtime configuration."""
    project_dir = Path(env.subst("$PROJECT_DIR"))
    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_gateway_wifi.h"
    ):
        source = header.read_text()
        original = source
        begin_wifi = r'''void begin_wifi()
{
  delay(10);
#ifdef USE_ETHERNET
  WiFi.onEvent(WiFiEvent);
  ETH.begin();
    while (!eth_connected)
  {
    DBG("Connecting ethernet...");
    delay(500);
  }
#else
#ifdef USE_STATIC_IPADDRESS
  // Convert from String to byte array
  stringToByteArray(FDRS_HOST_IPADDRESS, '.', hostIpAddress, 4, 10);
  stringToByteArray(FDRS_GW_IPADDRESS, '.', gatewayAddress, 4, 10);
  stringToByteArray(FDRS_SUBNET_ADDRESS, '.', subnetAddress, 4, 10);
  stringToByteArray(FDRS_DNS1_IPADDRESS, '.', dns1Address, 4, 10);
  stringToByteArray(FDRS_DNS2_IPADDRESS, '.', dns2Address, 4, 10);
  WiFi.config(hostIpAddress, gatewayAddress, subnetAddress, dns1Address, dns2Address);
#endif
  WiFi.begin(ssid, password);
  DBG("Connecting to WiFi SSID: " + String(FDRS_WIFI_SSID));
  int connectTries = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    connectTries++;
    delay(1000);
    if(connectTries >= 10) {
      DBG("Couldn't connect! Retrying...");
      WiFi.reconnect();
    }
  }
#endif // USE_ETHERNET
}'''
        runtime_wifi = r'''void begin_wifi()
{
  delay(10);
#ifdef USE_ETHERNET
  WiFi.onEvent(WiFiEvent);
  ETH.begin();
    while (!eth_connected)
  {
    DBG("Connecting ethernet...");
    delay(500);
  }
#else
#ifdef FDRS_RUNTIME_CONFIG
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(fdrs_runtime_ap_ssid, fdrs_runtime_ap_password);
  DBG("Setup AP started at 192.168.4.1.");
  if (ssid == nullptr || ssid[0] == '\0') {
    DBG("WiFi STA is not configured; AP-only mode active.");
    return;
  }
#else
  WiFi.mode(WIFI_STA);
#endif
#ifdef USE_STATIC_IPADDRESS
  // Convert from String to byte array
  stringToByteArray(FDRS_HOST_IPADDRESS, '.', hostIpAddress, 4, 10);
  stringToByteArray(FDRS_GW_IPADDRESS, '.', gatewayAddress, 4, 10);
  stringToByteArray(FDRS_SUBNET_ADDRESS, '.', subnetAddress, 4, 10);
  stringToByteArray(FDRS_DNS1_IPADDRESS, '.', dns1Address, 4, 10);
  stringToByteArray(FDRS_DNS2_IPADDRESS, '.', dns2Address, 4, 10);
  WiFi.config(hostIpAddress, gatewayAddress, subnetAddress, dns1Address, dns2Address);
#endif
  WiFi.begin(ssid, password);
  DBG("Connecting to WiFi SSID: " + String(ssid));
#ifdef FDRS_RUNTIME_CONFIG
  const unsigned long connectStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - connectStart < 15000) {
    delay(250);
  }
  if (WiFi.status() != WL_CONNECTED) {
    DBG("WiFi connection timed out; setup AP remains active.");
  }
#else
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    WiFi.reconnect();
  }
#endif
#endif // USE_ETHERNET
}'''
        source = re.sub(
            r"void begin_wifi\(\)\n\{.*?\n\}\n\n// send an NTP request",
            lambda _match: runtime_wifi + "\n\n// send an NTP request",
            source,
            count=1,
            flags=re.DOTALL,
        )
        if source != original:
            header.write_text(source)

    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_gateway_mqtt.h"
    ):
        source = header.read_text()
        original = source
        source = source.replace(
            'if (client.connect("FDRS_GATEWAY", mqtt_user, mqtt_pass))',
            'if (client.connect("FDRS_GATEWAY", mqtt_user, mqtt_pass, TOPIC_STATUS, 0, true, "{\\"event\\":\\"offline\\"}"))',
        )
        if source != original:
            header.write_text(source)

    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_gateway_ota.h"
    ):
        source = header.read_text()
        original = source
        source = source.replace(
            '    ArduinoOTA.setHostname("FDRSGW");',
            '    ArduinoOTA.setHostname("terrasmart-mqtt-gateway");\n'
            '    ArduinoOTA.setPassword(FDRS_OTA_PASSWORD);',
            1,
        )
        if source != original:
            header.write_text(source)

    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_gateway_mqtt.h"
    ):
        source = header.read_text()
        # Clear the retry source byte before each begin so a lost ACK cannot
        # carry into the next packet and cause a duplicate/stale ACK.
        source = re.sub(
            r"if \(strcmp\(topic, TERRASMART_OTA_TOPIC\) == 0\) \{\n"
            r"\s*terrasSmartOtaHandleMqttCommand\(message, length\);\n"
            r"\s*return;\n\s*\}\n",
            "",
            source,
        )
        callback = (
            "void mqtt_callback(char *topic, byte *message, unsigned int length)\n{\n"
            "    if (strcmp(topic, TERRASMART_OTA_TOPIC) == 0) {\n"
            "        terrasSmartOtaHandleMqttCommand(message, length);\n"
            "        return;\n"
            "    }\n"
        )
        source = re.sub(
            r"void mqtt_callback\(char \*topic, byte \*message, unsigned int length\)\n\{\n",
            lambda _match: callback,
            source,
            count=1,
        )
        header.write_text(source)

    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_gateway.h"
    ):
        source = header.read_text()
        original = source
        source = re.sub(
            r"#if defined\(TERRASMART_OTA_ESPNOW_GATEWAY\)\n  serviceOtaUart\(\);\n#endif\n",
            "",
            source,
        )
        if source != original:
            header.write_text(source)

def normalize_fdrs_serial_ota(*_args, **_kwargs) -> None:
    """Replace the upstream line parser with a bounded, OTA-aware parser."""
    project_dir = Path(env.subst("$PROJECT_DIR"))
    replacement = r'''void getSerial() {
  Stream *input = nullptr;
  if (UART_IF.available()) input = &UART_IF;
  else if (Serial.available()) input = &Serial;
  if (input == nullptr) return;

  String incomingString = input->readStringUntil((char)10);
  if (incomingString.startsWith("!TSOTA:")) {
    terrasSmartOtaHandleUartLine(incomingString.c_str());
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, incomingString);
  if (error) {
    DBG2("json parse err");
    DBG2(incomingString);
    return;
  }
  const int count = doc.size();
  if (count <= 0 || count > 256) return;
  JsonObject obj = doc[0].as<JsonObject>();
  if (obj.containsKey("type")) {
    for (int i = 0; i < count; ++i) {
      theData[i].id = doc[i]["id"];
      theData[i].t = doc[i]["type"];
      theData[i].d = doc[i]["data"];
    }
    ln = count;
    newData = event_serial;
  } else if (obj.containsKey("cmd")) {
    theCmd.cmd = doc[0]["cmd"];
    theCmd.param = doc[0]["param"];
  }
}

void getSerial_PLACEHOLDER()'''
    replacement = replacement.replace("\nvoid getSerial_PLACEHOLDER()", "")
    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_gateway_serial.h"
    ):
        source = header.read_text()
        updated = re.sub(
            r"void getSerial\(\) \{.*?\n\}\n(?=\s*void sendSerial\(\))",
            replacement + "\n",
            source,
            count=1,
            flags=re.DOTALL,
        )
        if updated != source:
            header.write_text(updated)


def patch_fdrs_initial_status(*_args, **_kwargs) -> None:
    project_dir = Path(env.subst("$PROJECT_DIR"))
    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_gateway.h"
    ):
        source = header.read_text()
        original = source
        source = source.replace(
            '  client.publish(TOPIC_STATUS, "FDRS initialized");\n',
            "",
        )
        if source != original:
            header.write_text(source)


def patch_fdrs_ota_transport(*_args, **_kwargs) -> None:
    """Route OTA UART frames and ESP-NOW frames into the application OTA layer."""
    project_dir = Path(env.subst("$PROJECT_DIR"))
    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_node_espnow.h"
    ):
        source = header.read_text()
        original = source
        source = re.sub(
            r"(void OnDataRecv\([^\n]+\)\n\{\n)(?!\s*if \(terrasSmartOtaCaptureEspNow)",
            r"\1    if (terrasSmartOtaCaptureEspNow(incomingData, len, mac)) return;\n",
            source,
        )
        if source != original:
            header.write_text(source)

    # Normalize the serial parser block after the other FDRS compatibility
    # patches. The pinned upstream header has no newline after the GPS endif
    # in some cached copies, which can swallow the following JSON declaration.
    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_gateway_serial.h"
    ):
        source = header.read_text()
        source = re.sub(
            r"#ifdef GPS_IF.*?#endif // GPS_IF[^\n]*",
            "#endif // GPS_IF",
            source,
            count=1,
            flags=re.DOTALL,
        )
        source = re.sub(
            r'\s*if \(incomingString\.startsWith\("!TSOTA:"\)\) \{.*?\n\s*\}\n?',
            "",
            source,
            count=1,
            flags=re.DOTALL,
        )
        source = source.replace(
            "#endif // GPS_IF\n  DeserializationError error =",
            "#endif // GPS_IF\n"
            '  if (incomingString.startsWith("!TSOTA:")) {\n'
            "    terrasSmartOtaHandleUartLine(incomingString.c_str());\n"
            "    return;\n"
            "  }\n"
            "  JsonDocument doc;\n  DeserializationError error =",
            1,
        )
        header.write_text(source)

    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_gateway_espnow.h"
    ):
        source = header.read_text()
        original = source
        source = re.sub(
            r"(void OnDataRecv\([^\n]+\)\n\{\n)(?!\s*if \(terrasSmartOtaCaptureEspNow)",
            r"\1  if (terrasSmartOtaCaptureEspNow(incomingData, len, mac)) return;\n",
            source,
        )
        if source != original:
            header.write_text(source)

    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_gateway_serial.h"
    ):
        source = header.read_text()
        original = source
        source = source.replace("}#endif // GPS_IF", "}\n#endif // GPS_IF")
        source = re.sub(
            r"#endif // GPS_IF[^\n]*"
            r"(?:\s*if \(incomingString\.startsWith\(\"!TSOTA:\"\)\) \{\n"
            r"\s*terrasSmartOtaHandleUartLine\(incomingString\.c_str\(\)\);\n"
            r"\s*return;\n\s*\}\n)?"
            r"\s*JsonDocument doc;",
            "#endif // GPS_IF\n"
            '  if (incomingString.startsWith("!TSOTA:")) {\n'
            "    terrasSmartOtaHandleUartLine(incomingString.c_str());\n"
            "    return;\n"
            "  }\n"
            "  JsonDocument doc;",
            source,
            count=1,
        )
        if source != original:
            header.write_text(source)

    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_gateway_mqtt.h"
    ):
        source = header.read_text()
        original = source
        source = source.replace(
            "            client.subscribe(TOPIC_COMMAND);",
            "            client.subscribe(TOPIC_COMMAND);\n"
            "            client.subscribe(TERRASMART_OTA_TOPIC);",
            1,
        )
        source = re.sub(
            r"(\s*client\.subscribe\(TERRASMART_OTA_TOPIC\);\n){2,}",
            "\n            client.subscribe(TERRASMART_OTA_TOPIC);\n",
            source,
        )
        source = re.sub(
            r'\s*if \(strcmp\(topic, TERRASMART_OTA_TOPIC\) == 0\) \{\n'
            r'\s*terrasSmartOtaHandleMqttCommand\(message, length\);\n'
            r'\s*return;\n\s*\}\n',
            "",
            source,
        )
        source = source.replace(
            "void mqtt_callback(char *topic, byte *message, unsigned int length)\n{\n",
            "void mqtt_callback(char *topic, byte *message, unsigned int length)\n{\n"
            "    if (strcmp(topic, TERRASMART_OTA_TOPIC) == 0) {\n"
            "        terrasSmartOtaHandleMqttCommand(message, length);\n"
            "        return;\n"
            "    }\n",
            1,
        )
        if source != original:
            header.write_text(source)

    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_gateway_ota.h"
    ):
        source = header.read_text()
        original = source
        source = source.replace(
            '    ArduinoOTA.setHostname("FDRSGW");',
            '    ArduinoOTA.setHostname("terrasmart-mqtt-gateway");\n'
            "    ArduinoOTA.setPassword(TERRASMART_OTA_PASSWORD);",
            1,
        )
        if source != original:
            header.write_text(source)

    for header in project_dir.glob(
        ".pio/libdeps/*/Farm-Data-Relay-System/src/fdrs_gateway_mqtt.h"
    ):
        source = header.read_text()
        original = source
        runtime_port = """const char *mqtt_server = FDRS_MQTT_ADDR;
#ifdef FDRS_RUNTIME_CONFIG
int mqtt_port = FDRS_MQTT_PORT;
#else
const int mqtt_port = FDRS_MQTT_PORT;
#endif


#ifdef FDRS_MQTT_AUTH"""
        source = re.sub(
            r"const char \*mqtt_server = FDRS_MQTT_ADDR;.*?#ifdef FDRS_MQTT_AUTH",
            runtime_port,
            source,
            count=1,
            flags=re.DOTALL,
        )

        runtime_mqtt_guard = """#ifdef FDRS_RUNTIME_CONFIG
    if (WiFi.status() != WL_CONNECTED)
        return;
#endif
"""
        source = re.sub(
            r"(?:#ifdef FDRS_RUNTIME_CONFIG\n    if \(WiFi\.status\(\) != WL_CONNECTED\)\n"
            r"        return;\n#endif\n)+",
            runtime_mqtt_guard,
            source,
            count=1,
        )
        if runtime_mqtt_guard not in source:
            source = source.replace(
                "void handleMQTT()\n{\n",
                "void handleMQTT()\n{\n" + runtime_mqtt_guard,
                1,
            )
        old_begin_mqtt = '''void begin_mqtt()
{
    client.setServer(mqtt_server, mqtt_port);
    client.setBufferSize(MQTT_MAX_BUFF_SIZE);

    if (!client.connected())
    {
        reconnect_mqtt(5);
    }
    client.setCallback(mqtt_callback);
}'''
        new_begin_mqtt = '''void begin_mqtt()
{
    client.setServer(mqtt_server, mqtt_port);
    client.setBufferSize(MQTT_MAX_BUFF_SIZE);
    client.setCallback(mqtt_callback);
#ifdef FDRS_RUNTIME_CONFIG
    if (WiFi.status() == WL_CONNECTED && mqtt_server != nullptr && mqtt_server[0] != '\\0')
        reconnect_mqtt(1);
#else
    if (!client.connected())
        reconnect_mqtt(5);
#endif
}'''
        if old_begin_mqtt in source:
            source = source.replace(old_begin_mqtt, new_begin_mqtt, 1)
        if source != original:
            header.write_text(source)


patch_fdrs_time()
patch_fdrs_espnow()
patch_fdrs_espnow_packet_lengths()
patch_fdrs_node_send_timeout()
patch_fdrs_espnow_channel()
patch_fdrs_node_espnow()
patch_fdrs_topics()
patch_runtime_gateway()
patch_fdrs_initial_status()
patch_fdrs_ota_transport()
normalize_fdrs_serial_ota()
env.AddPreAction("$BUILD_DIR/src/node_sensor.cpp.o", patch_fdrs_time)
env.AddPreAction("$BUILD_DIR/src/float_switch_node.cpp.o", patch_fdrs_time)
env.AddPreAction("$BUILD_DIR/src/gateway.cpp.o", patch_fdrs_time)
env.AddPreAction("$BUILD_DIR/src/mqtt_gateway.cpp.o", patch_fdrs_time)
env.AddPreAction("$BUILD_DIR/src/relay_node.cpp.o", patch_fdrs_time)
