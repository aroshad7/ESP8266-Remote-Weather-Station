#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <SPI.h>
#include <SD.h>

#define BUZZER 5
#define DHTPIN 4
#define DHTTYPE DHT11

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);
File myFile;
char msg[50];

const char* ssid = "AndroidAP";           //wifi ssid
const char* password = "12345678";      //wifi password
const char* mqtt_server = "m12.cloudmqtt.com";
const char* systemData = "systemData";
const char* temperature = "Temperature";
const char* humidity = "Humidity";
const char* heatIndex = "HeatIndex";
float temp = 0.0;
float humd = 0.0;
float hInd = 0.0;
bool dataReady = false;
bool cardOK = true;
int count = 0;
uint8_t backupCount = 0;
int ID = 0;
long sTime = millis();
long eTime = millis();

void setup() {

  Serial.begin(9600);
  pinMode(BUZZER, OUTPUT);
  setup_wifi();
  dht.begin();
  client.setServer(mqtt_server, 17360);
  client.setCallback(callback);

  Serial.print("Initializing SD card...");
  if (!SD.begin(15)) {
    Serial.println("initialization failed!");
    cardOK = false;
    analogWrite(BUZZER, 50);
    return;
  }
  Serial.println("initialization Complete!");
  reconnect();
}

void loop() {
  if (!client.connected()) {
    smartDelay(0, false);
    reconnect();
  }

  prepareData();

  if (dataReady) {
    publishData(temp, humd, hInd, ID);
    dataReady = false;
    temp = 0.0;
    humd = 0.0;
    hInd = 0.0;
  }

  if (backupCount > 0) {
    restoreBackup();
  }
  client.loop();
}

void setup_wifi() {

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    for (int i = 0; i < 500; i++) {
      delay(1);
    }
    Serial.print(".");
  }

  delay(1500);
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client", "aroshad7", "mqtt account password")) {
      Serial.println("connected");
      client.publish(systemData, "System Booting Complete!");
      client.subscribe(systemData);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      smartDelay(5000, true);
    }
  }
}

void publishData(float tempFloat, float humdFloat, float hIndFloat, int ID) {
  String tempString = String(ID, DEC) + ": " + String(tempFloat, 3) + "C";
  String humdString = String(ID, DEC) + ": " + String(humdFloat, 3) + "%";
  String hIndString = String(ID, DEC) + ": " + String(hIndFloat, 3) + "C";

  char tempChar[sizeof(tempString)];
  tempString.toCharArray(tempChar, sizeof(tempChar));
  char humdChar[sizeof(humdString)];
  humdString.toCharArray(humdChar, sizeof(humdChar));
  char hIndChar[sizeof(hIndString)];
  hIndString.toCharArray(hIndChar, sizeof(hIndChar));

  if (client.connected()) {
    if (client.publish(temperature, tempChar)) {
    }
    else {
      Serial.println("T failed!");
    }
    if (client.publish(humidity, humdChar)) {
    }
    else {
      Serial.println("H failed!");
    }
    if (client.publish(heatIndex, hIndChar)) {
    }
    else {
      Serial.println("HI failed!");
    }
  }
  else {
    Serial.println("Connection Failed!");
    reconnect();
  }
}

void writeToSDLog() {
  myFile = SD.open("DataLog.txt", FILE_WRITE);
  if (myFile) {
    myFile.println("Temperature: " + String(temp, 3) + "*C    Humidity: " + String(humd, 3) + "%    Heat Index: " + String(hInd, 3) + "*C" + String(ID, DEC) + "#");
    myFile.close();
    Serial.println("\nWrote to SD Card Log.");
    backupCount++;
  } else {
    Serial.println("error opening DataLog.txt");
  }
}

void prepareData() {
  eTime = millis();
  count++;
  temp += dht.readTemperature();
  humd += dht.readHumidity();
  //hInd += dht.computeHeatIndex(temp, humd, false);

  if ((eTime - sTime) > 5000) {
    temp = temp / count;
    humd = humd / count;
    hInd = dht.computeHeatIndex(temp, humd, false);
    count = 0;
    ID++;
    dataReady = true;
    sTime = millis();
  }

}

void smartDelay(int delayPeriod, bool shouldDelay) {
  long sTime2 = millis();
  long eTime2 = millis();
  while (!((eTime2 - sTime2) > delayPeriod)) {
    eTime2 = millis();
    prepareData();
    if (dataReady) {
      if (cardOK) {
        writeToSDLog();
      }
      dataReady = false;
      temp = 0.0;
      humd = 0.0;
      hInd = 0.0;
    }
    if (!shouldDelay) {
      break;
    }
  }
}

void restoreBackup() {
  myFile = SD.open("DataLog.txt");
  uint8_t i = 0;
  while (i < backupCount) {
    String line;
    char readChar = 3;
    while (1) {
      readChar = myFile.read();
      if (readChar == 13) {
        if (myFile.read() == 10) {
          break;
        }
      }
      line += readChar;
    }
    int tempIndex = line.indexOf("Temp");
    String IDString = line.substring(tempIndex + 66, line.indexOf("#"));
    String tempString = line.substring(tempIndex + 13, tempIndex + 19);
    String humdString = line.substring(tempIndex + 35, tempIndex + 41);
    String hIndString = line.substring(tempIndex + 58, tempIndex + 64);

    Serial.println(tempString + " " + humdString + " " + hIndString + " publishing backup data with ID = " + IDString);
    publishData(tempString.toFloat(), humdString.toFloat(), hIndString.toFloat(), IDString.toInt());
    i++;
  }
  backupCount = 0;
  myFile.close();
  SD.remove("DataLog.txt");
  Serial.println("Restoration complete!");
}

void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}

