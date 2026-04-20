#include <SPI.h>
#include <Ethernet.h>
#include <Preferences.h>
#include <esp_system.h>
#include <base64.h>

// -------------------- Sabitler --------------------
#define ETH_CS_PIN          5
#define RESET_BUTTON_PIN    4
#define RESET_HOLD_MS       5000

const char* FW_VERSION = "v2.0";

// Fabrika ayarlari
IPAddress FACTORY_IP(192, 168, 10, 50);
IPAddress FACTORY_SUBNET(255, 255, 255, 0);
IPAddress FACTORY_GATEWAY(192, 168, 10, 1);
IPAddress FACTORY_DNS(8, 8, 8, 8);

const char* FACTORY_DEVICE_NAME = "ESP32-W5500";
const char* FACTORY_USERNAME    = "admin";
const char* FACTORY_PASSWORD    = "1234";

// -------------------- Global --------------------
Preferences prefs;
EthernetServer server(80);

struct DeviceConfig {
  bool dhcp;
  IPAddress ip;
  IPAddress subnet;
  IPAddress gateway;
  IPAddress dns;
  String deviceName;
  String username;
  String password;
};

DeviceConfig cfg;
byte mac[6];

unsigned long startMillis = 0;
unsigned long buttonPressStart = 0;
bool buttonWasPressed = false;

// -------------------- Yardimci Fonksiyonlar --------------------
String ipToString(IPAddress ip) {
  return String(ip[0]) + "." + String(ip[1]) + "." + String(ip[2]) + "." + String(ip[3]);
}

bool parseIP(String str, IPAddress &ip) {
  int parts[4];
  int idx = 0;
  int last = 0;

  str.trim();

  for (int i = 0; i <= str.length(); i++) {
    if (i == str.length() || str[i] == '.') {
      if (idx > 3) return false;

      String p = str.substring(last, i);
      p.trim();
      if (p.length() == 0) return false;

      int val = p.toInt();
      if (!(p == "0" || val != 0)) return false;
      if (val < 0 || val > 255) return false;

      parts[idx++] = val;
      last = i + 1;
    }
  }

  if (idx != 4) return false;
  ip = IPAddress(parts[0], parts[1], parts[2], parts[3]);
  return true;
}

String urlDecode(String str) {
  String decoded = "";
  char temp[] = "0x00";

  for (unsigned int i = 0; i < str.length(); i++) {
    if (str[i] == '+') {
      decoded += ' ';
    } else if (str[i] == '%' && i + 2 < str.length()) {
      temp[2] = str[i + 1];
      temp[3] = str[i + 2];
      decoded += (char) strtol(temp, NULL, 16);
      i += 2;
    } else {
      decoded += str[i];
    }
  }
  return decoded;
}

String htmlEscape(String s) {
  s.replace("&", "&amp;");
  s.replace("<", "&lt;");
  s.replace(">", "&gt;");
  s.replace("\"", "&quot;");
  s.replace("'", "&#39;");
  return s;
}

String getParamValue(String requestLine, String paramName) {
  String search = paramName + "=";
  int start = requestLine.indexOf(search);
  if (start == -1) return "";

  start += search.length();
  int end = requestLine.indexOf('&', start);
  if (end == -1) end = requestLine.indexOf(' ', start);
  if (end == -1) return "";

  return urlDecode(requestLine.substring(start, end));
}

String getHeaderValue(String request, String headerName) {
  String key = headerName + ": ";
  int start = request.indexOf(key);
  if (start == -1) return "";

  start += key.length();
  int end = request.indexOf("\r\n", start);
  if (end == -1) return "";

  return request.substring(start, end);
}

bool isNetworkConfigValid(bool dhcpMode, IPAddress ip, IPAddress subnet, IPAddress gateway, IPAddress dns) {
  if (dhcpMode) return true;

  if (ip == IPAddress(0,0,0,0)) return false;
  if (subnet == IPAddress(0,0,0,0)) return false;
  if (gateway == IPAddress(0,0,0,0)) return false;
  if (dns == IPAddress(0,0,0,0)) return false;

  return true;
}

String getUptimeString() {
  unsigned long sec = (millis() - startMillis) / 1000;
  unsigned long days = sec / 86400;
  sec %= 86400;
  unsigned long hours = sec / 3600;
  sec %= 3600;
  unsigned long mins = sec / 60;
  sec %= 60;

  return String(days) + " gun " + String(hours) + " saat " + String(mins) + " dk " + String(sec) + " sn";
}

