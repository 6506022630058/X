#include <DHT.h>
#include <ESP8266WiFi.h>
#include "FirebaseESP8266.h"
#include <time.h> // For NTP time
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TridentTD_LineNotify.h>

#define DHTPIN D7     // DHT22 data pin connected to D4 (GPIO2)
#define DHTTYPE DHT22 // DHT 22 (AM2302)

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1  // No reset pin

#define LINE_TOKEN  "BuUMdhNAg59blNKybtfL6jE6E2SWLu1ZwT6hTKIkSOb"

String data;
int humidsoil, waterlevel;
int analogPin = A0;
String fertilize = "no";
String pumpstate = "no";
float humidsoilpercent;
int count = 0;
int relay = 2;

const char* ssid = "B415";
const char* password = "appletv415";

char FIREBASE_AUTH[] = "VwrzCLVoAVZXIFTgYKbsjC53AuDuh2QqaHkxVFop"; // Your Firebase Web API Key
char FIREBASE_HOST[] = "https://saiduen-farm-default-rtdb.asia-southeast1.firebasedatabase.app/"; // Your Firebase Host URL


DHT dht(DHTPIN, DHTTYPE);
WiFiServer server(80);
FirebaseData firebaseData;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


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
  server.begin(); // Starts the Server
  Serial.println("Server started");

  Serial.print("IP Address of network: ");
  Serial.println(WiFi.localIP());
  Serial.print("Copy and paste the following URL: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
}

