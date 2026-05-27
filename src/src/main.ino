#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>

// Screen setup (128x64)
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Servo setup
Servo myServo;

void setup() {
  Serial.begin(115200);

  // Initialize Display
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Alex-botv3 Initialized");
  display.display();
  
  // Initialize Servo
  myServo.attach(18); // GPIO 18
  
  Serial.println("Alex-botv3 Initialized!");
}

void loop() {
  // Main logic for your Alex bot
  delay(1000);
}
