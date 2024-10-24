#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BH1750.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <TridentTD_LineNotify.h>
#include "FirebaseESP8266.h"
#include <time.h> // For NTP time

// Pin definitions
#define DHTPIN D7     // DHT22 data pin connected to D4 (GPIO2)
#define DHTTYPE DHT22 // DHT 22 (AM2302)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1  // No reset pin

const char* ssid = "B415";
const char* password = "appletv415";
const char* mqtt_server = "broker.emqx.io";

char FIREBASE_AUTH[] = "1JY2E8qYVu1BwEZgm6eUMoOH7jCOXLYdgVg2ndld"; // Your Firebase Web API Key
char FIREBASE_HOST[] = "https://testiot1-3daef-default-rtdb.asia-southeast1.firebasedatabase.app"; // Your Firebase Host URL

FirebaseData firebaseData;

#define LINE_TOKEN  "felfJIBeDFM2l7sDGO1kIwSADuAqJfyeYWt1LaJdcYg"

// Sensor objects
DHT dht(DHTPIN, DHTTYPE);
BH1750 lightMeter;
// NewPing sonar(TRIGPIN, ECHOPIN, 400);  // Max distance of 400 cm
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

const int green_led = D0;
const int yellow_led = D4;
const int red_led = D6;

int normal = 0;

String getFormattedTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M:%S", timeinfo);

  return String(buffer);
}

unsigned long convertToSeconds(const char* datetimeStr) {
    int year, month, day, hour, minute, second;

    // Parse the datetime string
    sscanf(datetimeStr, "%d/%d/%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second);

    // Calculate days since epoch (January 1, 1970)
    unsigned long totalDays = (year - 1970) * 365 + (year - 1970) / 4; // leap years
    static const int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int m = 1; m < month; m++) {
        totalDays += daysInMonth[m];
    }
    // Adjust for leap year
    if (month > 2 && (year % 4 == 0) && (year % 100 != 0 || year % 400 == 0)) {
        totalDays++;
    }
    totalDays += day - 1; // Subtract 1 for the current day

    // Calculate total seconds
    unsigned long totalSeconds = totalDays * 86400 + hour * 3600 + minute * 60 + second;
    return totalSeconds;
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String messageTemp;
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    messageTemp += (char)payload[i];
  }
  Serial.println();

  // Control the LED based on the message
  if (String(topic) == "panuwat/green_led") {
    Serial.print("Changing green to ");
    if (messageTemp == "off") {
      digitalWrite(green_led, LOW); // Turn the LED on
      Serial.println("Off");
    } else if (messageTemp == "on") {
      digitalWrite(green_led, HIGH); // Turn the LED off
      Serial.println("On");
    }
  }
  if (String(topic) == "panuwat/yellow_led") {
    Serial.print("Changing yellow to ");
    if (messageTemp == "off") {
      digitalWrite(yellow_led, LOW); // Turn the LED on
      Serial.println("Off");
    } else if (messageTemp == "on") {
      digitalWrite(yellow_led, HIGH); // Turn the LED off
      Serial.println("On");
    }
  }
  if (String(topic) == "panuwat/red_led") {
    Serial.print("Changing red to ");
    if (messageTemp == "off") {
      digitalWrite(red_led, LOW); // Turn the LED on
      Serial.println("Off");
    } else if (messageTemp == "on") {
      digitalWrite(red_led, HIGH); // Turn the LED off
      Serial.println("On");
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("panuwat/green_led");
      client.subscribe("panuwat/yellow_led");
      client.subscribe("panuwat/red_led");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(green_led, OUTPUT); // Initialize the LED pin as an output
  digitalWrite(green_led, LOW); // Turn the LED off initially
  pinMode(yellow_led, OUTPUT); // Initialize the LED pin as an output
  digitalWrite(yellow_led, LOW); // Turn the LED off initially
  pinMode(red_led, OUTPUT); // Initialize the LED pin as an output
  digitalWrite(red_led, LOW); // Turn the LED off initially

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // Initialize DHT22
  dht.begin();

  LINE.setToken(LINE_TOKEN);

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // Time offset for Thailand
  Serial.println("\nWaiting for NTP time sync...");
  while (!time(nullptr)) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nTime synced!");
  
  // Initialize BH1750
  Wire.begin(D2, D1);  // SDA = D2 (GPIO4), SCL = D1 (GPIO5)
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 initialized successfully"));
  } else {
    Serial.println(F("Error initializing BH1750"));
  }

  // Initialize OLED display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3C for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);  // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.display();           // Show initial display buffer (cleared)
}

void loop() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println(F("Failed to read from DHT22"));
  }

  // Read light level from BH1750
  float lightLevel = lightMeter.readLightLevel();
  if (lightLevel < 0) {
    Serial.println(F("Error reading from BH1750"));
  }

  // // Read distance from HC-SR04
  // unsigned int distance = sonar.ping_cm();
  
  // Print sensor data to Serial Monitor
  Serial.print(F("Temperature: "));
  Serial.print(temperature);
  Serial.println(F(" °C"));

  Serial.print(F("Humidity: "));
  Serial.print(humidity);
  Serial.println(F(" %"));

  Serial.print(F("Light: "));
  Serial.print(lightLevel);
  Serial.println(F(" lx"));

  Serial.println(F("-----------------------"));

  // Update OLED display with sensor data
  display.clearDisplay();  // Clear buffer

  display.setCursor(0, 0);
  display.print(F("Temp: "));
  display.print(temperature);
  display.println(F(" C"));

  display.setCursor(0, 16);
  display.print(F("Humidity: "));
  display.print(humidity);
  display.println(F(" %"));

  display.setCursor(0, 32);
  display.print(F("Light: "));
  display.print(lightLevel);
  display.println(F(" lx"));

  display.display();  // Show the updated content on OLED

  digitalWrite(green_led, LOW);
  digitalWrite(yellow_led,LOW);
  digitalWrite(red_led, LOW);
  normal = 0;
  if (temperature < 30 || temperature > 32){
    digitalWrite(red_led, HIGH);
    normal = 1;
    String line_string = "อุณหภูมิสูง = " + String(temperature) + " C";
    if (LINE.notify(line_string)) {
      Serial.println("Notification sent successfully");
    }
  }
  if (humidity < 80 || humidity > 85){
    digitalWrite(yellow_led, HIGH);
    normal = 1;
  }
  if (normal == 0){
    digitalWrite(green_led, HIGH);
    digitalWrite(yellow_led,LOW);
    digitalWrite(red_led, LOW);
  }

  String timestamp = getFormattedTime();

  // Create a new log entry using the timestamp
  String logPath = "/logs/log_" + String(convertToSeconds(timestamp.c_str())); // Unique log path

  Firebase.setFloat(firebaseData, logPath + "/temperature", temperature);
  Firebase.setFloat(firebaseData, logPath + "/humidity", humidity);
  Firebase.setFloat(firebaseData, logPath + "/light", lightLevel);
  // Firebase.setInt(firebaseData, logPath + "/distance", distance);
  Firebase.setString(firebaseData, logPath + "/time", timestamp);
  Firebase.setString(firebaseData, logPath + "/times", String(convertToSeconds(timestamp.c_str())));

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Delay between readings
  delay(1000);
}
