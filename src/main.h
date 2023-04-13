// MQTT
const char *mqttBroker = "192.168.1.120";
const int mqttPort = 1883;
const char *clientId = "PanasonicAC"; // Add num to it
const char *mqttUsername = "openhabian";
const char *mqttPassword = "openhabian";
const char *topic = "PanasonicAC/"; // Add num to it
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void sendMQTTMessage(String payload);
void mqttCallback(char *topic, byte *payload, unsigned int length);

// AC Serial
String buffer;
static const uint8_t READ_TIMEOUT = 20; // The maximum time to wait before considering a packet complete
static const int POLL_INTERVAL = 5000;  // The interval at which to poll the AC
static const std::vector<uint8_t> CMD_POLL = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t CTRL_HEADER = 0xF0; // The header for control frames
static const uint8_t POLL_HEADER = 0x70; // The header for the poll command
uint32_t last_packet_sent_;              // Stores the time at which the last packet was sent
bool waiting_for_response_ = false;      // Set to true if we are waiting for a response
enum class CommandType
{
    Normal,
    Response,
    Resend
};
const unsigned long ACSerial_Baud = 115200;
const int ACSerial_RX = 16;
const int ACSerial_TX = 17;

void send_command(std::vector<uint8_t> command, CommandType type, uint8_t header);
void write_array(const std::vector<uint8_t> &packet);
void send_packet(const std::vector<uint8_t> &packet, CommandType type);
void handle_poll();
void onSerialData();
String toHex(const char *buffer, size_t length);

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

void saveWiFiCredentials(const char *ssid, const char *password);
boolean readWiFiCredentials(char *ssid, char *password);
void connectToRouter(String ssid, String password);
void createAP();

// Timer
const int ledPin = 2;             // beépített LED lába
unsigned long previousMillis = 0; // változó az időzítéshez
const long interval = 1000;       // az időköz, amelyenként villog a LED
const unsigned long Serial_Baud = 115200;

void timer();
void flashLed();
