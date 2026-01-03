#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "captive_portal.h"
#include "../../include/setup_manager.h"

ESP8266WebServer server(80);
DNSServer dnsServer;
const char *AP_SSID = "terrasmart-setup";

extern SetupManager g_setupManager;

const char STEP1_HTML[] PROGMEM = "<!DOCTYPE html><html><head><title>TerraSmart</title><meta charset='UTF-8'><meta name='viewport' content='width=device-width'><style>body{font-family:Arial;background:#667eea;margin:0;padding:20px;min-height:100vh;display:flex;align-items:center;justify-content:center}.container{background:#fff;padding:40px;border-radius:12px;max-width:500px;width:100%}h1{color:#333}label{display:block;font-weight:600;margin-bottom:8px}select,input{width:100%;padding:12px;border:2px solid #e0e0e0;border-radius:6px;margin-bottom:20px;font-size:14px;box-sizing:border-box}button{width:100%;padding:14px;background:#667eea;color:#fff;border:none;border-radius:6px;cursor:pointer;margin:5px 0}.error{display:none;background:#fee;border-left:4px solid #f44;color:#c00;padding:12px;margin-bottom:20px}.success{display:none;background:#efe;border-left:4px solid #4a4;color:#0a0;padding:12px;margin-bottom:20px}</style></head><body><div class='container'><h1>WiFi</h1><div class='error' id='err'></div><div class='success' id='ok'></div><label>Network:</label><select id='ssid'><option>Loading...</option></select><label>Password:</label><input type='password' id='pass'><button onclick='loadNets()'>Reload</button><button onclick='testWifi()'>Continue</button></div><script>function loadNets(){fetch('/api/scan').then(r=>r.json()).then(d=>{var sel=document.getElementById('ssid');sel.innerHTML=d.networks.map(n=>'<option>'+n.ssid+'</option>').join('')}).catch(e=>alert('Error'))}function testWifi(){var s=document.getElementById('ssid').value;var p=document.getElementById('pass').value;if(!s||!p){document.getElementById('err').style.display='block';return}fetch('/api/wifi/test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s,pass:p})}).then(r=>r.json()).then(d=>{if(d.success){document.getElementById('ok').style.display='block';setTimeout(()=>location.href='/step/mqtt',1500)}else{document.getElementById('err').style.display='block'}}).catch(e=>{})}window.addEventListener('load',loadNets);</script></body></html>";

const char STEP2_HTML[] PROGMEM = "<!DOCTYPE html><html><head><title>TerraSmart</title><meta charset='UTF-8'><meta name='viewport' content='width=device-width'><style>body{font-family:Arial;background:#667eea;margin:0;padding:20px;min-height:100vh;display:flex;align-items:center;justify-content:center}.container{background:#fff;padding:40px;border-radius:12px;max-width:500px;width:100%}h1{color:#333}label{display:block;font-weight:600;margin-bottom:8px}select,input{width:100%;padding:12px;border:2px solid #e0e0e0;border-radius:6px;margin-bottom:20px;font-size:14px;box-sizing:border-box}button{width:100%;padding:14px;background:#667eea;color:#fff;border:none;border-radius:6px;cursor:pointer;margin:5px 0}.error{display:none;background:#fee;border-left:4px solid #f44;color:#c00;padding:12px;margin-bottom:20px}.success{display:none;background:#efe;border-left:4px solid #4a4;color:#0a0;padding:12px;margin-bottom:20px}</style></head><body><div class='container'><h1>MQTT</h1><div class='error' id='err'></div><div class='success' id='ok'></div><label>Host:</label><input type='text' id='host' placeholder='mqtt.example.com'><label>Port:</label><input type='number' id='port' value='1883'><label>User:</label><input type='text' id='user'><label>Pass:</label><input type='password' id='pass'><button onclick='history.back()'>Back</button><button onclick='testMqtt()'>Continue</button></div><script>function testMqtt(){var h=document.getElementById('host').value;var p=parseInt(document.getElementById('port').value);var u=document.getElementById('user').value;var pw=document.getElementById('pass').value;if(!h||!p){document.getElementById('err').style.display='block';return}fetch('/api/mqtt/test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({host:h,port:p,user:u,pass:pw})}).then(r=>r.json()).then(d=>{if(d.success){document.getElementById('ok').style.display='block';setTimeout(()=>location.href='/step/confirm',1500)}else{document.getElementById('err').style.display='block'}}).catch(e=>{})}</script></body></html>";

const char STEP3_HTML[] PROGMEM = "<!DOCTYPE html><html><head><title>TerraSmart</title><meta charset='UTF-8'><meta name='viewport' content='width=device-width'><style>body{font-family:Arial;background:#667eea;margin:0;padding:20px;min-height:100vh;display:flex;align-items:center;justify-content:center}.container{background:#fff;padding:40px;border-radius:12px;max-width:500px;width:100%}h1{color:#333}.summary{background:#f9f9f9;padding:20px;border-radius:8px;margin-bottom:20px;border-left:4px solid #667eea}.summary-item{margin-bottom:12px;padding-bottom:12px;border-bottom:1px solid #e0e0e0}.summary-item:last-child{border-bottom:none}.label{font-weight:600;color:#666;font-size:12px}.value{color:#333;font-size:14px}button{width:100%;padding:14px;background:#667eea;color:#fff;border:none;border-radius:6px;cursor:pointer;margin:5px 0}</style></head><body><div class='container'><h1>Confirm</h1><div class='summary'><div class='summary-item'><div class='label'>SSID</div><div class='value' id='s1'>-</div></div><div class='summary-item'><div class='label'>MQTT Host</div><div class='value' id='s2'>-</div></div><div class='summary-item'><div class='label'>MQTT Port</div><div class='value' id='s3'>-</div></div><div class='summary-item'><div class='label'>MQTT User</div><div class='value' id='s4'>-</div></div></div><button onclick='history.back()'>Back</button><button onclick='finalize()'>Finish</button></div><script>function loadSum(){fetch('/api/setup/summary').then(r=>r.json()).then(d=>{document.getElementById('s1').textContent=d.wifi_ssid||'-';document.getElementById('s2').textContent=d.mqtt_host||'-';document.getElementById('s3').textContent=d.mqtt_port||'-';document.getElementById('s4').textContent=d.mqtt_user||'-'}).catch(e=>{})}function finalize(){fetch('/api/setup/complete',{method:'POST'}).then(r=>r.json()).then(d=>{if(d.success){alert('OK');setTimeout(()=>location.reload(),2000)}}).catch(e=>{})}window.addEventListener('load',loadSum);</script></body></html>";

