#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// WiFi & ThingSpeak Settings
const char* ssid = "Aishani";
const char* password = "Krishna135";
const char* thingspeak_server = "api.thingspeak.com";
const char* write_api_key = "TARBKGKSKEC8PRAR";
const char* read_api_key = "MJLCLFGX0W3M5E1O";
const unsigned long channel_id = 2918587;
const unsigned int field_number = 1;

// Sensor Pins
const int irEntryPin = 17;
const int trigEntryPin = 18;
const int echoEntryPin = 4;
const int irExitPin = 15;
const int trigExitPin = 23;
const int echoExitPin = 22;

// Output Pins
const int ledPin = 5;
const int buzzerPin = 32;
const int relayFanPin = 13;
const int servoSignalPin = 21;

// Timing Constants
const unsigned long publishInterval = 15000; // 30 seconds
const unsigned long avgInterval = 120000;    // 2 minutes
const unsigned long readInterval = 30000;    // 30 seconds
const unsigned long webControlTimeout = 300000; // 1 minute timeout
const unsigned long statusPrintInterval = 1000; // 1 second for status updates

// People Counting Variables
volatile int peopleCount = 0;
bool entryDetected = false;
bool exitDetected = false;

// Data Publishing Variables
unsigned long lastPublishTime = 0;
unsigned long lastReadTime = 0;
unsigned long lastStatusPrintTime = 0;
unsigned long lastReconnectAttempt = 0;
int readingsCount = 0;
long sumPeopleCount = 0;
float avgPeopleCount = 0;

// Servo Control
Servo fanServo;
const int SERVO_STOP = 0;
const int SERVO_RUN = 180;

// Web Control State
bool webControlledLed = false;
bool webControlledBuzzer = false;
bool webControlledFan = false;
unsigned long lastWebControlTime = 0;
// const int peopleCount=0;
void setup() {
  Serial.begin(115200);
  delay(1000); // Ensure serial monitor is ready
  Serial.println("\nSystem Initializing...");

  // Initialize hardware
  pinMode(irEntryPin, INPUT);
  pinMode(trigEntryPin, OUTPUT);
  pinMode(echoEntryPin, INPUT);
  pinMode(irExitPin, INPUT);
  pinMode(trigExitPin, OUTPUT);
  pinMode(echoExitPin, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(relayFanPin, OUTPUT);
  digitalWrite(buzzerPin, HIGH);
  
  fanServo.setPeriodHertz(50);
  fanServo.attach(servoSignalPin, 500, 2400);
  fanServo.write(SERVO_STOP);
  
  connectWiFi();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Handle people counting (always runs)
  handlePeopleCounting();
  
  // Automatic WiFi reconnection
  if (WiFi.status() != WL_CONNECTED && currentMillis - lastReconnectAttempt > 15000) {
    Serial.println("Attempting WiFi reconnection...");
    connectWiFi();
    lastReconnectAttempt = currentMillis;
  }
  
  // Publish data (non-blocking)
  if (currentMillis - lastPublishTime >= publishInterval && WiFi.status() == WL_CONNECTED) {
    lastPublishTime = currentMillis;
    
    // Update average calculation
    sumPeopleCount += peopleCount;
    readingsCount++;
    
    if (currentMillis % avgInterval < publishInterval) {
      avgPeopleCount = (float)sumPeopleCount / readingsCount;
      sumPeopleCount = 0;
      readingsCount = 0;
    }
    
    publishToThingSpeak(peopleCount, avgPeopleCount);
  }
  
  // Check for web commands (non-blocking)
  if (currentMillis - lastReadTime >= readInterval && WiFi.status() == WL_CONNECTED) {
    lastReadTime = currentMillis;
    readThingSpeakField();
  }
  
  // Reset web controls if timeout reached
   if (peopleCount > 5 && 
      (webControlledLed || webControlledBuzzer || webControlledFan) && 
      (currentMillis - lastWebControlTime >= webControlTimeout)) {
    webControlledLed = false;
    webControlledBuzzer = false;
    webControlledFan = false;
    Serial.println("Automatic mode activated - overriding web controls");
  }
  
  // Print system status regularly
  if (currentMillis - lastStatusPrintTime >= statusPrintInterval) {
    lastStatusPrintTime = currentMillis;
    printSystemStatus();
  }
  
  updateOutputs();
  
  delay(50); // Maintain original timing
}

void handlePeopleCounting() {
  // Read sensors first
  bool irEntryDetected = digitalRead(irEntryPin) == LOW;  // Active LOW
  bool irExitDetected = digitalRead(irExitPin) == LOW;    // Changed to active LOW to match entry
  
  long distEntry = readUltrasonicDistance(trigEntryPin, echoEntryPin);
  long distExit = readUltrasonicDistance(trigExitPin, echoExitPin);

  // Debug output
  // Serial.printf("Entry: IR=%d Dist=%dcm | Exit: IR=%d Dist=%dcm\n",
  //              irEntryDetected, distEntry, irExitDetected, distExit);

  // Entry detection
  if (irEntryDetected && distEntry <= 5 && !entryDetected) {
    peopleCount++;
    entryDetected = true;
    Serial.printf("Person entered - Count: %d\n", peopleCount);
    delay(300); // Reduced delay
  }
  
  // Entry clear
  if ((!irEntryDetected || distEntry > 5) && entryDetected) {
    entryDetected = false;
  }

  // Exit detection (changed to match entry logic)
  if (irExitDetected && distExit <= 5 && !exitDetected) {
    if (peopleCount > 0) {
      peopleCount--;
      exitDetected = true;
      Serial.printf("Person exited - Count: %d\n", peopleCount);
      delay(300); // Reduced delay
    }
  }
  
  // Exit clear
  if ((!irExitDetected || distExit > 5) && exitDetected) {
    exitDetected = false;
  }
}


void printSystemStatus() {
  Serial.println("\n=== System Status ===");
  Serial.printf("People Count: %d\n", peopleCount);
  Serial.printf("WiFi Status: %s\n", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
  }
  // Serial.printf("Web Control: LED=%s BUZZER=%s FAN=%s\n",
  //              webControlledLed ? "ON" : "OFF",
  //              webControlledBuzzer ? "ON" : "OFF",
  //              webControlledFan ? "ON" : "OFF");
  Serial.println("====================");
}

void publishToThingSpeak(int currentCount, float avgCount) {
  HTTPClient http;
  String url = "http://api.thingspeak.com/update?api_key=";
  url += write_api_key;
  url += "&field2=" + String(currentCount);
  url += "&field3=" + String(avgCount, 2);
  
  http.begin(url);
  http.setTimeout(5000);
  
  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.printf("Published to ThingSpeak - Current: %d, Avg: %.2f\n", currentCount, avgCount);
  } else {
    Serial.println("ThingSpeak publish failed (non-critical)");
  }
  http.end();
}

