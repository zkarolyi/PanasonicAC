#include "airconditioner.h"

#define VERSION "C" // A: livingRoom, B: Mid floor, C: Top floor

// MQTT
#define BASE_MQTT_TOPIC "PanasonicAC_" VERSION

const char *mqttBroker = "192.168.1.120";
const int mqttPort = 1883;
const char *clientId = "PanasonicAC_" VERSION;
const char *mqttUsername = "openhabian";
const char *mqttPassword = "openhabian";
String acTopicSend = BASE_MQTT_TOPIC "/out/"; 
String acTopicLog = BASE_MQTT_TOPIC "/log/"; 
String acTopicSet = BASE_MQTT_TOPIC "/set/#";
String acTopicSetTemperature = BASE_MQTT_TOPIC "/set/temperature/";
String acTopicSetMode = BASE_MQTT_TOPIC "/set/mode/";
String acTopicSetFanSpeed = BASE_MQTT_TOPIC "/set/fanspeed/";
String acTopicSetSwing = BASE_MQTT_TOPIC "/set/swing/";
String acTopicSetPreset = BASE_MQTT_TOPIC "/set/preset/";
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void sendMQTTMessage(String payload);
void sendMQTTLog(String payload);
void mqttCallback(char *topic, byte *payload, unsigned int length);

// AC Serial
static const uint8_t READ_TIMEOUT = 20; // The maximum time to wait before considering a packet complete
static const int POLL_INTERVAL = 10000; // The interval at which to poll the AC
static const std::vector<uint8_t> CMD_POLL = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t CTRL_HEADER = 0xF0; // The header for control frames
static const uint8_t POLL_HEADER = 0x70; // The header for the poll command
uint32_t last_packet_sent;               // Stores the time at which the last packet was sent
bool waiting_for_response = false;       // Set to true if we are waiting for a response
enum class CommandType
{
    Normal,
    Response,
    Resend
};
// Communication parameters: TTL 5V UART 9600,8,E,1
const unsigned long ACSerial_Baud = 9600;
const int ACSerial_RX = 16;
const int ACSerial_TX = 17;

void send_command(std::vector<uint8_t> command, CommandType type, uint8_t header);
void send_command(CommandType type, uint8_t header);
void write_array(const std::vector<uint8_t> &packet);
void send_packet(const std::vector<uint8_t> &packet, CommandType type);
void handle_poll();
void onSerialData();
String toHex(String, size_t length);
String toHex(std::vector<uint8_t> buffer);
AirConditioner ac;

// WEB
void onRequest();
void onNotFound();
void handleRoot();
void handleConnect();
void handleGet();

// WIFI
char ssid[32];
char password[64];
WebServer server(80); // A WebServer objektum inicializálása, 80-as porton
const char *WIFI_AP_SSID = "ESP32-AP";
const char *WIFI_AP_PWD = "almaalma";
unsigned long apActiveMillis = 0; // mennyi ideje van AP módban a WIFI. 0: nem AP módban van
const long apInterval = 600000;   // AP mód timeout
const char *HOSTNAME = "PanasonicAC_" VERSION;

void saveWiFiCredentials(const char *ssid, const char *password);
boolean readWiFiCredentials(char *ssid, char *password);
void connectToRouter(String ssid, String password);
void createAP();

// Timer
const int ledPin = 2;             // beépített LED lába
unsigned long previousMillis = 0; // változó az időzítéshez
const long interval = 3000;      // az időköz, amelyenként villog a LED
const unsigned long Serial_Baud = 115200;

void timer();
void flashLed();

// WDT
const int wdtTimeOut = 60;
