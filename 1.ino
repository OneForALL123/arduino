#define BLYNK_TEMPLATE_ID "TMPL6EBtI6Qbr"
#define BLYNK_TEMPLATE_NAME "TKT"
#define BLYNK_AUTH_TOKEN "_BvKZv6oDTvM7a761pbOG1sTq-71SX4b"

#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <BlynkSimpleEsp8266.h>
#include <FirebaseESP8266.h>
#include <ESP8266WiFi.h>

// OLED configuration
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET    -1 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// WiFi and Blynk credentials
char auth[] = BLYNK_AUTH_TOKEN;  
char ssid[] = "PTIT.HCM_CanBo";  
char pass[] = ""; 

// Firebase credentials
#define FIREBASE_HOST "https://dht11-6dd52-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "58E3S1C6DHahdouqX5Tq54MwIN57XRuHewcv2uYV"

// Blynk virtual pins
#define V_PIN_TEMP_OBJECT V0
#define V_PIN_TEMP_AMBIENT V1
#define V_PIN_HEART_RATE V2
#define V_PIN_SPO2 V3

MAX30105 particleSensor;
Adafruit_MLX90614 mlx = Adafruit_MLX90614();
FirebaseData fbdo;

uint32_t irBuffer[100];  
uint32_t redBuffer[100]; 
int32_t bufferLength = 100;  
int32_t spo2;          
int8_t validSPO2;      
int32_t heartRate;     
int8_t validHeartRate; 

#define SDA_PIN 4 
#define SCL_PIN 5 

void setup() {
  Serial.begin(115200);

  // Initialize Blynk
  Blynk.begin(auth, ssid, pass);

  // Initialize I2C for ESP8266
  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialize MLX90614 sensor
  if (!mlx.begin()) {
    Serial.println("Error connecting to MLX90614 sensor. Check your wiring!");
    while (1);
  }

  // Initialize MAX30105 sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println(F("MAX30105 not found. Check your wiring/power."));
    while (1);
  }

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1);
  }

  display.clearDisplay();
  display.display();

  // Initialize Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);

  byte ledBrightness = 60; 
  byte sampleAverage = 4;  
  byte ledMode = 2;        
  byte sampleRate = 100;   
  int pulseWidth = 411;    
  int adcRange = 4096;     

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
}

void loop() {
  // Keep Blynk connected
  Blynk.run();

  // Read heart rate and SpO2 data
  for (byte i = 0; i < bufferLength; i++) {
    while (particleSensor.available() == false)
      particleSensor.check();

    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
    Serial.print(F("red="));
    Serial.print(redBuffer[i], DEC);
    Serial.print(F(", ir="));
    Serial.println(irBuffer[i], DEC);
  }

  // Calculate heart rate and SpO2
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

  // Send data to Blynk
  Blynk.virtualWrite(V_PIN_HEART_RATE, heartRate);
  Blynk.virtualWrite(V_PIN_SPO2, spo2);

  // Display data on OLED and send to Firebase
  while (1) {
    for (byte i = 25; i < 100; i++) {
      redBuffer[i - 25] = redBuffer[i];
      irBuffer[i - 25] = irBuffer[i];
    }

    for (byte i = 75; i < 100; i++) {
      while (particleSensor.available() == false)
        particleSensor.check();

      redBuffer[i] = particleSensor.getRed();
      irBuffer[i] = particleSensor.getIR();
      particleSensor.nextSample();

      maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

      // Read temperatures from MLX90614
      float ambientTempC = mlx.readAmbientTempC();
      float objectTempC = mlx.readObjectTempC();

      // Update OLED display
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.print("BPM: ");
      display.print(heartRate);
      display.setCursor(0, 16);
      display.print("SpO2: ");
      display.print(spo2);
      display.setCursor(0, 32);
      display.print("Ambient: ");
      display.print(ambientTempC);
      display.print(" *C");
      display.setCursor(0, 48);
      display.print("Body: ");
      display.print(objectTempC);
      display.print(" *C");
      display.display();

      // Send data to Firebase
      Firebase.setFloat(fbdo, "Heart Rate", heartRate);
      Firebase.setFloat(fbdo, "SpO2", spo2);
      Firebase.setFloat(fbdo, "Ambient Temperature", ambientTempC);
      Firebase.setFloat(fbdo, "Body Temperature", objectTempC);

      // Send temperature data to Blynk
      Blynk.virtualWrite(V_PIN_TEMP_AMBIENT, ambientTempC);
      Blynk.virtualWrite(V_PIN_TEMP_OBJECT, objectTempC);

      delay(1000);
    }
  }
}
