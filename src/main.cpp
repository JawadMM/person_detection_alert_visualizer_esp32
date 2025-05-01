#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"

// RGB LED pins
#define RED_PIN 27   
#define GREEN_PIN 26
#define BLUE_PIN 25

// The topic from Raspberry Pi's person detection
#define AWS_IOT_SUBSCRIBE_TOPIC "person-detection/alerts"

// LED states
#define LED_OFF 0
#define LED_RED 1
#define LED_GREEN 2
#define LED_BLUE 3

WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// Timing variables
const unsigned long LOOP_INTERVAL = 50;  // Check for messages every 50ms
unsigned long lastDetectionTime = 0;     // Last time a person was detected
const unsigned long TIMEOUT_PERIOD = 3000; // Turn off LED after 3 seconds with no updates

// Current LED state
int currentLedState = LED_OFF;

// Function to set RGB LED color
void setLEDColor(int red, int green, int blue) {
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue);
}

// Set LED based on state
void updateLED(int state) {
  currentLedState = state;
  
  switch (state) {
    case LED_OFF:
      setLEDColor(0, 0, 0);
      break;
    case LED_RED:
      setLEDColor(230, 0, 0);
      break;
    case LED_GREEN:
      setLEDColor(0, 230, 0);
      break;
    case LED_BLUE:
      setLEDColor(0, 0, 230);
      break;
  }
}

void messageHandler(char* topic, byte* payload, unsigned int length) {
  Serial.print("Incoming message on topic: ");
  Serial.println(topic);
 
  // Print the first 100 bytes of payload for debugging
  Serial.print("Payload preview: ");
  for (int i = 0; i < min(100, (int)length); i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  
  // Increase the JSON document size to accommodate the person detection payload
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  
  // Check if parsing succeeded
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
  
  // Extract the detection_count from the message
  int detectionCount = doc["detection_count"];
  Serial.print("Detection count: ");
  Serial.println(detectionCount);
  
  // Update the last detection time
  lastDetectionTime = millis();
  
  // Turn on red LED when people are detected
  if (detectionCount > 0) {
    // People detected - turn on RED LED
    updateLED(LED_RED);
    Serial.print("People detected! Count: ");
    Serial.println(detectionCount);
    
    // Print details of the first detection
    if (doc["detections"].size() > 0) {
      float confidence = doc["detections"][0]["confidence"];
      Serial.print("First detection confidence: ");
      Serial.println(confidence, 4);
    }
  } else {
    // No people detected - turn on GREEN LED
    updateLED(LED_GREEN);
    Serial.println("No people detected");
  }
}
 
void connectAWS() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println("Connecting to Wi-Fi");
 
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Connected to WiFi. IP address: ");
  Serial.println(WiFi.localIP());
 
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
 
  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);
 
  // Create a message handler
  client.setCallback(messageHandler);
 
  Serial.println("Connecting to AWS IoT");
 
  while (!client.connect(THINGNAME)) {
    Serial.print(".");
    delay(100);
  }
 
  if (!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }
 
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
 
  Serial.println("AWS IoT Connected!");
  
  // Blink the blue LED to indicate successful connection
  for (int i = 0; i < 3; i++) {
    updateLED(LED_BLUE);
    delay(200);
    updateLED(LED_OFF);
    delay(200);
  }
  
  // Set to green initially (no people detected)
  updateLED(LED_GREEN);
}

void setup() {
  Serial.begin(115200);
  
  // Initialize RGB LED pins
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  
  // Turn off LED initially
  updateLED(LED_OFF);
  
  // Connect to AWS IoT
  connectAWS();
}
 
void loop() {
  // Ensure we're still connected to AWS IoT
  if (!client.connected()) {
    Serial.println("AWS IoT disconnected. Reconnecting...");
    updateLED(LED_OFF);
    connectAWS();
  }
  
  // Process incoming MQTT messages
  client.loop();
  
  // Check if we haven't received a detection message for a while
  // This handles the case where the Raspberry Pi stops sending messages
  unsigned long now = millis();
  if (currentLedState == LED_RED && (now - lastDetectionTime > TIMEOUT_PERIOD)) {
    Serial.println("No detection updates received recently. Setting LED to GREEN");
    updateLED(LED_GREEN);
  }
  
  // Small delay to prevent CPU overload
  delay(LOOP_INTERVAL);
}