// Servo library
#include <Servo.h>
// Temperature and humidity sensor library
#include "DHT.h"

#define DHTTYPE DHT11   // [DHT11, DHT22, DHT21]

// Pin definitions
const int DHTPIN = 2;       // DHT sensor data pin
const int COOLFAN =  4;     // [PWM] PWM fan control output pin for cool air fan
const int HOTFAN = 7;       // [PWM] PWM fan control output pin for hot air fan
const int MIST = 8;    // 12v atomister control output pin
const int PELTIER = 12; // 12v Peltier Plate relay output pin

DHT dht(DHTPIN, DHTTYPE);
Servo baffleServo;

// Environment value limits
const float humidityLower = 50;
const float humidityUpper = 90;
const float tempLower = 22;
const float tempUpper = 23;

int isHumidityAboveLimit = 0;
int isHumidityBelowLimit = 0;
int isTempAboveLimit = 0;
int isTempBelowLimit = 0;

int isBaffleOpen = 0;
int airState = 0 ; // 0 - fans off, 1 - ventilating (no AC), 2 - cooling, 3 - heating

long mistOnTime = 0;
int readingsTaken = 0;

class Device {
  private:
    String _name;
    int _pin, _pinmode, _isOn;
  public:
    Device(String name, int pin, int pinmode = 0) {
      _name = name;
      _pin = pin;
      _pinmode = pinmode;
      _isOn = 0;
    }
    String getName() {
      return _name;
    }
    int getPin() {
      return _pin;
    }
    int getPinmode() {
      return _pinmode;
    }
    int isOn() {
      return _isOn;
    }
    void setupDevice() {
      String mode;
      switch (_pinmode) {
        case 0:
          pinMode(_pin, OUTPUT);
          mode = "output";
          break;
        case 1:
          pinMode(_pin, INPUT);
          mode = "input";
          break;
        case 2:
          pinMode(_pin, INPUT_PULLUP);
          mode = "input pullup";
          break;
        default:
          mode = "PINMODE ERROR!";
          break;
      }
      Serial.print("Set up device ");
      Serial.print(_name);
      Serial.print(" at pin ");
      Serial.print(_pin);
      Serial.print(" with pinmode ");
      Serial.println(mode);
    }
    void on() {
      if(!_isOn) {
        digitalWrite(_pin, HIGH);
        _isOn = 1;
      }
    }
    void off() {
      if(_isOn) {
        digitalWrite(_pin, LOW);
        _isOn = 0;
      }
    }
    void printStatusToSerial() {
      String s;
      s += _name;
      _isOn ? s += " is ON. " : s += " is OFF. ";
      Serial.println(s);
    }
    void printSetupToSerial() {
      String s;
      s += "Pin ";
      s += _pin;
      s += " pinmode ";
      s += _pinmode;
      Serial.println(s);
    }
};

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

Device coolfan("cool fan", COOLFAN);
Device hotfan("hot fan", HOTFAN);
Device mist("mist", MIST);
Device peltier("peltier", PELTIER);

Device deviceList[] = {
  coolfan,
  hotfan,
  mist,
  peltier
};
// !Must match number of devices added to device list!
const int deviceNumber = 4;

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

void setup() {
  Serial.begin(9600);
  delay(50);
  Serial.println("Setting up...");

  for(int i = 0; i < deviceNumber; i++) {
    deviceList[i].setupDevice();
  };
  //  baffleServo.attach(9);
  dht.begin();
  Serial.println("Setup complete.");
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
  

  Serial.print("\nReading #");
  Serial.println(readingsTaken);
  reading.printToSerial();
  
  humidityCheck(humidity);
  tempCheck(temperature);

  determineAirState();
  
  // Iterate through devices, change limit to reflect # of devices
  for (int i = 0; i < 4; i++) {
    deviceList[i].printStatusToSerial();
  };

  Serial.print("Baffle open: ");
  Serial.println(isBaffleOpen);  

  // Delay between measurements
  delayMinutes(.2);
}