void handleRoot()
{
  server.sendHeader("Location", "/step/wifi");
  server.send(302, "text/html", "");
}

void handleApiScan()
{
  int n = WiFi.scanNetworks();
  String json = "{\"networks\":[";
  for (int i = 0; i < n; i++)
  {
    if (i > 0)
      json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\"}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleApiWiFiTest()
{
  if (server.method() != HTTP_POST)
  {
    server.send(405, "application/json", "{\"success\":false}");
    return;
  }
  String payload = server.arg("plain");
  int ss = payload.indexOf("\"ssid\":\"");
  int se = payload.indexOf("\"", ss + 8);
  String ssid = payload.substring(ss + 8, se);
  int ps = payload.indexOf("\"pass\":\"");
  int pe = payload.indexOf("\"", ps + 8);
  String pass = payload.substring(ps + 8, pe);

  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), pass.c_str());
  int att = 0;
  while (WiFi.status() != WL_CONNECTED && att < 20)
  {
    delay(500);
    att++;
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    g_setupManager.saveWiFiConfig(ssid, pass);
    server.send(200, "application/json", "{\"success\":true}");
  }
  else
  {
    server.send(200, "application/json", "{\"success\":false}");
  }
}

void handleApiMQTTTest()
{
  if (server.method() != HTTP_POST)
  {
    server.send(405, "application/json", "{\"success\":false}");
    return;
  }
  String payload = server.arg("plain");

  // Parse host
  int hs = payload.indexOf("\"host\":\"");
  int he = payload.indexOf("\"", hs + 8);
  String host = payload.substring(hs + 8, he);

  // Parse port
  int ps = payload.indexOf("\"port\":");
  int pe = payload.indexOf(",", ps);
  int port = payload.substring(ps + 7, pe).toInt();

  // Parse user
  int us = payload.indexOf("\"user\":\"");
  int ue = payload.indexOf("\"", us + 8);
  String user = payload.substring(us + 8, ue);

  // Parse password
  int pws = payload.indexOf("\"pass\":\"");
  int pwe = payload.indexOf("\"", pws + 8);
  String pass = payload.substring(pws + 8, pwe);

  if (host.length() > 0 && port > 0)
  {
    g_setupManager.saveMQTTConfig(host, port, user, pass);
    server.send(200, "application/json", "{\"success\":true}");
  }
  else
  {
    server.send(200, "application/json", "{\"success\":false}");
  }
}

void handleApiSetupSummary()
{
  String json = "{\"wifi_ssid\":\"" + g_setupManager.getWiFiSSID() +
                "\",\"mqtt_host\":\"" + g_setupManager.getMQTTHost() +
                "\",\"mqtt_port\":" + String(g_setupManager.getMQTTPort()) +
                ",\"mqtt_user\":\"" + g_setupManager.getMQTTUser() + "\"}";
  server.send(200, "application/json", json);
}

void handleApiSetupComplete()
{
  if (server.method() != HTTP_POST)
  {
    server.send(405, "application/json", "{\"success\":false}");
    return;
  }

  // Salvar configuração completa no LittleFS antes de marcar como completo
  if (g_setupManager.saveCurrentConfig())
  {
    g_setupManager.markSetupComplete();
    server.send(200, "application/json", "{\"success\":true}");
    delay(2000);
    ESP.restart();
  }
  else
  {
    server.send(200, "application/json", "{\"success\":false,\"error\":\"Falha ao salvar configuração\"}");
  }
}

void startCaptivePortal()
{
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);

  IPAddress ip(192, 168, 4, 1);
  IPAddress gw(192, 168, 4, 1);
  IPAddress sn(255, 255, 255, 0);

  WiFi.softAPConfig(ip, gw, sn);
  WiFi.softAP(AP_SSID);
  delay(1000);

  Serial.println("[AP] Started");

  dnsServer.start(53, "*", ip);

  server.on("/", handleRoot);
  server.on("/step/wifi", []()
            { server.send(200, "text/html", STEP1_HTML); });
  server.on("/step/mqtt", []()
            { server.send(200, "text/html", STEP2_HTML); });
  server.on("/step/confirm", []()
            { server.send(200, "text/html", STEP3_HTML); });
  server.on("/api/scan", handleApiScan);
  server.on("/api/wifi/test", handleApiWiFiTest);
  server.on("/api/mqtt/test", handleApiMQTTTest);
  server.on("/api/setup/summary", handleApiSetupSummary);
  server.on("/api/setup/complete", handleApiSetupComplete);
  server.on("/generate_204", []()
            { server.send(200, "text/html", STEP1_HTML); });
  server.on("/fwlink", []()
            { server.send(200, "text/html", STEP1_HTML); });
  server.onNotFound([]()
                    { server.send(200, "text/html", STEP1_HTML); });

  server.begin();
}

void captiveLoop()
{
  dnsServer.processNextRequest();
  server.handleClient();
  delay(2);
}
