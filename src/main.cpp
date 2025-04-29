#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"

// RGB LED pins
#define RED_PIN 27   
#define GREEN_PIN 26
#define BLUE_PIN 25

#define AWS_IOT_SUBSCRIBE_TOPIC "raspberry-pi/alerts"
 
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// Timing variables
const unsigned long LOOP_INTERVAL = 50;  // Check for messages every 50ms

// Function to set RGB LED color
void setLEDColor(int red, int green, int blue) {
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue);
}

void messageHandler(char* topic, byte* payload, unsigned int length) {
  Serial.print("incoming: ");
  Serial.println(topic);
 
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  
  // Check if message contains LED command
  int red = 230;
  int green = 57;
  int blue = 70;
  
  Serial.printf("Setting LED - R:%d G:%d B:%d\n", red, green, blue);
  setLEDColor(red, green, blue);
  const char* message = doc["message"];
  Serial.println(message);
  
}
 
void connectAWS() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println("Connecting to Wi-Fi");
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
 
  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);
 
  // Create a message handler
  client.setCallback(messageHandler);
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME))
  {
    Serial.print(".");
    delay(100);
  }
 
  if (!client.connected())
  {
    Serial.println("AWS IoT Timeout!");
    return;
  }
 
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
 
  Serial.println("AWS IoT Connected!");
}

void setup() {
  Serial.begin(115200);
  
  // Initialize RGB LED pins
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  
  connectAWS();
}
 
void loop() {
  // Ensure we're still connected to AWS IoT
  if (!client.connected()) {
    Serial.println("AWS IoT disconnected. Reconnecting...");
    connectAWS();
  }
  
  // Call client.loop() frequently to process incoming messages quickly
  // and chnage the LED color with almost no delay
  client.loop();
  
  // Small delay to prevent CPU overload
  delay(LOOP_INTERVAL);
}