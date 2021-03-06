// Servo library
#include <Servo.h>
// Temperature and humidity sensor library
#include "DHT.h"

#define DHTTYPE DHT11   // [DHT11, DHT22, DHT21]

// Pin definitions
const int DHTPIN = 2;       // DHT sensor data pin
const int COOLFAN =  4;     // [PWM] PWM fan control output pin for cool air fan
const int HOTFAN = 8;       // [PWM] PWM fan control output pin for hot air fan
const int MIST = 7;    // 12v atomister control output pin
const int MISTSWITCH = 13; // Atomister on/off switch pin
const int PELTIER = 12; // 12v Peltier Plate relay output pin

DHT dht(DHTPIN, DHTTYPE);
Servo baffleServo;

// Environment value limits
const float optimalTemperature[] = {23, 25};
const float optimalHumidity[] = {90, 99};
const float warningTemperature[] = {20, 27};
const float warningHumidity[] = {55, 85};
const float dangerTemperature[] = {20, 25};
const float dangerHumidity[] = {50, 95};

// App variables
int isBaffleOpen = 0;
long mistOnTime = 0;
int readingsTaken = 0;

// Class definitions
class Device {
  protected:
    String _name;
    bool _isOn;
  public:
    Device() {
      _isOn = false;
    }
    Device(String name): Device() {
      _name = name;
    }
    bool isOn() {
      return _isOn;
    }

    virtual void setupDevice() {}
    virtual void on() {}
    virtual void off() {}

    String statusToString() {
      String s;
      s += _name;
      _isOn ? s += " ON." : s += " OFF.";
      return s;
    }
};

class Relay : public Device {
  private:
    int _relaypin;
  public:
    Relay(String name, int pin): Device(name) {
      _relaypin = pin;
    }
    void on() {
      if (!_isOn) {
        digitalWrite(_relaypin, HIGH);
        _isOn = true;
      }
    }
    void off() {
      digitalWrite(_relaypin, LOW);
      _isOn = false;
    }
    void setupDevice() {
      pinMode(_relaypin, OUTPUT);
    }
};

