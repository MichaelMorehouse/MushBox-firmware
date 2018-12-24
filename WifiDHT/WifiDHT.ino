
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

const int DHTPIN = 5;       // DHT sensor data pin
#define DHTTYPE DHT11  // [DHT11, DHT22, DHT21]

DHT dht(DHTPIN, DHTTYPE);
ESP8266WiFiMulti WiFiMulti;

const char * SSID_NAME = "Millers";
const char * WIFI_PASS = "internet1706";

void setup() {
  Serial.begin(115200);
  delay(10);

  // We start by connecting to a WiFi network
  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(SSID_NAME, WIFI_PASS);

  Serial.println();
  Serial.println();
  Serial.print("Wait for WiFi... ");

  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  delay(500);
}


void loop() {
  delay(100);
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float hum = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float temp = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(hum) || isnan(temp)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Serial.println(encodeURL(temp, hum));
  printTempAndHumidityToSerial(temp, hum);
  
  const char * host = "http://sleepy-savannah-47711.herokuapp.com/log"; // dns

  Serial.print("connecting to ");
  Serial.println(host);

  HTTPClient http;

  
  
  http.begin(host);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST(encodeURL(temp, hum));
  http.writeToStream(&Serial);
  http.end();
  delay(900000);
}

void printTempAndHumidityToSerial(float temp, float hum) {
  Serial.print("Humidity: ");
  Serial.print(hum);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(temp);
  Serial.println(" *C ");
}

String encodeURL(float temp, float hum) {
  String URL;
  URL += "temperature=";
  URL += temp;
  URL += "&";
  URL += "humidity=";
  URL += hum;
  return URL;
}
