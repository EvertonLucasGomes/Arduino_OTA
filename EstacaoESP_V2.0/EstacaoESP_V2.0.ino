#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include "cert.h"

#include <Adafruit_BMP280.h>
#include "Adafruit_Si7021.h"
#include <ThingsBoard.h>

#define SECRET_SSID "ebt-mi8"                               // Dados da Internet
#define SECRET_PASS "ebtwifimi8"

#define Token "rWxfsfvfCiA10Bp8EtU0"                            // dados do thingsboard / token do canal thingsboard
#define THINGSBOARD_SERVER "eltontorres.asuscomm.com"
#define SERIAL_DEBUG_BAUD 115200

int RainSensorPin = 33;                                        // Rain REED-ILS sensor GPIO 21 on ESP32

volatile unsigned long contactTime;                            // tempo de debounce

volatile unsigned long tempRain;                              // quantidade de vezes que a balança do pluviometro girou

#define Bucket_Size_EU 0.2                                   // tamanho do bucket do pluviometro

float rain = 0;                                              // Chuva temporario no período de loop, calculo total feito na plataforma thingsboard

#define uS_TO_S_FACTOR 1000000                               // Fator de conversão de micro segundos para segundos
#define TIME_TO_SLEEP 60                                     // tempo de sleep em segundos

Adafruit_BMP280 bmpSensor;                                   //objetos de comunicação com os sensores
Adafruit_Si7021 siSensor = Adafruit_Si7021();

WiFiClient cliente;                                         // objetos WiFi e Thingsboard
ThingsBoard tb(cliente);

int status = WL_IDLE_STATUS;                               // status conexão WiFi

char ssid[] = SECRET_SSID;                                 // variáveis de conexão ao WiFi
char pass[] = SECRET_PASS;

String FirmwareVer = {                                     // versão do firmware
  "6.0"
};

#define URL_fw_Version "https://raw.githubusercontent.com/EvertonLucasGomes/Arduino_OTA/main/bin_version.txt"       // path do github
#define URL_fw_Bin "https://raw.githubusercontent.com/EvertonLucasGomes/Arduino_OTA/main/fw.bin"

void connect_wifi();
void reconnect();                                         // funções
void forceIncrement();
void getRain();
void firmwareUpdate();
int FirmwareVersionCheck();

void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD);

  Serial.print("Bela Atualização");

  pinMode(RainSensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RainSensorPin), forceIncrement, FALLING);        // caso especial force increment

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);            // tempo deepSleep

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,0);                              // fator do pluviometro

  if(esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0){                // incremento pluviometro
    tempRain++;
  }
  
  connect_wifi();                                            // conexão com wi-fi            
  
  if (WiFi.status() != WL_CONNECTED)                         //verifica conexão Wi-Fi
  { 
    reconnect();
    return;
  }
  
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  if (FirmwareVersionCheck()) {                           // checagem de versão
      firmwareUpdate();
    }

  // Connect to WiFi network

  if (!tb.connected())                                  //verifica conexão thingsboard
  { 
    Serial.print("Conectando a: ");
    Serial.print(THINGSBOARD_SERVER);
    Serial.print("token: ");
    Serial.println(Token);
    if (!tb.connect(THINGSBOARD_SERVER, Token))
    {
      Serial.println("falha ao conectar");
      delay(5000);
      return;
    }
    getRain(); //envio de dados do sensor BMP280             // envio de dados
    Serial.println(rain);
    tb.sendTelemetryFloat("pluviometer", rain);

    if (!bmpSensor.begin())
    {
      Serial.println("Falha ao tentar ler o sensor BPM280");
    }
    else
    {
      tb.sendTelemetryFloat("temperature", bmpSensor.readTemperature());
      tb.sendTelemetryFloat("pressure", bmpSensor.readPressure());
      tb.sendTelemetryFloat("altitude", bmpSensor.readAltitude(1015.25));
    }

    if (!siSensor.begin())
    { //envio de dados do sensor SI7021
      Serial.println("Falha ao tentar ler o sensor SI7021");
    }
    else
    {
      tb.sendTelemetryFloat("humidity", siSensor.readHumidity());
      tb.sendTelemetryFloat("SItemperature", siSensor.readTemperature());
    }
  }

  Serial.flush();
  esp_deep_sleep_start();
}

void loop() {
  
}

void connect_wifi() {
  Serial.println("Waiting for WiFi");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect()
{ //reeconexão em caso de queda
  status = WiFi.status();
  if (status != WL_CONNECTED)
  {
    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      Serial.print(".");
    }
    Serial.println("reconexão bem sucedida");
  }
}

void forceIncrement()
{

  if ((millis() - contactTime) > 500)
  { // debounce sensor magnético

    tempRain++;

    contactTime = millis();
  }
}

void getRain(void)
{
  cli(); //desabilita interrupções

  rain = Bucket_Size_EU * tempRain;

  tempRain = 0;

  sei(); //habilita interrupções
}

void firmwareUpdate(void) {
  WiFiClientSecure client;
  client.setCACert(rootCACertificate);
  httpUpdate.setLedPin(LED_BUILTIN, LOW);
  t_httpUpdate_return ret = httpUpdate.update(client, URL_fw_Bin);

  switch (ret) {
  case HTTP_UPDATE_FAILED:
    Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
    break;

  case HTTP_UPDATE_NO_UPDATES:
    Serial.println("HTTP_UPDATE_NO_UPDATES");
    break;

  case HTTP_UPDATE_OK:
    Serial.println("HTTP_UPDATE_OK");
    break;
  }
}

int FirmwareVersionCheck(void) {
  String payload;
  int httpCode;
  String fwurl = "";
  fwurl += URL_fw_Version;
  fwurl += "?";
  fwurl += String(rand());
  Serial.println(fwurl);
  WiFiClientSecure * client = new WiFiClientSecure;

  if (client) 
  {
    client -> setCACert(rootCACertificate);

    HTTPClient https;

    if (https.begin( * client, fwurl))  // conexão HTTPS   
    {    
      Serial.print("[HTTPS] GET...\n");
      // 
      delay(100);
      httpCode = https.GET();                                  // estabelecendo conexão
      delay(100);
      if (httpCode == HTTP_CODE_OK)                             // versão recebida
      {
        payload = https.getString();                           // obtenção dos dados
        Serial.println(payload);
      } else {                                                 // erro
        Serial.print("error in downloading version file:");
        Serial.println(httpCode);
      }
      https.end();
    }
    delete client;
  }
      
  if (httpCode == HTTP_CODE_OK) // verificação
  {
    payload.trim();
    if (payload.equals(FirmwareVer)) {
      Serial.printf("\nDevice already on latest firmware version:%s\n", FirmwareVer);
      return 0;
    } 
    else 
    {
      Serial.println(payload);
      Serial.println("New firmware detected");
      return 1;
    }
  } 
  return 0;  
}
