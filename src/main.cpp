#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <PubSubClient.h>
#include "main.h"

// MQTT
void sendMQTTMessage(String payload)
{
  Serial.println(payload);

  String mqttTopic = topic;
  String mqttPayload = payload;

  Serial.print("Sending (");
  Serial.print(mqttTopic);
  Serial.print("): ");
  Serial.println(mqttPayload);

  if (!mqttClient.connected())
  {
    if (mqttClient.connect(clientId, mqttUsername, mqttPassword))
    {
      Serial.println("Connected to MQTT broker");
    }
    else
    {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying later");
    }
  }
  if (mqttClient.publish(mqttTopic.c_str(), mqttPayload.c_str()))
  {
    Serial.println("Message published successfully.");
  }
  else
  {
    Serial.print("Error publishing message. Error code: ");
    Serial.println(mqttClient.state());
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  // handle incoming messages here
  Serial.print("Received something on MQTT (");
  Serial.print(topic);
  Serial.print(") -> ");
  String strPayload = String((char *)payload).substring(0, length);
  Serial.println(strPayload);
}

// AC Serial
void send_command(std::vector<uint8_t> command, CommandType type, uint8_t header = CTRL_HEADER)
{
  uint8_t length = command.size();
  command.insert(command.begin(), header);
  command.insert(command.begin() + 1, length);

  uint8_t checksum = 0;

  for (uint8_t i : command)
    checksum -= i; // Add to checksum

  command.push_back(checksum);

  send_packet(command, type); // Actually send the constructed packet
}

void write_array(const std::vector<uint8_t> &packet)
{
  // Az adatok írása a soros portra
  for (uint8_t i = 0; i < packet.size(); i++)
  {
    Serial2.write(packet[i]);
  }
}

void send_packet(const std::vector<uint8_t> &packet, CommandType type)
{
  last_packet_sent_ = millis(); // Save the time when we sent the last packet

  if (type != CommandType::Response) // Don't wait for a response for responses
    waiting_for_response_ = true;    // Mark that we are waiting for a response

  write_array(packet); // Write to UART
}

void handle_poll()
{
  if (millis() - last_packet_sent_ > POLL_INTERVAL)
  {
    Serial.println("Polling AC");
    sendMQTTMessage("Polling AC");
    send_command(CMD_POLL, CommandType::Normal, POLL_HEADER);
  }
}

void onSerialData()
{
  if (Serial2.available())
  {
    String data = Serial2.readString();
    if (data.length() > 0)
    {
      Serial.print("Serial data: ");
      Serial.println(data);
      buffer = data;
      sendMQTTMessage(data);
    }
  }
}

String toHex(String buffer, size_t length)
{
  String hexString;
  const char *hexDigits = "0123456789ABCDEF";

  for (size_t i = 0; i < length; i++)
  {
    hexString += hexDigits[(buffer[i] >> 4) & 0x0F];
    hexString += hexDigits[buffer[i] & 0x0F];
  }

  return hexString;
}

// WEB
void onRequest()
{
  String html = "<html>\n<body>\n";
  html += buffer;
  html += "<br/>\n";
  html += toHex(buffer, sizeof(buffer));
  html += "<br/>\n";
  html += "</body>\n</html>\n";
  server.send(200, "text/html", html);
}

void onNotFound()
{
  server.send(404, "text/html", "<p1>Nem nyert</p1>");
}

void handleRoot()
{ // A függvény, ami a főoldalt kiszolgálja
  // Serial.print("Wifi mode: ");
  // Serial.println(WiFi.getMode());
  if (WiFi.getMode() == WIFI_STA)
  {
    server.send(200, "text/html", "<p1>Kapcsolat rendben</p1>");
  }
  else
  {
    server.send(200, "text/html", "<form method='POST' action='/connect'><label>SSID: </label><input type='text' name='ssid'><br><label>Password: </label><input type='password' name='password'><br><input type='submit' value='Connect'></form>"); // A form létrehozása az SSID és jelszó megadására
  }
}

void handleConnect()
{
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  saveWiFiCredentials(ssid.c_str(), password.c_str());
  connectToRouter(ssid, password);
  // server.send(200, "text/html", "Connected to WiFi!");  // A kapcsolódás visszajelzése
}

void handleGet()
{
  // Start status reading
  server.send(200, "text/html", "readed info...");
}

// WIFI
void saveWiFiCredentials(const char *ssid, const char *password)
{
  // Wi-Fi hálózati adatok mentése az SPIFFS fájlrendszerbe
  File file = SPIFFS.open("/wifi.txt", FILE_WRITE);
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  file.println(ssid);
  file.println(password);
  file.close();
}

boolean readWiFiCredentials(char *ssid, char *password)
{
  // Wi-Fi hálózati adatok olvasása az SPIFFS fájlrendszerből
  if (!SPIFFS.exists("/wifi.txt"))
  {
    Serial.println("No wifi file found");
    return false;
  }
  File file = SPIFFS.open("/wifi.txt", FILE_READ);
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return false;
  }
  file.readStringUntil('\n').toCharArray(ssid, 32);
  ssid[strlen(ssid) - 1] = '\0';
  file.readStringUntil('\n').toCharArray(password, 64);
  password[strlen(password) - 1] = '\0';
  file.close();
  Serial.println("Wifi read from file");
  return true;
}

