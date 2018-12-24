#include <Servo.h>
// Temperature and humidity sensor library
#include "DHT.h"

// Pin definitions
const int DHTPIN = 2;       // DHT sensor data pin
const int COOLFAN =  4;     // [PWM] PWM fan control output pin for cool air fan
const int HOTFAN = 7;       // [PWM] PWM fan control output pin for hot air fan
const int MIST = 8;    // 12v atomister control output pin
const int PELTIER = 12; // 12v Peltier Plate relay output pin

#define DHTTYPE DHT11   // [DHT11, DHT22, DHT21]
DHT dht(DHTPIN, DHTTYPE);
Servo baffleServo;

// Environment value limits
const float humidityLower = 50;
const float humidityUpper = 90;
const float tempLower = 22;
const float tempUpper = 26;

int isHumidityAboveLimit = 0;
int isHumidityBelowLimit = 0;
int isTempAboveLimit = 0;
int isTempBelowLimit = 0;

int isMistOn = 0;
int isCoolFanOn = 0;
int isHotFanOn = 0;
int isPeltierOn = 0;
int isBaffleOpen = 0;
int airState = 0 ; // 0 - fans off, 1 - ventilating (no AC), 2 - cooling, 3 - heating

long mistOnTime = 0;
int readingsTaken = 0;

class DHTReading {
  private:
    float _humidity, _temperature;
  public:
    DHTReading() {
      _humidity = dht.readHumidity();
      _temperature = dht.readTemperature();
      if (isValid()) readingsTaken++;
    }
    float getTemp()
    {
      return _temperature;
    }
    float getHum()
    {
      return _humidity;
    }
    bool isValid() {
      return (!isnan(_humidity) && !isnan(_temperature));
    }
    void printToSerial() {
      Serial.print("Humidity: ");
      Serial.print(getHum());
      Serial.print(" %\t");
      Serial.print("Temperature: ");
      Serial.print(getTemp());
      Serial.println(" *C ");
    }
};

void setup() {
  Serial.begin(9600);
  delay(50);
  Serial.print("Setting up...");

  pinMode(COOLFAN, OUTPUT);
  pinMode(HOTFAN, OUTPUT);
  pinMode(MIST, OUTPUT);
  pinMode(PELTIER, OUTPUT);

  //  baffleServo.attach(9);
  dht.begin();
  Serial.println("complete.");
  delay(100);
}

void loop() {
  DHTReading reading;
  float humidity = reading.getHum();
  float temperature = reading.getTemp();

  // Check if any reads failed and exit early (to try again).
  if (!reading.isValid()) {
    Serial.println("Failed to read from DHT sensor!");
    Serial.println("Waiting to try again...");
    delay(5000);
    Serial.println("Re-reading...");
    return;
  }

  reading.printToSerial();
  Serial.println(readingsTaken);
  humidityCheck(humidity);
  tempCheck(temperature);

  determineAirState();
  setACDeviceState();

  // Delay between measurements
  delayMinutes(.2);
}

void delayMinutes(float minutes) {
  float delaymillis = minutes * 60000;
  delay(delaymillis);
}

void humidityCheck(float hum) {
  // Check humidity against limits
  if (hum < humidityLower) {
    isHumidityBelowLimit = 1;
    Serial.print("Humidity below ");
    Serial.print(humidityLower);
    Serial.println("%");
  } else if (hum > humidityUpper) {
    isHumidityAboveLimit = 1;
    Serial.print("Humidity above ");
    Serial.print(humidityUpper);
    Serial.println("%");
  } else {
    isHumidityBelowLimit = 0;
    isHumidityAboveLimit = 0;
  }
}

void tempCheck(float temp) {
  // Check temperature against limits
  if (temp < tempLower) {
    isTempBelowLimit = 1;
    Serial.print("Temperature below ");
    Serial.print(tempLower);
    Serial.println(" *C");
  } else if (temp > tempUpper) {
    isTempAboveLimit = 1;
    Serial.print("Temperature above ");
    Serial.print(tempUpper);
    Serial.println(" *C");
  } else {
    isTempBelowLimit = 0;
    isTempAboveLimit = 0;
  }
}

void determineAirState() {
  if (isTempBelowLimit) airState = 2;
  if (isTempAboveLimit) airState = 3;
  if (isHumidityAboveLimit) airState = 1;
  Serial.print("AC state: ");
  switch (airState) {
    case 0:
      Serial.println("All off");
      return;
    case 1:
      Serial.println("Ventilation on");
      return;
    case 2:
      Serial.println("Cooling on");
      return;
    case 3:
      Serial.println("Heating on");
      return;
    default:
      Serial.println("Problem determining AC state!");
      return;
  }
}

void setACDeviceState() {
  switch (airState) {
    // All off
    case 0:
      peltierOff();
      coolFanOff();
      hotFanOff();
      break;
    // No AC, fans on
    case 1:
      peltierOff();
      coolFanOn();
      hotFanOn();
      baffleClose();
      break;
    // Air cooling, heat vented
    case 2:
      peltierOn();
      coolFanOn();
      hotFanOn();
      baffleOpen();
      break;
    // Air heating
    case 3:
      peltierOn();
      coolFanOff();
      hotFanOn();
      baffleClose();
      break;
    default:
      break;
  }
}

// Atomister methods
void mistToggle() {
  Serial.print("Toggling mist: ");
  isMistOn ? mistOff() : mistOn();
  return;
}

void mistOn() {
  Serial.println("Mist on.");
  digitalWrite(MIST, HIGH);
  return;
}

void mistOff() {
  Serial.println("Mist off.");
  digitalWrite(MIST, LOW);
  return;
}

// Fan methods
void coolFanToggle() {
  isCoolFanOn ? coolFanOff() : coolFanOn();
  return;
}

void coolFanOn() {
  digitalWrite(COOLFAN, HIGH);
  return;
}

void coolFanOff() {
  digitalWrite(COOLFAN, LOW);
  return;
}

void hotFanToggle() {
  isHotFanOn ? hotFanOff() : hotFanOn();
  return;
}

void hotFanOn() {
  digitalWrite(HOTFAN, HIGH);
  return;
}

void hotFanOff() {
  digitalWrite(HOTFAN, LOW);
  return;
}

// Peltier plate methods
void peltierToggle() {
  isPeltierOn ? peltierOff() : peltierOn();
  return;
}

void peltierOn() {
  digitalWrite(PELTIER, HIGH);
  return;
}

void peltierOff() {
  digitalWrite(PELTIER, LOW);
  return;
}

// Baffle methods

void baffleOpen() {
  if (!isBaffleOpen) {
    Serial.println("Opening baffle!");
    isBaffleOpen = 1;
  }
  return;
}

void baffleClose() {
  if (isBaffleOpen) {
    Serial.println("Closing baffle!");
    isBaffleOpen = 0;
  }
  return;
}