void setFactoryDefaults() {
  cfg.dhcp = false;
  cfg.ip = FACTORY_IP;
  cfg.subnet = FACTORY_SUBNET;
  cfg.gateway = FACTORY_GATEWAY;
  cfg.dns = FACTORY_DNS;
  cfg.deviceName = FACTORY_DEVICE_NAME;
  cfg.username = FACTORY_USERNAME;
  cfg.password = FACTORY_PASSWORD;
}

void saveConfig() {
  prefs.begin("netcfg", false);
  prefs.putBool("valid", true);
  prefs.putBool("dhcp", cfg.dhcp);
  prefs.putString("ip", ipToString(cfg.ip));
  prefs.putString("subnet", ipToString(cfg.subnet));
  prefs.putString("gateway", ipToString(cfg.gateway));
  prefs.putString("dns", ipToString(cfg.dns));
  prefs.putString("devname", cfg.deviceName);
  prefs.putString("user", cfg.username);
  prefs.putString("pass", cfg.password);
  prefs.end();
}

void clearConfig() {
  prefs.begin("netcfg", false);
  prefs.clear();
  prefs.end();
}

void loadConfig() {
  prefs.begin("netcfg", true);
  bool valid = prefs.getBool("valid", false);

  if (!valid) {
    prefs.end();
    setFactoryDefaults();
    return;
  }

  cfg.dhcp       = prefs.getBool("dhcp", false);
  cfg.deviceName = prefs.getString("devname", FACTORY_DEVICE_NAME);
  cfg.username   = prefs.getString("user", FACTORY_USERNAME);
  cfg.password   = prefs.getString("pass", FACTORY_PASSWORD);

  String ipStr      = prefs.getString("ip", ipToString(FACTORY_IP));
  String subnetStr  = prefs.getString("subnet", ipToString(FACTORY_SUBNET));
  String gatewayStr = prefs.getString("gateway", ipToString(FACTORY_GATEWAY));
  String dnsStr     = prefs.getString("dns", ipToString(FACTORY_DNS));
  prefs.end();

  if (!parseIP(ipStr, cfg.ip)) cfg.ip = FACTORY_IP;
  if (!parseIP(subnetStr, cfg.subnet)) cfg.subnet = FACTORY_SUBNET;
  if (!parseIP(gatewayStr, cfg.gateway)) cfg.gateway = FACTORY_GATEWAY;
  if (!parseIP(dnsStr, cfg.dns)) cfg.dns = FACTORY_DNS;

  if (cfg.deviceName.length() == 0) cfg.deviceName = FACTORY_DEVICE_NAME;
  if (cfg.username.length() == 0) cfg.username = FACTORY_USERNAME;
  if (cfg.password.length() == 0) cfg.password = FACTORY_PASSWORD;
}

void generateUniqueMAC() {
  uint64_t chipid = ESP.getEfuseMac();

  mac[0] = 0x02; // locally administered
  mac[1] = (chipid >> 40) & 0xFF;
  mac[2] = (chipid >> 32) & 0xFF;
  mac[3] = (chipid >> 24) & 0xFF;
  mac[4] = (chipid >> 16) & 0xFF;
  mac[5] = (chipid >> 8) & 0xFF;
}

