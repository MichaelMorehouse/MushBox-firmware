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
const float optimalTemperature[] = {22, 25};
const float optimalHumidity[] = {65, 80};
const float warningTemperature[] = {20, 27};
const float warningHumidity[] = {55, 85};
const float dangerTemperature[] = {20, 25};
const float dangerHumidity[] = {50, 95};

// App variables
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
    String toString() {
      String s;
      s += "Humidity: ";
      s += getHum();
      s += "% ";
      s += "Temperature: ";
      s += getTemp();
      s += " *C";
      return s;
    }
};

class Band {
  private:
    float _lower, _upper;
  public:
    Band(){};
    Band(float lower, float upper) {
      _lower = lower;
      _upper = upper;
    }
    Band(float* arr) {
      _lower = arr[0];
      _upper = arr[1];
    }
    float getLower() {
      return _lower;
    }
    float getUpper() {
      return _upper;
    }
    bool belowLower(float num) {
      return num < _lower;
    }
    bool aboveUpper(float num) {
      return num > _upper;
    }
    int stateCheck(float num) {
      int state;
      belowLower(num) ? state = 0 : aboveUpper(num) ? state = 2 : state = 1;
      return state;
    }
    virtual String toString() {
      String s;
      s += "lower ";
      s += _lower;
      s += ", upper ";
      s += _upper;
      return s;
    }
};

class EnvironmentMonitor {
  private:
    Band _tempOptimal, _humOptimal;
    Band _tempWarn, _humWarn;
    Band _tempDanger, _humDanger;
    int _tempState, _humState;
  public:
    EnvironmentMonitor(Band to, Band ho) {
      _tempOptimal = to;
      _humOptimal = ho;
      _tempWarn = to;
      _humWarn = ho;
      _tempDanger = to;
      _humDanger = ho;
    }
    EnvironmentMonitor(Band to, Band ho, Band tw, Band hw, Band td, Band hd) {
      _tempOptimal = to;
      _humOptimal = ho;
      _tempWarn = tw;
      _humWarn = hw;
      _tempDanger = td;
      _humDanger = hd;
    }
    Band getTempOptimal() {
      return _tempOptimal;
    }
    int getTempState() {
      return _tempState;
    }
    int getHumState() {
      return _humState;
    }
    void setHTState(DHTReading dhtreading) {
      setTempState(dhtreading.getTemp());
      setHumState(dhtreading.getHum());
    }
    void setTempState(float temp) {
      _tempState = _tempOptimal.stateCheck(temp);
    }
    void setHumState(float hum) {
      _humState = _humOptimal.stateCheck(hum);
    }
    String toString() {
      String s;
      s += "\nEnvironment Monitor status:";
      s += "\nOptimal temperature range: ";
      s += _tempOptimal.toString();
      s += "\nOptimal humidity range: ";
      s += _humOptimal.toString();
      s += "\nEnv states:";
      s += "\nTemperature state ";
      s += _tempState;
      s += "\nHumidity state ";
      s += _humState;
      return s;
    }
};

class Device {
  private:
    String _name;
    int _pin, _pinmode;
    bool _isOn;
  public:
    Device(String name, int pin, int pinmode = 0) {
      _name = name;
      _pin = pin;
      _pinmode = pinmode;
      _isOn = false;
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
    bool isOn() {
      return _isOn;
    }
    void setupDevice() {
      switch (_pinmode) {
        case 0:
          pinMode(_pin, OUTPUT);
          break;
        case 1:
          pinMode(_pin, INPUT);
          break;
        case 2:
          pinMode(_pin, INPUT_PULLUP);
          break;
        default:
          break;
      }
    }
    void on() {
      if(!_isOn) {
        digitalWrite(_pin, HIGH);
        _isOn = true;
      }
    }
    void off() {
      if(_isOn) {
        digitalWrite(_pin, LOW);
        _isOn = false;
      }
    }
    String statusToString() {
      String s;
      s += _name;
      _isOn ? s += " ON." : s += " OFF.";
      return s;
    }
    String setupToString() {
      String s;
      s += _name;
      s += " set up at pin ";
      s += _pin;
      s += " with pinmode ";
      s += _pinmode;
      return s;
    }
};

Band tempOpt(optimalTemperature);
Band humOpt(optimalHumidity);
EnvironmentMonitor em(tempOpt, humOpt);

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

// Baffle methods
void baffleOpen() {
  if (!isBaffleOpen) {
    Serial.println("Opening baffle!");
    isBaffleOpen = 1;
  }
}

void baffleClose() {
  if (isBaffleOpen) {
    Serial.println("Closing baffle!");
    isBaffleOpen = 0;
  }
}

void setup() {
  Serial.begin(9600);
  delay(50);
  Serial.println("Setting up...");

  for(int i = 0; i < deviceNumber; i++) {
    deviceList[i].setupDevice();
    Serial.println(deviceList[i].setupToString());
  };
  
  //  baffleServo.attach(9);
  dht.begin();

  Serial.println("Setup complete.");
  delay(100);
}

void loop() {
  // Take a new DHT reading
  DHTReading reading;
  
  // Check if reading failed and exit early to retry
  // Delayed to avoid sensor damage
  if (!reading.isValid()) {
    Serial.println("Failed to read from DHT sensor!");
    Serial.println("Waiting to try again...");
    delay(5000);
    Serial.println("Re-reading...");
    return;
  } else {
    readingsTaken++;
  }
  
  Serial.print("\n\nReading #");
  Serial.println(readingsTaken);
  Serial.println(reading.toString());

  Serial.println(coolfan.statusToString());
  Serial.println(hotfan.statusToString());
  Serial.println(mist.statusToString());
  Serial.println(peltier.statusToString());

  Serial.print("Baffle open: ");
  Serial.println(isBaffleOpen);

  em.setHTState(reading);

  Serial.print(em.toString());

  // Delay between measurements
  delayMinutes(.2);
}