void setup() {
  Serial.begin(9600);  // Initialize serial communication at 9600 baud rate
  dht.begin();
  setup_wifi();
  pinMode(relay, OUTPUT); // Set relay pin as an output
  digitalWrite(relay, LOW); 
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  LINE.setToken(LINE_TOKEN);
  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov"); // Time offset for Thailand
  Serial.println("\nWaiting for NTP time sync...");
  while (!time(nullptr)) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nTime synced!");
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
  int pH = analogRead(analogPin);
  if (pH < 247){
    fertilize = "yes";
  }else{
    fertilize = "no";
  }
  
  if (Serial.available()) {
    data = Serial.readStringUntil('\n'); // Read the incoming data until a newline character
    sscanf(data.c_str(), "%d,%d,%d", &humidsoil, &count, &waterlevel); // Parse the data
    waterlevel = waterlevel * 4/7;
    humidsoilpercent = (1000-humidsoil)/10;
    if (humidsoilpercent > 100 || humidsoilpercent < 0){
      humidsoilpercent = 50;
    }
    Serial.print("humid-soil = ");
    Serial.println(humidsoil);
    Serial.print("humid-soil-percent = ");
    Serial.println(humidsoilpercent);
    Serial.print("water-level ");
    Serial.println(waterlevel);
    Serial.print("pH = ");
    Serial.println(pH);
    Serial.print("fertilize = ");
    Serial.println(fertilize);
    Serial.print("pumpstate = ");
    Serial.println(pumpstate);
    Serial.print("temp = ");
    Serial.println(temperature);
    Serial.print("humid = ");
    Serial.println(humidity);
    Serial.println("------------------------");
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
  display.print(F("Humidsoil: "));
  display.print(humidsoilpercent);
  display.println(F(" %"));

  display.setCursor(0, 48);
  display.print(F("Waterlevel: "));
  display.print(waterlevel);
  display.println(F(" mm"));

  display.display();  // Show the updated content on OLED

  if (waterlevel > 10) {
    if (LINE.notify("Warning: Water level is above 10 cm!!")) {
      Serial.println("Notification sent successfully");
    }
  }

  if (humidsoilpercent < 45){
      digitalWrite(relay, HIGH); // Turn LED1 ON
      pumpstate = "yes";
    }else{
      digitalWrite(relay, LOW); // Turn LED1 ON
      pumpstate = "no";
    }
    String timestamp = getFormattedTime();

    // Create a new log entry using the timestamp
    String logPath2 = "/logs_5sec/log_1"; // Unique log path
    String logPath1 = "/logs_10min/log_" + String(convertToSeconds(timestamp.c_str())); // Unique log path

    if(count == 10){

    Firebase.setFloat(firebaseData, logPath1 + "/temperature", temperature);
    Firebase.setFloat(firebaseData, logPath1 + "/humidity", humidity);
    Firebase.setInt(firebaseData, logPath1 + "/humidsoil", humidsoil);
    Firebase.setFloat(firebaseData, logPath1 + "/humidsoilpercent", humidsoilpercent);
    Firebase.setInt(firebaseData, logPath1 + "/waterlevel", waterlevel);
    Firebase.setInt(firebaseData, logPath1 + "/pH", pH);
    Firebase.setString(firebaseData, logPath1 + "/fertilize", fertilize);
    Firebase.setString(firebaseData, logPath1 + "/pumpstate", pumpstate);
    Firebase.setString(firebaseData, logPath1 + "/time", timestamp);
    Firebase.setString(firebaseData, logPath1 + "/times", String(convertToSeconds(timestamp.c_str())));
    }
    else{
    Firebase.setFloat(firebaseData, logPath2 + "/temperature", temperature);
    Firebase.setFloat(firebaseData, logPath2 + "/humidity", humidity);
    Firebase.setInt(firebaseData, logPath2 + "/humidsoil", humidsoil);
    Firebase.setFloat(firebaseData, logPath2 + "/humidsoilpercent", humidsoilpercent);
    Firebase.setInt(firebaseData, logPath2 + "/waterlevel", waterlevel);
    Firebase.setInt(firebaseData, logPath2 + "/pH", pH);
    Firebase.setString(firebaseData, logPath2 + "/fertilize", fertilize);
    Firebase.setString(firebaseData, logPath2 + "/pumpstate", pumpstate);
    Firebase.setString(firebaseData, logPath2 + "/time", timestamp);
    Firebase.setString(firebaseData, logPath2 + "/times", String(convertToSeconds(timestamp.c_str())));

    }
    WiFiClient client = server.available();
    if (!client)
    {
      return;
    }
    Serial.println("New client connected");

    // while (!client.available())
    // {
    //   delay(1);
    // }

    String request = client.readStringUntil('\r');
    Serial.println(request);
    client.flush();

    int value1 = LOW;
    
    if (request.indexOf("/relay=ON") != -1)
    {
      digitalWrite(relay, HIGH); // Turn LED1 ON
      pumpstate = "yes";
      value1 = HIGH;
    }
    if (request.indexOf("/relay=OFF") != -1)
    {
      digitalWrite(relay, LOW); // Turn LED1 OFF
      pumpstate = "no";
      value1 = LOW;
    }

    // HTTP response
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("");
    client.println("<!DOCTYPE HTML>");
    client.println("<html>");

    client.println("<h1>Saiduen-Din</h1>");

    client.print("CONTROL Motor: <br>");
    
    client.print("Motor: ");
    if (value1 == HIGH)
    {
      client.print("ON");
    }
    else
    {
      client.print("OFF");
    }
    client.println("<br><br>");
    client.println("<a href=\"/relay=ON\"><button>ON</button></a>");
    client.println("<a href=\"/relay=OFF\"><button>OFF</button></a><br /><br />");

    client.println("<img src=\"https://st3.depositphotos.com/9876904/16286/v/450/depositphotos_162862798-stock-illustration-worm-crawling-on-white-background.jpg\" width=\"200\" height=\"150\"></img>");
    
    client.println("</html>");

    // delay(1);
    Serial.println("Client disconnected");
    Serial.println("");

    Firebase.setString(firebaseData, logPath2 + "/pumpstate", pumpstate);
    // Now you can use humidsoil, waterlevel, var3, var4, var5 as needed
    // Update OLED display with sensor data
  }
}
