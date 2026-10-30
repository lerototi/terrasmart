import re


def patch_mqtt_connect_identity(source: str) -> str:
    """Use the gateway-specific MQTT identity and retained status Last Will."""
    replacement = (
        "client.connect(terrasSmartMqttClientId(), mqtt_user, mqtt_pass, "
        "terrasSmartMqttStatusTopic(), 0, true, "
        "terrasSmartMqttOfflinePayload())"
    )
    return re.sub(
        r"if \(client\.connect\(.*\)\)",
        "if (" + replacement + ")",
        source,
        count=1,
    )


def patch_mqtt_command_subscription(source: str) -> str:
    reconnect = r'''void reconnect_mqtt(short int attempts, bool silent)
{
    if (!silent)
        DBG("Connecting MQTT...");

    for (short int i = 1; i <= attempts; i++)
    {
        if (client.connect(terrasSmartMqttClientId(), mqtt_user, mqtt_pass,
                           terrasSmartMqttStatusTopic(), 0, true,
                           terrasSmartMqttOfflinePayload()))
        {
#ifdef FDRS_RUNTIME_CONFIG
            client.subscribe(terrasSmartMqttCommandTopic());
#else
            client.subscribe(TOPIC_COMMAND);
#endif
            client.subscribe(TERRASMART_OTA_TOPIC);
            if (!silent)
                DBG(" MQTT Connected");
            return;
        }
        else
        {
            if (!silent)
            {
                char msg[23];
                sprintf(msg, " Attempt %d/%d", i, attempts);
                DBG(msg);
            }
            if ((attempts != 1))
                delay(3000);
        }
    }

    if (!silent)
        DBG(" Connecting MQTT failed.");
}

void reconnect_mqtt(int attempts)'''
    source = re.sub(
        r"void reconnect_mqtt\(short int attempts, bool silent\).*?"
        r"\nvoid reconnect_mqtt\(int attempts\)",
        reconnect,
        source,
        count=1,
        flags=re.DOTALL,
    )
    return source


def patch_mqtt_callback(source: str) -> str:
    callback = '''void mqtt_callback(char *topic, byte *message, unsigned int length)
{
    if (strcmp(topic, TERRASMART_OTA_TOPIC) == 0) {
        terrasSmartOtaHandleMqttCommand(message, length);
        return;
    }
#ifdef FDRS_RUNTIME_CONFIG
    if (terrasSmartMqttCommandTopicMatches(topic)) {
        terrasSmartMqttHandleGatewayCommand(message, length);
        return;
    }
#endif
    String incomingString;
    DBG(topic);
    for (unsigned int i = 0; i < length; ++i) {
        incomingString += static_cast<char>(message[i]);
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
    for (int i = 0; i < count; ++i) {
        theData[i].id = doc[i]["id"];
        theData[i].t = doc[i]["type"];
        theData[i].d = doc[i]["data"];
    }
    ln = count;
    newData = event_mqtt;
    DBG("Incoming MQTT.");
}
'''
    return re.sub(
        r"void mqtt_callback\(char \*topic, byte \*message, unsigned int length\)\s*\{.*?\n\}\n(?=\s*void begin_mqtt\()",
        callback,
        source,
        count=1,
        flags=re.DOTALL,
    )
