#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
////////////////////////////////////////////

#define leituraMaxima 60 // Numero maximo de leituras offline, antes de fazer upload para supervisorio
#define TEMPO_ENTRE_ENVIOS 60// Numero de minutos entre envios de dados
//intervalo de descarga em minutos = leituraMaxima*DEEP_SLEEP_TEMPO*

float COMPENSADOR = ((TEMPO_ENTRE_ENVIOS*60)-222.290656)/2991.3267539;

typedef struct {
  float Tensao;
  float Fluxo;
  float Pressao;
} leiturasSensores;

RTC_DATA_ATTR int leituraCnt = 0;

RTC_DATA_ATTR leiturasSensores Leituras[leituraMaxima];

/////////////////////////////////////////////

int posi[100], pos, aux, posi2[100], pos2, aux2;

int analogPIN1 = 33; //Pino de leitura da tensão da bateria
int analogPIN2 = 32; //Pino de leitura da pressão d'água
int fluxoPIN3 = 17;
int amostras = 100;

float tensao, pressao;

long currentMillis = 0;
long previousMillis = 0;
int interval = 1000;
float calibrationFactor = 0.0025*60;
volatile byte pulseCount = 0;
byte pulse1Sec = 0;
float flowRate = 0;

////////-------------*************************-------------////////////////-------------*************************-------------////////////////-------------*************************-------------////////

void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}

////////-------------*************************-------------////////////////-------------*************************-------------////////////////-------------*************************-------------////////

void fazerLeituras() {

  for (int k = 0; k < amostras; k++) {
    posi[k] = analogRead(analogPIN1);
    posi2[k] = analogRead(analogPIN2);
    int now = millis();
    while (millis() - now < 25) { }
  }

  for (int j = 0; j < amostras; j++) {
    for (int l = 0; l < amostras; l++) {
      if (posi[j] < posi[l]) {
        aux = posi[l];
        posi[l] = posi[j];
        posi[j] = aux;

        aux2 = posi2[l];
        posi2[l] = posi2[j];
        posi2[j] = aux2;

      }
    }
  }

  pos = (posi[(amostras / 2) - 1] + posi[amostras / 2]) / 2;
  pos2 = (posi2[(amostras / 2) - 1] + posi2[amostras / 2]) / 2;

  if (pos >= 0 && pos <= 1200) {
    tensao = 0.003407 * pos + 0.3911; //V

  } else if (pos > 1200 && pos < 2579) {
    tensao = 0.003258 * pos + 0.5111; //V

  } else if (pos >= 2579 && pos < 3546) {
    tensao = 0.003130 * pos + 0.8605; //V

  } else if (pos > 3546 && pos <= 4096) {
    tensao = 0.001985 * pos + 4.622; //V

  }

  pressao = ((float(pos2) - 752.0) / (4095 - 752.0)) * 35.0; //mca
  /*
  if (pressao < 0) {
    pressao = 0;

  } else if (pressao > 40) {
    pressao = 40;

  }
  */

  attachInterrupt(fluxoPIN3, pulseCounter, RISING);

  previousMillis = millis();

  while (millis() - previousMillis < 1000) {}

  detachInterrupt(fluxoPIN3);

  pulse1Sec = pulseCount;
  pulseCount = 0;
  
  flowRate = (( pulse1Sec) * calibrationFactor); //L/hora
  previousMillis = millis();
  Serial.println("AAA - " + String(flowRate));
  /*
  Serial.println(flowRate);
  Serial.print("  |  ");
  Serial.print(pos);
  Serial.print("  |  ");
  Serial.print(pos2);
  Serial.println();
  */
  Leituras[leituraCnt].Tensao = tensao;
  Leituras[leituraCnt].Fluxo = flowRate;
  Leituras[leituraCnt].Pressao = pressao;

  leituraCnt = leituraCnt + 1;
  
  Serial.println("----------------------");
  Serial.println(leituraCnt);
  Serial.println(COMPENSADOR);
  
}

