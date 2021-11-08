#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include "Adafruit_Si7021.h"
#include <ThingsBoard.h>

void InitWiFi(); // declaração das funções
void reconnect();
void isr_rg();
void getRain();

#define SECRET_SSID "brisa-914290" // data Internet
#define SECRET_PASS "ea97ytxu"

const char* host = "esp32";
char ssid[] = SECRET_SSID; // variáveis de conexão ao WiFi
char pass[] = SECRET_PASS;

#define Token "rWxfsfvfCiA10Bp8EtU0" // data thingsboard / token do canal thingsboard
#define THINGSBOARD_SERVER "eltontorres.asuscomm.com"
#define SERIAL_DEBUG_BAUD 115200

int RainSensorPin = 33; // Rain REED-ILS sensor GPIO 21 on ESP32

volatile unsigned long contactTime; // tempo de debounce

volatile unsigned long tempRain; // quantidade de vezes que a balança do pluviometro girou

#define Bucket_Size_EU 0.2 // tamanho do bucket do pluviometro

float rain = 0; // Chuva temporario no período de loop, calculo total feito na plataforma thingsboard

#define uS_TO_S_FACTOR 1000000 // Fator de conversão de micro segundos para segundos
#define TIME_TO_SLEEP 20       // tempo de sleep em segundos

Adafruit_BMP280 bmpSensor; //objetos de comunicação com os sensores
Adafruit_Si7021 siSensor = Adafruit_Si7021();

WiFiClient cliente; // objetos WiFi e Thingsboard
ThingsBoard tb(cliente);

int status = WL_IDLE_STATUS; // status conexão WiFi
bool subscribed = false;     // status Thingsboard

void setup(void) {
  Serial.begin(SERIAL_DEBUG_BAUD);

  pinMode(RainSensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(RainSensorPin), isr_rg, FALLING);

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,0);

  if(esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0){
    tempRain++;
  }

  // Connect to WiFi network
  WiFi.begin(ssid, pass);
  InitWiFi();
  
  // Wait for connection
  if (WiFi.status() != WL_CONNECTED)
  { //verifica conexão Wi-Fi
    reconnect();
    return;
  }
  
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (!tb.connected())
  { //verifica conexão thingsboard
    subscribed = false;

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
    getRain(); //envio de dados do sensor BMP280
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

void loop(void) {
  
}


void InitWiFi() //conexão Wi-fi
{
  Serial.println("Conectando ao Wi-Fi ...");

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Conexão bem sucedida");
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

void isr_rg()
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
