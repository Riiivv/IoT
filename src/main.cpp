#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>

#include "secrets.h"

// Function declarations
void handleFeedback(int index);
void startupAnimation();
void printFeedbackCounts();
void connectWiFi();
void connectMQTT();
void setupTime();

// Secure WiFi client for MQTT TLS connection
WiFiClientSecure espClient;

// MQTT client
PubSubClient client(espClient);

// LED pins
const int ledPins[4] = {21, 19, 18, 5};

// Button pins
const int buttonPins[4] = {32, 33, 25, 26};

// MQTT feedback values
const char* feedbackValues[4] = {
  "angry",
  "sad",
  "happy",
  "very_happy"
};

// Friendly names for Serial Monitor
const char* feedbackNames[4] = {
  "Meget sur 😡",
  "Utilfreds 😕",
  "Tilfreds 🙂",
  "Meget glad 😄"
};

// Counter for each feedback type
int feedbackCounts[4] = {0, 0, 0, 0};

// LED feedback time
const unsigned long lockTime = 3000;

// Debounce delay for buttons
const unsigned long debounceDelay = 50;

// Prevent multiple button presses during feedback
bool locked = false;

void setup()
{
  // Start Serial Monitor
  Serial.begin(115200);

  // Setup LEDs and buttons
  for (int i = 0; i < 4; i++)
  {
    // LED output
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);

    // Button input with pulldown resistor
    pinMode(buttonPins[i], INPUT_PULLDOWN);
  }

  // Run startup LED animation
  startupAnimation();

  // Connect to WiFi
  connectWiFi();

  // Get Danish time from NTP server
  setupTime();

  // Skip certificate validation (temporary solution)
  espClient.setInsecure();

  // TLS handshake timeout
  espClient.setHandshakeTimeout(30);

  // MQTT broker settings
  client.setServer(MQTT_HOST, MQTT_PORT);

  // Connect to MQTT broker
  connectMQTT();

  Serial.println("Smiley feedback klar");
}

void loop()
{
  // Reconnect MQTT if disconnected
  if (!client.connected())
  {
    connectMQTT();
  }

  // Keep MQTT connection alive
  client.loop();

  // Ignore input while locked
  if (locked) return;

  // Check all buttons
  for (int i = 0; i < 4; i++)
  {
    // Button pressed
    if (digitalRead(buttonPins[i]) == HIGH)
    {
      // Debounce delay
      delay(debounceDelay);

      // Confirm button still pressed
      if (digitalRead(buttonPins[i]) == HIGH)
      {
        handleFeedback(i);
      }
    }
  }
}

void setupTime()
{
  // UTC+2 for Denmark summer time
  configTime(7200, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print("Venter på dansk tid");

  time_t now = time(nullptr);

  // Wait until valid time is received
  while (now < 100000)
  {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }

  Serial.println();
  Serial.println("Dansk tid OK!");
  Serial.println(ctime(&now));
}

void handleFeedback(int index)
{
  // Lock buttons during processing
  locked = true;

  // Increase counter
  feedbackCounts[index]++;

  // Print feedback to Serial Monitor
  Serial.print("Tak for din feedback: ");
  Serial.println(feedbackNames[index]);

  // Print all counters
  printFeedbackCounts();

  // Get current time
  time_t now = time(nullptr);

  // Convert time to readable format
  struct tm *timeinfo = localtime(&now);

  // Timestamp buffer
  char timestamp[30];

  // Format timestamp
  strftime(
    timestamp,
    sizeof(timestamp),
    "%Y-%m-%dT%H:%M:%S",
    timeinfo
  );

  // Create JSON message
  String message = "{";
  message += "\"value\":\"" + String(feedbackValues[index]) + "\",";
  message += "\"timestamp\":\"" + String(timestamp) + "\",";
  message += "\"button\":" + String(index + 1);
  message += "}";

  // Publish MQTT message
  bool sent = client.publish(
    "/devices/device05/Button",
    message.c_str(),
    false
  );

  // Print result
  if (sent)
  {
    Serial.println("MQTT JSON sendt!");
    Serial.println(message);
  }
  else
  {
    Serial.println("MQTT besked FEJLEDE!");
  }

  // Turn on selected LED
  digitalWrite(ledPins[index], HIGH);

  // Keep LED on
  delay(lockTime);

  // Turn LED off
  digitalWrite(ledPins[index], LOW);

  // Wait until button released
  while (digitalRead(buttonPins[index]) == HIGH)
  {
    delay(10);
  }

  // Unlock buttons
  locked = false;
}

void printFeedbackCounts()
{
  Serial.println("Feedback status:");

  // Print all counters
  for (int i = 0; i < 4; i++)
  {
    Serial.print(feedbackNames[i]);
    Serial.print(": ");
    Serial.println(feedbackCounts[i]);
  }

  Serial.println("------------------");
}

void startupAnimation()
{
  // LED sweep animation
  for (int i = 0; i < 4; i++)
  {
    digitalWrite(ledPins[i], HIGH);
    delay(200);
    digitalWrite(ledPins[i], LOW);
  }

  delay(200);

  // Turn all LEDs on
  for (int i = 0; i < 4; i++)
  {
    digitalWrite(ledPins[i], HIGH);
  }

  delay(300);

  // Turn all LEDs off
  for (int i = 0; i < 4; i++)
  {
    digitalWrite(ledPins[i], LOW);
  }
}

void connectWiFi()
{
  Serial.println("Forbinder til WiFi...");

  // Start WiFi connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi forbundet!");

  // Print IP address
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT()
{
  // Keep trying until connected
  while (!client.connected())
  {
    Serial.println("Forbinder til MQTT...");

    // Connect to broker
    if (client.connect("device05", MQTT_USERNAME, MQTT_PASSWORD))
    {
      Serial.println("MQTT forbundet!");
    }
    else
    {
      // Print MQTT error
      Serial.print("Fejl: ");
      Serial.println(client.state());

      delay(2000);
    }
  }
}