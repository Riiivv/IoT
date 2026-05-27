#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include <esp_sleep.h>

#include "secrets.h"

// Function declarations
void handleFeedback(int index);
void startupAnimation();
void printFeedbackCounts();
void connectWiFi();
void connectMQTT();
void setupTime();
void goToDeepSleep();
void printWakeupReason();

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

// Go to deep sleep after 1 minute without button press
const unsigned long sleepAfter = 60000;

// Wake automatically after 1 minute
const uint64_t sleepTimeUs = 60ULL * 1000000ULL;

// Track last button activity
unsigned long lastActivityTime = 0;

// Wake on any button HIGH
const uint64_t wakeupButtonMask =
  (1ULL << 32) |
  (1ULL << 33) |
  (1ULL << 25) |
  (1ULL << 26);

// Prevent multiple button presses during feedback
bool locked = false;

void setup()
{
  Serial.begin(115200);
  delay(500);

  printWakeupReason();

  // Setup LEDs and buttons
  for (int i = 0; i < 4; i++)
  {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);

    pinMode(buttonPins[i], INPUT_PULLDOWN);
  }

  startupAnimation();

  connectWiFi();

  setupTime();

  // TLS without certificate validation
  espClient.setInsecure();
  espClient.setHandshakeTimeout(30);

  client.setServer(MQTT_HOST, MQTT_PORT);

  connectMQTT();

  lastActivityTime = millis();

  Serial.println("Smiley feedback klar");
}

void loop()
{
  if (!client.connected())
  {
    connectMQTT();
  }

  client.loop();

  if (locked) return;

  for (int i = 0; i < 4; i++)
  {
    if (digitalRead(buttonPins[i]) == HIGH)
    {
      delay(debounceDelay);

      if (digitalRead(buttonPins[i]) == HIGH)
      {
        handleFeedback(i);
      }
    }
  }

  // If no button has been pressed for 1 minute, save power
  if (millis() - lastActivityTime >= sleepAfter)
  {
    goToDeepSleep();
  }
}

void setupTime()
{
  // UTC+2 for Danish summer time
  configTime(7200, 0, "pool.ntp.org", "time.nist.gov");

  Serial.print("Venter på dansk tid");

  time_t now = time(nullptr);

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
  locked = true;
  lastActivityTime = millis();

  feedbackCounts[index]++;

  Serial.print("Tak for din feedback: ");
  Serial.println(feedbackNames[index]);

  printFeedbackCounts();

  time_t now = time(nullptr);
  struct tm *timeinfo = localtime(&now);

  char timestamp[30];

  strftime(
    timestamp,
    sizeof(timestamp),
    "%Y-%m-%dT%H:%M:%S",
    timeinfo
  );

  String message = "{";
  message += "\"value\":\"" + String(feedbackValues[index]) + "\",";
  message += "\"timestamp\":\"" + String(timestamp) + "\",";
  message += "\"button\":" + String(index + 1);
  message += "}";

  bool sent = client.publish(
    "/devices/device05/Button",
    message.c_str(),
    false
  );

  if (sent)
  {
    Serial.println("MQTT JSON sendt!");
    Serial.println(message);
  }
  else
  {
    Serial.println("MQTT besked FEJLEDE!");
  }

  digitalWrite(ledPins[index], HIGH);

  delay(lockTime);

  digitalWrite(ledPins[index], LOW);

  while (digitalRead(buttonPins[index]) == HIGH)
  {
    delay(10);
  }

  lastActivityTime = millis();
  locked = false;
}

void goToDeepSleep()
{
  Serial.println("Ingen aktivitet i 1 minut.");
  Serial.println("Går i DeepSleep...");

  client.disconnect();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  delay(500);

  // Wake after 1 minute
  esp_sleep_enable_timer_wakeup(sleepTimeUs);

  // Wake if any button goes HIGH
  esp_sleep_enable_ext1_wakeup(wakeupButtonMask, ESP_EXT1_WAKEUP_ANY_HIGH);

  esp_deep_sleep_start();
}

void printWakeupReason()
{
  esp_sleep_wakeup_cause_t wakeupReason = esp_sleep_get_wakeup_cause();

  if (wakeupReason == ESP_SLEEP_WAKEUP_TIMER)
  {
    Serial.println("Vågnet fra DeepSleep: Timer");
  }
  else if (wakeupReason == ESP_SLEEP_WAKEUP_EXT1)
  {
    Serial.println("Vågnet fra DeepSleep: Knaptryk");
  }
  else
  {
    Serial.println("Normal start");
  }
}

void printFeedbackCounts()
{
  Serial.println("Feedback status:");

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
  for (int i = 0; i < 4; i++)
  {
    digitalWrite(ledPins[i], HIGH);
    delay(200);
    digitalWrite(ledPins[i], LOW);
  }

  delay(200);

  for (int i = 0; i < 4; i++)
  {
    digitalWrite(ledPins[i], HIGH);
  }

  delay(300);

  for (int i = 0; i < 4; i++)
  {
    digitalWrite(ledPins[i], LOW);
  }
}

void connectWiFi()
{
  Serial.println("Forbinder til WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi forbundet!");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT()
{
  while (!client.connected())
  {
    Serial.println("Forbinder til MQTT...");

    if (client.connect("device05", MQTT_USERNAME, MQTT_PASSWORD))
    {
      Serial.println("MQTT forbundet!");
    }
    else
    {
      Serial.print("Fejl: ");
      Serial.println(client.state());

      delay(2000);
    }
  }
}