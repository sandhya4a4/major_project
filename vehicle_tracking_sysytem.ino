#include <WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <TinyGPS++.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ThingSpeak.h>
#include <HTTPClient.h>

// ===== WIFI =====
const char* ssid = "vivo T4 5G";
const char* password = "12345677";

// ===== TELEGRAM =====
String botToken = "8700079269:AAEy2QHcTI751UtiDSrmCBnQdIC_WKTmOwQ";
String chatID = "5952100508";

// ===== THINGSPEAK =====
unsigned long channelID = 3313233;
const char* writeAPIKey = "HJ108OUFRVNNYVSD";

WiFiClient client;

// ===== RFID =====
#define SS_PIN 5
#define RST_PIN 27
MFRC522 rfid(SS_PIN, RST_PIN);

// ===== GPS =====
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);

// ===== OLED =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ===== PINS =====
#define BUZZER 2
#define LED_GREEN 15
#define LED_RED 13
#define RELAY 14  // SOLENOID LOCK

// ===== AUTHORIZED UID =====
String card1 = "F9 2E AB 04";
String card2 = "9C C8 7C 05";

void setup() {
  Serial.begin(115200);

  SPI.begin();
  rfid.PCD_Init();

  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  //gsmSerial.begin(9600, SERIAL_8N1, 26, 25);

  pinMode(BUZZER, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(RELAY, OUTPUT);

  digitalWrite(RELAY, HIGH); // LOCK OFF initially

  // OLED
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // WIFI
  WiFi.begin(ssid, password);
  display.println("Connecting WiFi...");
  display.display();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  display.clearDisplay();
  display.println("WiFi Connected");
  display.display();

  ThingSpeak.begin(client);
}

void loop() {
  checkRFID();
  readGPS();
  delay(1000);
}

// ===== RFID =====
void checkRFID() {

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial())
    return;

  String tag = "";

  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) tag += "0";
    tag += String(rfid.uid.uidByte[i], HEX);
    if (i != rfid.uid.size - 1) tag += " ";
  }

  tag.toUpperCase();
  Serial.println("Card UID: " + tag);

  if(tag == card1 || tag == card2) {
    accessGranted();
  }
  else{
    accessDenied();
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  rfid.PCD_Init(); 
}

// ===== ACCESS GRANTED =====
void accessGranted() {

  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_RED, LOW);

  digitalWrite(BUZZER, HIGH);
  delay(500);
  digitalWrite(BUZZER, LOW);

  // 🔓 UNLOCK SOLENOID
  digitalWrite(RELAY, LOW);
  
  display.clearDisplay();
  display.println("ACCESS GRANTED");
  display.display();

  sendTelegram("Access Granted");

  delay(2000); // keep door open

  // 🔒 LOCK AGAIN
  digitalWrite(RELAY, HIGH);
  digitalWrite(LED_GREEN, LOW);
}

// ===== ACCESS DENIED =====
void accessDenied() {

  digitalWrite(LED_RED, HIGH);

  digitalWrite(BUZZER, HIGH);
  delay(1000);
  digitalWrite(BUZZER, LOW);

  display.clearDisplay();
  display.println("ACCESS DENIED");
  display.display();

  sendTelegram("Unauthorized Access! https://maps.app.goo.gl/yn5V7jJM2LpjB9eFA");
}

// ===== GPS =====
void readGPS() {

  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  if (gps.location.isUpdated()) {

    float lat = gps.location.lat();
    float lng = gps.location.lng();

    Serial.println("GPS Updated");

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Location:");
    display.println(lat, 6);
    display.println(lng, 6);
    display.display();

    sendToThingSpeak(lat, lng);

    delay(2000);  // give time for GPS
  }
  else {
    Serial.println("Waiting for GPS...");
  }
}

// ===== THINGSPEAK =====
void sendToThingSpeak(float lat, float lng) {

  ThingSpeak.setField(1, lat);
  ThingSpeak.setField(2, lng);
  ThingSpeak.writeFields(channelID, writeAPIKey);

  delay(15000);
}

// ===== TELEGRAM =====
void sendTelegram(String message) {

  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = "https://api.telegram.org/bot" + botToken +
                 "/sendMessage?chat_id=" + chatID +
                 "&text=" + message;

    http.begin(url);
    http.GET();
    http.end();
  }
}