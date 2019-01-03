const byte relaypin = 2;
const byte pwmpin = 3;
const byte sensepin = 5;

class PWMFan {
  private:
    bool _on;
    int _duty;
    int _relaypin, _pwmpin, _sensepin;
  public:
    PWMFan(int rp, int pwm, int sp) {
      _on = false;
      _duty = 0;
      _relaypin = rp;
      _pwmpin = pwm;
      _sensepin = sp;
    }
    void setupDevice() {
      pinMode(_relaypin, OUTPUT);
      pinMode(_pwmpin, OUTPUT);
      pinMode(_sensepin, INPUT_PULLUP);
    }
    void on(int percent = 100) {
      // Safety logic
      if (percent < 0) percent = 0;
      if (percent == 0) {
        off();
        return;
      }
      if (percent > 100) percent = 100;
      
      // Normalize percent to 10-bit PWM value
      int duty = percent * 1024 / 100;
      if (duty != _duty) {
        digitalWrite(_relaypin, HIGH);
        digitalWrite(_pwmpin, duty);
        _duty = duty;
      }
      if (!_on) _on = 1;
    }
    void off() {
      if (_on) {
        digitalWrite(_relaypin, LOW);
        digitalWrite(_pwmpin, LOW);
        _duty = 0;
        _on = 0;
      }
    }
};

PWMFan fan1(relaypin, pwmpin, sensepin);

void setup() {
  Serial.begin(9600);
  fan1.setupDevice();
}

void loop() {
  fan1.on();
  Serial.println("Turning on fan.");
  for (byte i = 0; i < 20; i++) {
    Serial.print(i);
    Serial.print(": ");
    Serial.println(analogRead(sensepin));
    delay(2000);
  }
}