////////-------------*************************-------------////////////////-------------*************************-------------////////////////-------------*************************-------------////////

void uploadDeLeituras() {

  //const char* ssid = "JABRE2_2G";
  //const char* password = "casaamarela";
  //const char* ssid = "Lucas Dantas";
  //const char* password = "lucas123";
  //const char* ssid = "TP-LINK_9291E8";
  //const char* password = "";
  const char* ssid = "TP-LINK_9291E8-EXT";
  const char* password = "";

  const char* mqtt_server = "broker.emqx.io";
  const int mqtt_port = 1883; // Porta padrão do MQTT

  WiFiClient espClient;
  PubSubClient client(espClient);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando à rede Wi-Fi...");
  }

  Serial.println("Conectado à rede Wi-Fi!");

  client.setServer(mqtt_server, mqtt_port);

  if (!client.connected()) {
    while (!client.connected()) {
      //Serial.print("Conectando ao EMQ X Broker...");

      // Tenta conectar ao broker MQTT
      if (client.connect("ESP32_Client")) {
        //Serial.println("Conectado!");
      } else {
        //Serial.print("Falha na conexão - Estado: ");
        //Serial.print(client.state());
        //Serial.println(" Tentando novamente em 5 segundos...");

        // Espera 5 segundos antes de tentar novamente
        delay(1000);
      }
    }
  }

  String dadosTensao;
  String dadosFluxo;
  String dadosPressao;

  for (int j = 1; j < 11; j++) {

    for (int p = (j - 1) * 6; p < (j) * 6; p++) {

      // Converter o valor inteiro para string antes de concatenar
      dadosTensao += String(Leituras[p].Tensao);
      dadosFluxo += String(Leituras[p].Fluxo);
      dadosPressao += String(Leituras[p].Pressao);

      // Adicionar um separador (por exemplo, uma vírgula) entre os itens
      if (p < (j) * 6 - 1) {
        dadosTensao += ",";
        dadosFluxo += ",";
        dadosPressao += ",";


      }
    }

    const int JSON_BUFFER_SIZE = JSON_OBJECT_SIZE(3 * 6) + 200; // Ajuste o tamanho conforme suas necessidades

    // Converter estrutura para JSON
    StaticJsonDocument<JSON_BUFFER_SIZE> jsonDoc;
    //Serial.println(dadosFluxo);
    jsonDoc["tensao"] = dadosTensao;
    jsonDoc["fluxo"] = dadosFluxo;
    jsonDoc["pressao"] = dadosPressao;

    String jsonString;
    serializeJson(jsonDoc, jsonString);

    client.publish("leituras_ESP32_08112001", jsonString.c_str());

    dadosTensao = "";
    dadosFluxo = "";
    dadosPressao = "";

    delay(1500);

  }

  WiFi.disconnect();

  // Aguarda até que a conexão seja encerrada
  while (WiFi.status() == WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  leituraCnt = 0; // Resetar contador*/
}

////////-------------*************************-------------////////////////-------------*************************-------------////////////////-------------*************************-------------////////

void goToDeepSleep()
{

  //Serial.println("Indo dormir...");
   
  esp_sleep_enable_timer_wakeup(60 * 1000000);
  esp_deep_sleep_start();

}

////////-------------*************************-------------////////////////-------------*************************-------------////////////////-------------*************************-------------////////

void setup()
{
  Serial.begin(115200);
  pinMode(analogPIN1, INPUT);
  pinMode(analogPIN2, INPUT);
  pinMode(fluxoPIN3, OUTPUT);
  digitalWrite(fluxoPIN3, HIGH);

  fazerLeituras();

  if (leituraCnt >= leituraMaxima) {

    uploadDeLeituras();

  }



  goToDeepSleep();

}

////////-------------*************************-------------////////////////-------------*************************-------------////////////////-------------*************************-------------////////

void loop()
{
  // Utilizando o modo deepSleep, apenas o setup() é executado.
}