class MistDevice : public Device {
  private:
    int _relaypin, _switchpin;
  public:
    MistDevice(String name, int pin, int sp): Device(name) {
      _relaypin = pin;
      _switchpin = sp;
    }
    void on() {
      if (!_isOn) {
        digitalWrite(_relaypin, HIGH);
        digitalWrite(_switchpin, LOW);
        delay(500);
        digitalWrite(_switchpin, HIGH);
        _isOn = true;
      }
    }
    void off() {
      digitalWrite(_relaypin, LOW);
      digitalWrite(_switchpin, LOW);
      delay(500);
      digitalWrite(_switchpin, HIGH);
      _isOn = false;
    }
    void setupDevice() override {
      pinMode(_relaypin, OUTPUT);
      pinMode(_switchpin, OUTPUT);
      digitalWrite(_switchpin, HIGH);
    }
};

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
    Band() {
      _lower = 0;
      _upper = 0;
    };
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
    bool below(float num) {
      return num < _lower;
    }
    bool above(float num) {
      return num > _upper;
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

class Environment {
  private:
    Band _tempOptimal, _humOptimal;
    Band _tempWarn, _humWarn;
    Band _tempDanger, _humDanger;
  public:
    Environment() {};
    Environment(Band to = {0, 0}, Band ho = {0, 0}, Band tw = {0, 0}, Band hw = {0, 0}, Band td = {0, 0}, Band hd = {0, 0}) {
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
    Band getHumOptimal() {
      return _humOptimal;
    }
    String toString() {
      String s;
      s += "Environment parameters:";
      s += "\nOptimal temperature range: ";
      s += _tempOptimal.toString();
      s += "\nOptimal humidity range: ";
      s += _humOptimal.toString();
      if (_tempWarn.getUpper() - _tempWarn.getLower() != 0) {
        s += "\nWarning temperature range: ";
        s += _tempWarn.toString();
      }
      if (_humWarn.getUpper() - _humWarn.getLower() != 0) {
        s += "\nWarning humidity range: ";
        s += _humWarn.toString();
      }
      if (_tempDanger.getUpper() - _tempDanger.getLower() != 0) {
        s += "\nDanger temperature range: ";
        s += _tempDanger.toString();
      }
      if (_humDanger.getUpper() - _humDanger.getLower() != 0) {
        s += "\nDanger humidity range: ";
        s += _humDanger.toString();
      }
      return s;
    }
};

class EnvironmentMonitor {
  private:
    Environment *_env;
    bool _needHeating, _needCooling, _needHumidity;
  public:
    EnvironmentMonitor(Environment *env) {
      _env = env;
      _needHeating, _needCooling, _needHumidity = 0;
    }
    void determineNeeds(DHTReading *reading) {
      float temp = reading->getTemp();
      float hum = reading->getHum();
      _needHeating = _env->getTempOptimal().below(temp);
      _needCooling = _env->getTempOptimal().above(temp);
      _needHumidity = _env->getHumOptimal().below(hum);
    }
    bool needHeating() {
      return _needHeating;
    }
    bool needCooling() {
      return _needCooling;
    }
    int needHumidity() {
      return _needHumidity;
    }
    String toString() {
      String s;
      s += "Monitoring environment...";
      return s;
    }
};

class EnvironmentController {
  private:
    EnvironmentMonitor *_em;
    Device *_coolfan, *_mist, *_peltier;
    bool _heating, _cooling, _misting;
  public:
    EnvironmentController() {}
    EnvironmentController(EnvironmentMonitor *em, Device *coolfan, Device *mist, Device *peltier) {
      _em = em;
      _coolfan = coolfan;
      _mist = mist;
      _peltier = peltier;
      _heating, _cooling, _misting = 0;
    }
    void determineActions() {
      if (_em->needCooling()) {
        startCooling();
      } else if (_em->needHeating()) {
        startHeating();
      } else {
        stopHeating();
        stopCooling();
        startVentilation();
      }
      if (_em->needHumidity()) {
        startMist();
      } else {
        stopMist();
        startVentilation();
      }
    }
    void startCooling() {
      if (!_cooling) {
        if (_heating) stopHeating();
        _peltier->on();
        _coolfan->on();
        _cooling = 1;
      }
    }
    void stopCooling() {
      if (_cooling) {
        _peltier->off();
        _coolfan->off();
        _cooling = 0;
      }
    }
    void startHeating() {
      if (!_heating) {
        if (_cooling) stopCooling();
        _peltier->on();
        // Baffle close
        _heating = 1;
      }
    }
    void stopHeating() {
      if (_heating) {
        _peltier->off();
        // Baffle open
        _heating = 0;
      }
    }
    void startMist() {
      if (!_misting) {
        _mist->on();
        _misting = 1;
      }
    }
    void stopMist() {
      if (_misting) {
        _mist->off();
        _misting = 0;
      }
    }
    void startVentilation() {
      if (!_heating || !_cooling) _coolfan->on();
    }
    void stopVentilation() {
      if (!_heating || !_cooling) _coolfan->off();
    }
    void setup() {
      _coolfan->setupDevice();
      _mist->setupDevice();
      _peltier->setupDevice();
    }
    String setupToString() {
      String s;
      s += "Controlling environment...";
      return s;
    }
    String statusToString() {
      String s;
      s += "Environment is being";
      if (_heating) s += " heated.";
      if (_cooling) s += " cooled.";
      if (_misting) s += " moisturized.";
      if (!_heating || !_cooling || !_misting) s += " ventilated.";
      return s;
    }
};

// Helper methods
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

Relay coolfan("cool fan", COOLFAN);
MistDevice mist("mist", MIST, MISTSWITCH);
Relay peltier("peltier", PELTIER);

Band tempOpt(optimalTemperature);
Band humOpt(optimalHumidity);
Band tempWarn(warningTemperature);
Band humWarn(warningHumidity);
Environment env(tempOpt, humOpt, tempWarn, humWarn);
EnvironmentMonitor em(&env);
EnvironmentController ec(&em, &coolfan, &mist, &peltier);

void setup() {
  Serial.begin(9600);
  delay(50);
  Serial.println("Setting up...");

  ec.setup();

  Serial.println(env.toString());
  Serial.println(em.toString());
  Serial.println(ec.setupToString());

  //  baffleServo.attach(9);
  dht.begin();

  Serial.println("\nSetup complete.");
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

  // Send reading to environment monitor and tell
  // environment controller to figure out what to do
  em.determineNeeds(&reading);
  ec.determineActions();

  Serial.print("\n\nReading #");
  Serial.println(readingsTaken);
  Serial.println(reading.toString());

  Serial.println(coolfan.statusToString());
  Serial.println(mist.statusToString());
  Serial.println(peltier.statusToString());

  Serial.print("Baffle open: ");
  Serial.println(isBaffleOpen);

  Serial.println(ec.statusToString());
  // Delay between measurements
  delayMinutes(.4);
}
