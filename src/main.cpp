#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <driver/i2s.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "BluetoothA2DPSource.h"

// --- Screen Settings ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Wi-Fi & ROS 2 UDP Settings ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
WiFiUDP udp;
const unsigned int localUdpPort = 4210; 
char incomingPacket[255];

// --- Bluetooth Speaker Name ---
const char* bluetooth_speaker_name = "YOUR_BT_SPEAKER_NAME"; 
BluetoothA2DPSource a2dp_source;

// --- Classic ESP32 Dedicated Pins ---
#define OLED_SDA        21
#define OLED_SCL        22

#define I2S_MIC_WS      15
#define I2S_MIC_SD      32
#define I2S_MIC_SCK     14

#define LEFT_SERVO_PIN  13
#define RIGHT_SERVO_PIN 12

// --- Audio Configuration ---
#define SAMPLE_RATE     16000
#define AUDIO_BUFFER_LEN 512
int16_t audioBuffer[AUDIO_BUFFER_LEN];

Servo leftServo;
Servo rightServo;

// --- Bluetooth Stream Data Provider ---
int32_t get_audio_data(Channels *channels, int32_t len) {
    size_t bytesRead = 0;
    // Read sound directly from the I2S Microphone
    i2s_read(I2S_NUM_0, &audioBuffer, len * sizeof(int16_t), &bytesRead, 0);
    
    int32_t samples = bytesRead / sizeof(int16_t);
    for (int32_t i = 0; i < samples; i++) {
        channels[i].left = audioBuffer[i];
        channels[i].right = audioBuffer[i]; // Duplicate mono mic to stereo speaker
    }
    return samples;
}

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

void setup() {
    Serial.begin(115200);

    Wire.begin(OLED_SDA, OLED_SCL);
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
        Serial.println("OLED failed");
    }
    drawExpression("NEUTRAL");

    // Network Setup
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    udp.begin(localUdpPort);

    initMicrophone();
    
    // Connect to Bluetooth Speaker wirelessly
    a2dp_source.start(bluetooth_speaker_name, get_audio_data);

    // Setup Arm Servos to standard rest posture (0 degrees)
    leftServo.attach(LEFT_SERVO_PIN);
    leftServo.write(0); 
    rightServo.attach(RIGHT_SERVO_PIN);
    rightServo.write(0);
}

void loop() {
    // Read Wi-Fi packets for ROS 2 Commands
    int packetSize = udp.parsePacket();
    if (packetSize) {
        int len = udp.read(incomingPacket, 255);
        if (len > 0) incomingPacket[len] = 0;
        
        String command = String(incomingPacket);
        if (command.startsWith("EXP:")) {
            drawExpression(command.substring(4));
        } else if (command.startsWith("SRVL:")) {
            leftServo.write(command.substring(5).toInt());
        } else if (command.startsWith("SRVR:")) {
            rightServo.write(command.substring(5).toInt());
        }
    }
    delay(10); // Yield to keep background Bluetooth stack stable
}