// void updateOutputs() {
//   // Automatic control takes precedence when people count > 5
//   if (peopleCount > 5) {
//     digitalWrite(ledPin, HIGH);
//     digitalWrite(buzzerPin, LOW);
//     digitalWrite(relayFanPin, HIGH);
//     fanServo.write
//     webControlledLed = false;
//     webControlledBuzzer = false;
//     webControlledFan = false;
//   } 
//   else {
//     // Only apply web control if active
//     digitalWrite(ledPin, webControlledLed ? HIGH : LOW);
//     digitalWrite(buzzerPin, webControlledBuzzer ? LOW : HIGH); // Active LOW
//     digitalWrite(relayFanPin, webControlledFan ? HIGH : LOW);
//     fanServo.write(webControlledFan ? SERVO_RUN : SERVO_STOP);
//   }
// }

// void updateOutputs() {
//   if (peopleCount > 5) {
//     digitalWrite(ledPin, HIGH);
//     digitalWrite(buzzerPin, LOW);
    
//     // Fan logic from the working code
//     digitalWrite(relayFanPin, HIGH);
//     fanServo.write(0);
//     delay(500);
//     fanServo.write(180);
//     delay(500);

//     // Disable web control override
//     webControlledLed = false;
//     webControlledBuzzer = false;
//     webControlledFan = false;
//   } 
//   else {
//     digitalWrite(ledPin, webControlledLed ? HIGH : LOW);
//     digitalWrite(buzzerPin, webControlledBuzzer ? LOW : HIGH); // Active LOW
    
//     // Fan logic when not active
//     digitalWrite(relayFanPin, LOW);
//     fanServo.write(0);
//   }
// }

