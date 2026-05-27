#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <driver/i2s.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// --- Screen Settings ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Wi-Fi & ROS 2 UDP Settings ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
WiFiUDP udp;
const unsigned int localUdpPort = 4210; // Port listening for ROS 2 commands
char incomingPacket[255];

// --- Configuration Pins for ESP32-S3 ---
// OLED I2C Pins
#define OLED_SDA        8
#define OLED_SCL        9

// I2S Microphone Pins (INMP441)
#define I2S_MIC_WS      15
#define I2S_MIC_SD      16
#define I2S_MIC_SCK     17

// I2S Speaker Pins (MAX98357A)
#define I2S_SPK_DOUT    18
#define I2S_SPK_BCLK    19
#define I2S_SPK_LRCK    20

// Servo Pin
#define SERVO_PIN       21

// --- Audio Configuration ---
#define SAMPLE_RATE     16000
#define AUDIO_BUFFER_LEN 1024
int16_t audioBuffer[AUDIO_BUFFER_LEN];

Servo alexServo;

// --- Render Expressions on OLED ---
void drawExpression(String expression) {
    display.clearDisplay();
    if (expression == "HAPPY") {
        display.fillRoundRect(30, 20, 20, 30, 10, SSD1306_WHITE);
        display.fillRoundRect(78, 20, 20, 30, 10, SSD1306_WHITE);
    } else if (expression == "SAD") {
        display.fillTriangle(30, 40, 50, 20, 30, 20, SSD1306_WHITE);
        display.fillTriangle(98, 40, 78, 20, 98, 20, SSD1306_WHITE);
    } else { // NEUTRAL
        display.fillCircle(40, 32, 12, SSD1306_WHITE);
        display.fillCircle(88, 32, 12, SSD1306_WHITE);
    }
    display.display();
}

void initMicrophone() {
    i2s_config_t mic_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 512,
        .use_apll = false
    };
    i2s_pin_config_t mic_pins = {
        .bck_io_num = I2S_MIC_SCK,
        .ws_io_num = I2S_MIC_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_MIC_SD
    };
    i2s_driver_install(I2S_NUM_0, &mic_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &mic_pins);
}

void initSpeaker() {
    i2s_config_t spk_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 512,
        .use_apll = false
    };
    i2s_pin_config_t spk_pins = {
        .bck_io_num = I2S_SPK_BCLK,
        .ws_io_num = I2S_SPK_LRCK,
        .data_out_num = I2S_SPK_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };
    i2s_driver_install(I2S_NUM_1, &spk_config, 0, NULL);
    i2s_set_pin(I2S_NUM_1, &spk_pins);
}

void setup() {
    Serial.begin(115200);

    // Start I2C bus with custom pins
    Wire.begin(OLED_SDA, OLED_SCL);
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
        Serial.println("OLED allocation failed");
    }
    
    drawExpression("NEUTRAL");

    // Network Setup
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Active.");
    udp.begin(localUdpPort);

    // Audio initializations
    initMicrophone();
    initSpeaker();
    
    // Set servo rest/idle posture to 0 degrees
    alexServo.attach(SERVO_PIN);
    alexServo.write(0); 
    
    Serial.println("Alex Bot Engine Ready.");
}

void loop() {
    // Process Network Commands from ROS 2 Node
    int packetSize = udp.parsePacket();
    if (packetSize) {
        int len = udp.read(incomingPacket, 255);
        if (len > 0) incomingPacket[len] = 0;
        
        String command = String(incomingPacket);
        
        if (command.startsWith("EXP:")) {
            drawExpression(command.substring(4));
        } else if (command.startsWith("SRV:")) {
            alexServo.write(command.substring(4).toInt());
        }
    }

    // Audio Pipeline Passthrough
    size_t bytesRead = 0;
    size_t bytesWritten = 0;
    i2s_read(I2S_NUM_0, &audioBuffer, sizeof(audioBuffer), &bytesRead, 0);
    if (bytesRead > 0) {
        i2s_write(I2S_NUM_1, &audioBuffer, bytesRead, &bytesWritten, 0);
    }
}

