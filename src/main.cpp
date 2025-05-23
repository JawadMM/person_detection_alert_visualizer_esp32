#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"

// RGB LED pins
#define RED_PIN 27   
#define GREEN_PIN 26
#define BLUE_PIN 25

// Topic to subscribe to
#define AWS_IOT_SUBSCRIBE_TOPIC "person-detection/alerts"

// LED states
#define LED_OFF 0
#define LED_RED 1
#define LED_GREEN 2
#define LED_BLUE 3

// Confidence threshold for person detection
#define PERSON_CONFIDENCE_THRESHOLD 0.7

// Increase MQTT buffer size to handle larger messages
#define MQTT_MAX_PACKET_SIZE 2048

WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// Timing variables
const unsigned long LOOP_INTERVAL = 50;      // Check for messages every 50ms
unsigned long lastDetectionTime = 0;         // Last time a person was detected
const unsigned long DETECTION_TIMEOUT = 5000; // 5 seconds timeout
int currentLedState = LED_GREEN;             // Track current LED state

// Function to set RGB LED color
void setLEDColor(int red, int green, int blue) {
  analogWrite(RED_PIN, red);
  analogWrite(GREEN_PIN, green);
  analogWrite(BLUE_PIN, blue);
  Serial.printf("LED set to R:%d G:%d B:%d\n", red, green, blue);
}

// Set LED based on state
void updateLED(int state) {
  currentLedState = state;
  
  switch (state) {
    case LED_OFF:
      setLEDColor(0, 0, 0);
      break;
    case LED_RED:
      setLEDColor(255, 0, 0);
      break;
    case LED_GREEN:
      setLEDColor(0, 255, 0);
      break;
    case LED_BLUE:
      setLEDColor(0, 0, 255);
      break;
  }
}

void messageHandler(char* topic, byte* payload, unsigned int length) {
  // Record receive time for latency calculation
  unsigned long receiveTime = millis();
  
  Serial.print("Incoming message on topic: ");
  Serial.println(topic);
 
  // Print the payload for debugging
  Serial.print("Raw payload (");
  Serial.print(length);
  Serial.print(" bytes): ");
  for (int i = 0; i < min(length, (unsigned int)100); i++) {
    Serial.print((char)payload[i]);
  }
  if (length > 100) Serial.print("...");
  Serial.println();
  
  // Create a temporary buffer to ensure null-termination
  char* jsonBuffer = new char[length + 1];
  memcpy(jsonBuffer, payload, length);
  jsonBuffer[length] = '\0';
  
  // Increase the JSON document size to accommodate larger payloads
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, jsonBuffer);
  
  // Check if parsing succeeded
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    delete[] jsonBuffer;
    return;
  }
  
  // Extract the detection_count from the message
  int detectionCount = doc["detection_count"];
  Serial.print("Detection count: ");
  Serial.println(detectionCount);
  
  // Get the detections array
  JsonArray detections = doc["detections"];
  
  // Flag to track if at least one detection is a confident person
  bool personDetected = false;
  float highestConfidence = 0.0;
  
  // Only proceed if there are detections
  if (detectionCount > 0 && detections.size() > 0) {
    // Print details of the detections
    Serial.print("Number of detections: ");
    Serial.println(detections.size());
    
    // Check each detection
    for (int i = 0; i < detections.size(); i++) {
      int trackId = detections[i]["track_id"];
      float confidence = detections[i]["confidence"];
      
      Serial.print("  Detection #");
      Serial.print(i + 1);
      Serial.print(": ID=");
      Serial.print(trackId);
      Serial.print(", Confidence=");
      Serial.println(confidence);
      
      // Track highest confidence seen
      if (confidence > highestConfidence) {
        highestConfidence = confidence;
      }
      
      // If this is a detection with confidence > threshold, mark as person detected
      if (confidence > PERSON_CONFIDENCE_THRESHOLD) {
        personDetected = true;
      }
    }
    
    // Only turn LED red if a person was detected with sufficient confidence
    if (personDetected) {
      // Calculate processing time
      unsigned long processingTime = millis() - receiveTime;
      
      // Person detected - turn on RED LED
      updateLED(LED_RED);
      
      // Update the last detection time
      lastDetectionTime = millis();
      
      Serial.print("Person detected with confidence above threshold (");
      Serial.print(PERSON_CONFIDENCE_THRESHOLD);
      Serial.println(")! Setting LED to RED");
      Serial.print("Highest confidence: ");
      Serial.println(highestConfidence);
      Serial.print("Processing time: ");
      Serial.print(processingTime);
      Serial.println("ms");
      Serial.println("LED will return to GREEN in 5 seconds");
    } else {
      // Detections exist but none are confident enough to be a person
      updateLED(LED_GREEN);
      Serial.print("Detections found but confidence (");
      Serial.print(highestConfidence);
      Serial.print(") is below threshold (");
      Serial.print(PERSON_CONFIDENCE_THRESHOLD);
      Serial.println("). LED remains GREEN.");
    }
  } else {
    // No detections - turn on GREEN LED
    updateLED(LED_GREEN);
    Serial.println("No detections found. Setting LED to GREEN");
  }
  
  // Clean up
  delete[] jsonBuffer;
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
  
  // Set the buffer size before connecting
  client.setBufferSize(MQTT_MAX_PACKET_SIZE);
 
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
 
  Serial.print("AWS IoT Connected! Subscribed to topic: ");
  Serial.println(AWS_IOT_SUBSCRIBE_TOPIC);
  
  // Testing all LED colors
  Serial.println("Testing LED colors...");
  // Red
  updateLED(LED_RED);
  delay(500);
  // Green
  updateLED(LED_GREEN);
  delay(500);
  // Blue
  updateLED(LED_BLUE);
  delay(500);
  // Off
  updateLED(LED_OFF);
  delay(500);
  // Set to green initially (no people detected)
  updateLED(LED_GREEN);
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Give time for serial to connect
  Serial.println("\n\n=== ESP32 Person Detection Alert System ===");
  Serial.println("With person confidence threshold of " + String(PERSON_CONFIDENCE_THRESHOLD));
  
  // Initialize RGB LED pins
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  
  // Turn off LED initially
  updateLED(LED_OFF);
  
  // Connect to AWS IoT
  connectAWS();
  
  // Start with green (no detection)
  updateLED(LED_GREEN);
  
  Serial.println("System ready - waiting for detections...");
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
  
  // Check if the detection timeout has elapsed
  if (currentLedState == LED_RED) {
    unsigned long now = millis();
    if (now - lastDetectionTime >= DETECTION_TIMEOUT) {
      Serial.println("Detection timeout elapsed (5 seconds). Returning to GREEN");
      updateLED(LED_GREEN);
    }
  }
  
  // Small delay to prevent CPU overload
  delay(LOOP_INTERVAL);
}