String macToString() {
  char buf[18];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

void printBootInfo() {
  Serial.println();
  Serial.println("===== CIHAZ BILGISI =====");
  Serial.print("FW       : "); Serial.println(FW_VERSION);
  Serial.print("Cihaz    : "); Serial.println(cfg.deviceName);
  Serial.print("MAC      : "); Serial.println(macToString());
  Serial.print("Mod      : "); Serial.println(cfg.dhcp ? "DHCP" : "STATIK");
  Serial.print("IP       : "); Serial.println(Ethernet.localIP());
  Serial.print("Subnet   : "); Serial.println(cfg.subnet);
  Serial.print("Gateway  : "); Serial.println(cfg.gateway);
  Serial.print("DNS      : "); Serial.println(cfg.dns);
  Serial.print("Kullanici: "); Serial.println(cfg.username);
  Serial.println("=========================");
}

// -------------------- Ethernet --------------------
void startEthernet() {
  Ethernet.init(ETH_CS_PIN);

  if (cfg.dhcp) {
    Serial.println("DHCP ile IP alinmaya calisiliyor...");
    if (Ethernet.begin(mac) == 0) {
      Serial.println("DHCP basarisiz, statik IP fallback uygulanacak...");
      Ethernet.begin(mac, cfg.ip, cfg.dns, cfg.gateway, cfg.subnet);
    }
  } else {
    Serial.println("Statik IP ile baslatiliyor...");
    Ethernet.begin(mac, cfg.ip, cfg.dns, cfg.gateway, cfg.subnet);
  }

  delay(1000);
}

// -------------------- Auth --------------------
bool isAuthorized(String request) {
  String auth = getHeaderValue(request, "Authorization");
  if (auth.length() == 0) return false;

  String raw = cfg.username + ":" + cfg.password;
  String encoded = base64::encode(raw);
  String expected = "Basic " + encoded;

  return auth == expected;
}

void sendUnauthorized(EthernetClient &client) {
  client.println("HTTP/1.1 401 Unauthorized");
  client.println("WWW-Authenticate: Basic realm=\"ESP32-W5500\"");
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();
  client.println("<html><body><h3>401 - Yetkisiz erisim</h3></body></html>");
}

// -------------------- HTTP Header --------------------
void sendHtmlHeader(EthernetClient &client, const char* status = "200 OK") {
  client.println("HTTP/1.1 " + String(status));
  client.println("Content-Type: text/html; charset=UTF-8");
  client.println("Connection: close");
  client.println();
}

void sendJsonHeader(EthernetClient &client, const char* status = "200 OK") {
  client.println("HTTP/1.1 " + String(status));
  client.println("Content-Type: application/json; charset=UTF-8");
  client.println("Connection: close");
  client.println();
}

// -------------------- HTML --------------------
void sendMainPage(EthernetClient &client, String message = "", bool isError = false) {
  sendHtmlHeader(client);

  String devEsc = htmlEscape(cfg.deviceName);
  String userEsc = htmlEscape(cfg.username);
  String passEsc = htmlEscape(cfg.password);

  client.println("<!DOCTYPE html><html><head><meta charset='UTF-8'>");
  client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  client.println("<title>ESP32 W5500 Yonetim</title>");
  client.println("<style>");
  client.println("body{font-family:Arial,sans-serif;background:#f2f2f2;margin:20px;}");
  client.println(".box{max-width:760px;background:#fff;padding:20px;border-radius:12px;box-shadow:0 2px 12px rgba(0,0,0,.12);}");
  client.println("input[type=text],input[type=password]{width:100%;padding:10px;margin:8px 0 15px 0;box-sizing:border-box;}");
  client.println("button{padding:10px 20px;background:#007bff;color:#fff;border:none;border-radius:6px;cursor:pointer;}");
  client.println(".msg{padding:12px;border-radius:8px;margin-bottom:15px;}");
  client.println(".ok{background:#d4edda;color:#155724;}");
  client.println(".err{background:#f8d7da;color:#721c24;}");
  client.println("code{background:#eee;padding:2px 5px;border-radius:4px;}");
  client.println(".row{margin-bottom:10px;}");
  client.println("a{display:inline-block;margin-top:10px;margin-right:15px;}");
  client.println("</style></head><body><div class='box'>");

  client.println("<h2>ESP32 + W5500 Yonetim Paneli</h2>");

  client.println("<p><b>Firmware:</b> " + String(FW_VERSION) + "</p>");
  client.println("<p><b>Cihaz Adi:</b> " + devEsc + "</p>");
  client.println("<p><b>Aktif IP:</b> " + ipToString(Ethernet.localIP()) + "</p>");
  client.println("<p><b>MAC:</b> " + macToString() + "</p>");
  client.println("<p><b>Mod:</b> " + String(cfg.dhcp ? "DHCP" : "Statik") + "</p>");
  client.println("<p><b>Uptime:</b> " + getUptimeString() + "</p>");
  client.println("<p><b>API:</b> <code>/api/status</code> &nbsp; <code>/api/reboot</code> &nbsp; <code>/api/reset</code></p>");

  if (message.length() > 0) {
    client.println("<div class='msg " + String(isError ? "err" : "ok") + "'>" + htmlEscape(message) + "</div>");
  }

  client.println("<form action='/save' method='get'>");

  client.println("<label>Cihaz Adi</label>");
  client.println("<input type='text' name='devname' value='" + devEsc + "' maxlength='50' required>");

  client.println("<label>Kullanici Adi</label>");
  client.println("<input type='text' name='user' value='" + userEsc + "' maxlength='32' required>");

  client.println("<label>Sifre</label>");
  client.println("<input type='password' name='pass' value='" + passEsc + "' maxlength='32' required>");

  client.println("<div class='row'>");
  client.println("<label><input type='checkbox' name='dhcp' value='1' " + String(cfg.dhcp ? "checked" : "") + "> DHCP kullan</label>");
  client.println("</div>");

  client.println("<label>IP Adresi</label>");
  client.println("<input type='text' name='ip' value='" + ipToString(cfg.ip) + "' required>");

  client.println("<label>Subnet Mask</label>");
  client.println("<input type='text' name='subnet' value='" + ipToString(cfg.subnet) + "' required>");

  client.println("<label>Gateway</label>");
  client.println("<input type='text' name='gateway' value='" + ipToString(cfg.gateway) + "' required>");

  client.println("<label>DNS</label>");
  client.println("<input type='text' name='dns' value='" + ipToString(cfg.dns) + "' required>");

  client.println("<button type='submit'>Kaydet ve Yeniden Baslat</button>");
  client.println("</form>");

  client.println("<p>");
  client.println("<a href='/api/status'>API Status</a>");
  client.println("<a href='/api/reboot' onclick=\"return confirm('Cihaz yeniden baslatilsin mi?');\">API Reboot</a>");
  client.println("<a href='/api/reset' onclick=\"return confirm('Fabrika ayarina donulsun mu?');\">API Reset</a>");
  client.println("</p>");

  client.println("</div></body></html>");
}

void sendNotFound(EthernetClient &client) {
  sendHtmlHeader(client, "404 Not Found");
  client.println("<html><body><h3>404 - Sayfa bulunamadi</h3></body></html>");
}

// -------------------- API --------------------
void sendApiStatus(EthernetClient &client) {
  sendJsonHeader(client);

  client.println("{");
  client.println("  \"firmware\": \"" + String(FW_VERSION) + "\",");
  client.println("  \"device_name\": \"" + cfg.deviceName + "\",");
  client.println("  \"mode\": \"" + String(cfg.dhcp ? "DHCP" : "STATIC") + "\",");
  client.println("  \"ip\": \"" + ipToString(Ethernet.localIP()) + "\",");
  client.println("  \"subnet\": \"" + ipToString(cfg.subnet) + "\",");
  client.println("  \"gateway\": \"" + ipToString(cfg.gateway) + "\",");
  client.println("  \"dns\": \"" + ipToString(cfg.dns) + "\",");
  client.println("  \"mac\": \"" + macToString() + "\",");
  client.println("  \"uptime\": \"" + getUptimeString() + "\",");
  client.println("  \"uptime_ms\": " + String(millis() - startMillis));
  client.println("}");
}

void sendApiAction(EthernetClient &client, String action, String result) {
  sendJsonHeader(client);
  client.println("{");
  client.println("  \"action\": \"" + action + "\",");
  client.println("  \"result\": \"" + result + "\"");
  client.println("}");
}

// -------------------- Request --------------------
void handleRequest(EthernetClient &client, String requestLine, String fullRequest) {
  Serial.println("Istek: " + requestLine);

  if (!isAuthorized(fullRequest)) {
    sendUnauthorized(client);
    return;
  }

  if (requestLine.startsWith("GET / ")) {
    sendMainPage(client);
    return;
  }

  if (requestLine.startsWith("GET /api/status ")) {
    sendApiStatus(client);
    return;
  }

  if (requestLine.startsWith("GET /api/reboot ")) {
    sendApiAction(client, "reboot", "device will reboot with saved settings");
    delay(1000);
    ESP.restart();
    return;
  }

  if (requestLine.startsWith("GET /api/reset ")) {
    clearConfig();
    sendApiAction(client, "reset", "factory defaults restored, device will reboot");
    delay(1000);
    ESP.restart();
    return;
  }

  if (requestLine.startsWith("GET /save?")) {
    String devNameStr = getParamValue(requestLine, "devname");
    String userStr    = getParamValue(requestLine, "user");
    String passStr    = getParamValue(requestLine, "pass");
    String dhcpStr    = getParamValue(requestLine, "dhcp");
    String ipStr      = getParamValue(requestLine, "ip");
    String subnetStr  = getParamValue(requestLine, "subnet");
    String gatewayStr = getParamValue(requestLine, "gateway");
    String dnsStr     = getParamValue(requestLine, "dns");

    bool newDhcp = (dhcpStr == "1");
    IPAddress newIP, newSubnet, newGateway, newDNS;

    if (devNameStr.length() == 0) {
      sendMainPage(client, "Cihaz adi bos olamaz.", true);
      return;
    }

    if (userStr.length() == 0) {
      sendMainPage(client, "Kullanici adi bos olamaz.", true);
      return;
    }

    if (passStr.length() == 0) {
      sendMainPage(client, "Sifre bos olamaz.", true);
      return;
    }

    if (!parseIP(ipStr, newIP) ||
        !parseIP(subnetStr, newSubnet) ||
        !parseIP(gatewayStr, newGateway) ||
        !parseIP(dnsStr, newDNS)) {
      sendMainPage(client, "IP bilgileri gecersiz.", true);
      return;
    }

    if (!isNetworkConfigValid(newDhcp, newIP, newSubnet, newGateway, newDNS)) {
      sendMainPage(client, "Ag ayarlari gecersiz.", true);
      return;
    }

    cfg.deviceName = devNameStr;
    cfg.username = userStr;
    cfg.password = passStr;
    cfg.dhcp = newDhcp;
    cfg.ip = newIP;
    cfg.subnet = newSubnet;
    cfg.gateway = newGateway;
    cfg.dns = newDNS;

    saveConfig();

    sendHtmlHeader(client);
    client.println("<html><head><meta charset='UTF-8'></head><body>");
    client.println("<h3>Ayarlar kaydedildi.</h3>");
    client.println("<p>Cihaz yeniden baslatiliyor...</p>");
    client.println("<p>Yeni mod: <b>" + String(cfg.dhcp ? "DHCP" : "Statik") + "</b></p>");
    if (!cfg.dhcp) {
      client.println("<p>Yeni IP: <b>" + ipToString(cfg.ip) + "</b></p>");
    } else {
      client.println("<p>Yeni IP DHCP'den alinacaktir.</p>");
    }
    client.println("</body></html>");

    delay(1500);
    ESP.restart();
    return;
  }

  sendNotFound(client);
}

void checkHttpClient() {
  EthernetClient client = server.available();
  if (!client) return;

  String fullRequest = "";
  String requestLine = "";
  bool firstLineRead = false;
  unsigned long timeout = millis();

  while (client.connected() && millis() - timeout < 3000) {
    while (client.available()) {
      char c = client.read();
      fullRequest += c;
      timeout = millis();

      if (!firstLineRead && c == '\n') {
        requestLine = fullRequest;
        requestLine.replace("\r", "");
        requestLine.replace("\n", "");
        firstLineRead = true;
      }

      if (fullRequest.endsWith("\r\n\r\n")) {
        handleRequest(client, requestLine, fullRequest);
        delay(5);
        client.stop();
        return;
      }
    }
  }

  client.stop();
}

// -------------------- Reset Butonu --------------------
void checkResetButton() {
  bool pressed = (digitalRead(RESET_BUTTON_PIN) == LOW);

  if (pressed && !buttonWasPressed) {
    buttonWasPressed = true;
    buttonPressStart = millis();
  }

  if (!pressed && buttonWasPressed) {
    buttonWasPressed = false;
    buttonPressStart = 0;
  }

  if (pressed && buttonWasPressed) {
    if (millis() - buttonPressStart >= RESET_HOLD_MS) {
      Serial.println("Reset butonu ile fabrika ayari yuklenecek...");
      clearConfig();
      delay(1000);
      ESP.restart();
    }
  }
}

// -------------------- Setup / Loop --------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);

  startMillis = millis();

  Serial.println();
  Serial.println("Sistem basliyor...");

  generateUniqueMAC();
  loadConfig();
  startEthernet();
  server.begin();

  printBootInfo();

  Serial.print("Panel URL : http://");
  Serial.println(Ethernet.localIP());
  Serial.println("API       : /api/status , /api/reboot , /api/reset");
}

void loop() {
  checkHttpClient();
  checkResetButton();
}