// 


// void updateOutputs() {
//   static unsigned long lastFanUpdate = 0;
//   static int fanPosition = SERVO_STOP;
  
//   if (peopleCount > 5) {
//     // Automatic mode
//     digitalWrite(ledPin, HIGH);
//     digitalWrite(buzzerPin, LOW);
//     digitalWrite(relayFanPin, HIGH);
    
//     // Non-blocking servo control
//     if (millis() - lastFanUpdate >= 500) {
//       lastFanUpdate = millis();
//       fanPosition = (fanPosition == SERVO_STOP) ? SERVO_RUN : SERVO_STOP;
//       fanServo.write(fanPosition);
//     }
    
//     // Disable web overrides
//     webControlledLed = false;
//     webControlledBuzzer = false;
//     webControlledFan = false;
//   } 
//   else {
//     // Web-controlled outputs
//     digitalWrite(ledPin, webControlledLed ? HIGH : LOW);
//     digitalWrite(buzzerPin, webControlledBuzzer ? LOW : HIGH);
    
//     // Immediate fan control without delays
//     digitalWrite(relayFanPin, webControlledFan ? HIGH : LOW);
//     fanServo.write(webControlledFan ? SERVO_RUN : SERVO_STOP);
//   }
// }


// void updateOutputs() {
//   static unsigned long lastFanUpdate = 0;
//   static int fanPosition = SERVO_STOP;
//   static bool isFanMoving = false;
  
//   if (peopleCount > 5) {
//     // Automatic mode - people count > 5
//     digitalWrite(ledPin, HIGH);
//     digitalWrite(buzzerPin, LOW);
//     digitalWrite(relayFanPin, HIGH);
    
//     // Non-blocking servo oscillation
//     if (millis() - lastFanUpdate >= 500) {
//       lastFanUpdate = millis();
//       fanPosition = (fanPosition == SERVO_STOP) ? SERVO_RUN : SERVO_STOP;
//       fanServo.write(fanPosition);
//       isFanMoving = true;
//     }
    
//     // Disable web overrides
//     webControlledLed = false;
//     webControlledBuzzer = false;
//     webControlledFan = false;
//   } 
//   else {
//     // Web-controlled mode - people count <= 5
//     digitalWrite(ledPin, webControlledLed ? HIGH : LOW);
//     digitalWrite(buzzerPin, webControlledBuzzer ? LOW : HIGH);
//     digitalWrite(relayFanPin, webControlledFan ? HIGH : LOW);
    
//     if (webControlledFan) {
//       // Continuous rotation when fan is ON
//       if (!isFanMoving) {
//         // Start movement if not already moving
//         fanPosition = SERVO_STOP;
//         fanServo.write(fanPosition);
//         lastFanUpdate = millis();
//         isFanMoving = true;
//       }
      
//       // Continue oscillating
//       if (millis() - lastFanUpdate >= 500 && isFanMoving) {
//         lastFanUpdate = millis();
//         fanPosition = (fanPosition == SERVO_STOP) ? SERVO_RUN : SERVO_STOP;
//         fanServo.write(fanPosition);
//       }
//     } else {
//       // Immediate stop when fan is OFF
//       if (isFanMoving) {
//         fanServo.write(SERVO_STOP);
//         isFanMoving = false;
//       }
//     }
//   }
// }


void updateOutputs() {
  static unsigned long lastFanUpdate = 0;
  static int fanPosition = SERVO_STOP;
  static bool isFanMoving = false;
  
  if (peopleCount > 5) {
     digitalWrite(ledPin, HIGH);
    digitalWrite(buzzerPin, LOW);
    digitalWrite(relayFanPin, HIGH);
    
    // Non-blocking servo oscillation
    if (millis() - lastFanUpdate >= 500) {
      lastFanUpdate = millis();
      fanPosition = (fanPosition == SERVO_STOP) ? SERVO_RUN : SERVO_STOP;
      fanServo.write(fanPosition);
      isFanMoving = true;
    }
    
    // Disable web overrides
    webControlledLed = false;
    webControlledBuzzer = false;
    webControlledFan = false;
    
  } 
  else {
    // Web-controlled mode - people count <= 5
    digitalWrite(ledPin, webControlledLed ? HIGH : LOW);
    digitalWrite(buzzerPin, webControlledBuzzer ? LOW : HIGH); // Active LOW
    digitalWrite(relayFanPin, webControlledFan ? HIGH : LOW);

    // Immediate fan stop handling
    if (!webControlledFan) {
      if (isFanMoving) {
        fanServo.write(SERVO_STOP);
        isFanMoving = false;
        Serial.println("Fan immediately stopped");
      }
      return; // Skip fan movement logic when OFF
    }

    // Fan movement logic only when ON
    if (!isFanMoving) {
      fanPosition = SERVO_STOP;
      fanServo.write(fanPosition);
      lastFanUpdate = millis();
      isFanMoving = true;
    }
    
    if (millis() - lastFanUpdate >= 500) {
      lastFanUpdate = millis();
      fanPosition = (fanPosition == SERVO_STOP) ? SERVO_RUN : SERVO_STOP;
      fanServo.write(fanPosition);
    }
  }
}






long readUltrasonicDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  return pulseIn(echoPin, HIGH) * 0.034 / 2;
}

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Already connected to WiFi");
    return;
  }
  
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected successfully!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
      lastReadTime = 0;
  } else {
    Serial.println("\nWiFi connection failed");
  }
    
}

// bool readThingSpeakField() {
//   HTTPClient http;
//   String url = "http://api.thingspeak.com/channels/";
//   url += String(channel_id);
//   url += "/fields/";
//   url += String(field_number);
//   url += "/last.json?api_key=";
//   url += read_api_key;
  
//   http.begin(url);
//   http.setTimeout(5000);
  
//   int httpCode = http.GET();
//   if (httpCode == HTTP_CODE_OK) {
//     String payload = http.getString();
//     DynamicJsonDocument doc(256);
//     DeserializationError error = deserializeJson(doc, payload);
    
//     if (!error) {
//       String fieldValue = doc["field" + String(field_number)];
//       controlDevices(fieldValue);
//       http.end();
//       return true;
//     }
//   }
//   http.end();
//   return false;
// }




// bool readThingSpeakField() {
//   HTTPClient http;
//   String url = "http://api.thingspeak.com/channels/";
//   url += String(channel_id);
//   url += "/fields/";
//   url += String(field_number);
//   url += "/last.json?api_key=";
//   url += read_api_key;
  
//   http.begin(url);
//   http.setTimeout(5000);
  
//   int httpCode = http.GET();
//   if (httpCode == HTTP_CODE_OK) {
//     String payload = http.getString();
//     DynamicJsonDocument doc(256);
//     DeserializationError error = deserializeJson(doc, payload);
    
//     if (!error) {
//       String fieldValue = doc["field" + String(field_number)];
//       // Immediate control update
//       if (fieldValue == "0" || fieldValue == "000") {
//         webControlledLed = false;
//         webControlledBuzzer = false;
//         webControlledFan = false;
//       } else {
//         controlDevices(fieldValue);
//       }
//       http.end();
//       return true;
//     }
//   }
//   http.end();
//   return false;
// }

bool readThingSpeakField() {
  HTTPClient http;
  String url = "http://api.thingspeak.com/channels/" + String(channel_id) + 
              "/fields/" + String(field_number) + 
              "/last.json?api_key=" + read_api_key + 
              "&results=1"; // Get only the latest result
  
  http.begin(url);
  http.setTimeout(3000); // Reduce timeout for faster failure
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(256);
    
    if (!deserializeJson(doc, payload)) {
      if (doc.containsKey("field1")) {
        String fieldValue = doc["field1"].as<String>();
        controlDevices(fieldValue);
        
        // Force immediate output update
        updateOutputs();
        return true;
      }
    }
  }
  http.end();
  return false;
}



void controlDevices(String fieldValue) {
  int value = fieldValue.toInt();
  bool ledState = (value & 0b100);
  bool buzzerState = (value & 0b010);
  bool fanState = (value & 0b001);

  webControlledLed = ledState;
  webControlledBuzzer = buzzerState;
  webControlledFan = fanState;
  lastWebControlTime = millis();
  
  Serial.printf("Web Control Updated - LED: %s, Buzzer: %s, Fan: %s\n",
               ledState ? "ON" : "OFF",
               buzzerState ? "ON" : "OFF",
               fanState ? "ON" : "OFF");
}