void connectToRouter(String ssid, String password)
{
  Serial.print(ssid);
  Serial.print(password);
  Serial.println("--");
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str()); // A WiFi hálózathoz való kapcsolódás
  int countDown = 10;
  Serial.print("Connecting to router ");
  while (WiFi.status() != WL_CONNECTED && countDown-- > 0)
  { // Várakozás a kapcsolódásra
    Serial.print(".");
    // Serial.println(WiFi.status());
    delay(1000);
  }
  if (countDown < 1)
  {
    Serial.println("Can not connect. Back to AP mode.");
    createAP();
    return;
  }

  Serial.println("Connected to WiFi");
  Serial.print("Kapott IP-cím: ");
  Serial.println(WiFi.localIP());
}

void createAP()
{
  WiFi.mode(WIFI_AP);                     // AP módban indítás
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PWD); // Az AP beállítása
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP-cím: ");
  Serial.println(myIP);
}

// Timer
void timer()
{
  unsigned long currentMillis = millis(); // aktuális idő lekérdezése

  if (currentMillis - previousMillis >= interval)
  {                                 // ha eltelt a megadott idő
    previousMillis = currentMillis; // frissítjük az előző időt
    flashLed();
    handle_poll();
  }
}

void flashLed()
{
  digitalWrite(ledPin, !digitalRead(ledPin)); // LED állapotának megfordítása
}

// SETUP
void setup()
{
  Serial.begin(Serial_Baud);
  Serial2.begin(ACSerial_Baud, SERIAL_8N1, ACSerial_RX, ACSerial_TX, false, READ_TIMEOUT);

  pinMode(ledPin, OUTPUT); // LED lábának beállítása kimenetre
  pinMode(ACSerial_TX, OUTPUT);

  // SPIFFS inicializálása
  if (!SPIFFS.begin(true))
  {
    Serial.println("Failed to initialize SPIFFS");
    return;
  }

  Serial.println("SPIFFS initialized");

  if (readWiFiCredentials(ssid, password))
  {
    Serial.println("Connect to router");
    connectToRouter(ssid, password);
  }
  else
  {
    Serial.println("Start AP mode");
    createAP();
  }
  server.on("/", handleRoot);           // A főoldalhoz tartozó kezelő függvény
  server.on("/connect", handleConnect); // Az adatok beküldésekor meghívott függvény
  server.on("/get", handleGet);
  server.on("/dump", onRequest);
  server.serveStatic("/", SPIFFS, "/");
  server.onNotFound(onNotFound);
  server.begin(); // A WebServer elindítása

  Serial.println("Starting MQTT");
  mqttClient.setServer(mqttBroker, mqttPort);
  // mqttClient.setCallback(mqttCallback);
  while (!mqttClient.connected())
  {
    if (mqttClient.connect(clientId, mqttUsername, mqttPassword))
    {
      mqttClient.subscribe(topic);
    }
    else
    {
      Serial.println("Waitinf for MQTT");
      delay(5000);
    }
  }
  Serial.println("MQTT connected.");
}

// LOOP
void loop()
{
  server.handleClient(); // A klienst kezelő függvény hívása
  // Serial.println("Loop...");
  onSerialData();
  mqttClient.loop();
  timer();
}
