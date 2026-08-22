// CÓDIGO PRINCIPAL DE MONITORAMENTO TÉRMICO
// Este arquivo contém a lógica do sistema (leitura do sensor, exibição no LCD,
// controle do LED de alerta e envio de dados para o ThingSpeak).


// BIBLIOTECAS
// Aqui importamos as bibliotecas necessárias para o projeto.

#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <time.h>


// CONFIGURAÇÕES DO WI-FI
// Aqui colocamos o nome e a senha da rede Wi-Fi.

const char* ssid = "Wokwi-GUEST";
const char* password = "";


// CONFIGURAÇÕES DO THINGSPEAK (NUVEM)
// Aqui definimos o servidor HTTP e a sua chave de API de escrita.

const char* serverName = "http://api.thingspeak.com/update";
String apiKey = "SECRET_THINGSPEAK_KEY";

// Variáveis para controle do tempo de envio para a nuvem sem travar o código
unsigned long ultimoEnvioNuvem = 0;
const unsigned long intervaloNuvem = 20000; // Envia a cada 20 segundos (20000 ms)


// CONFIGURAÇÃO DO DHT22
// O sensor de temperatura está conectado ao GPIO 33.

#define DHTPIN 33
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);


// CONFIGURAÇÃO DO LED
// O LED está conectado ao GPIO 32.

#define LED_PIN 32


// LIMITES DE TEMPERATURA
// Esses valores determinam o comportamento do LED.

#define LIMITE_ATENCAO 28.0
#define LIMITE_ALERTA 30.0


// CONFIGURAÇÃO DO LCD (16x2 com comunicação I2C)
// SDA → GPIO 26
// SCL → GPIO 25

LiquidCrystal_I2C lcd(0x27, 16, 2);


// CONFIGURAÇÃO DA DATA E HORA
// Aqui configuramos o horário obtido pela internet.

const char* ntpServer = "pool.ntp.org";

const long gmtOffset_sec = -3 * 3600;
const int daylightOffset_sec = 0;


// SETUP
// Esta parte é executada uma única vez quando a ESP32 inicia.

void setup() {

  // Inicia a comunicação serial
  Serial.begin(115200);

  // Configura o LED como saída
  pinMode(LED_PIN, OUTPUT);

  // Começamos com o LED desligado
  digitalWrite(LED_PIN, LOW);

  // Inicia o sensor DHT22
  dht.begin();

  // Inicia a comunicação I2C
  Wire.begin(26, 25);

  // Inicia o LCD
  lcd.init();
  lcd.backlight();

  // Mensagem inicial
  lcd.setCursor(0, 0);
  lcd.print("Conectando WiFi");

  // Conecta ao Wi-Fi com limite de tentativas (evita loop infinito)
  WiFi.begin(ssid, password);

  int tentativas = 0;
  int maxTentativas = 10; // Tenta conectar durante ~5 segundos

  while (WiFi.status() != WL_CONNECTED && tentativas < maxTentativas) {
    delay(500);
    tentativas++;
  }

  // Verifica se conseguiu conectar ou se deu tempo limite
  if (WiFi.status() == WL_CONNECTED) {

    // Configura data e hora pela internet apenas se houver conexão
    configTime(
      gmtOffset_sec,
      daylightOffset_sec,
      ntpServer
    );

    // Mensagem de conexão bem-sucedida
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi conectado");

  } else {

    // Mensagem caso não consiga conectar ao Wi-Fi
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sem Conexao WiFi");

  }

  delay(1500);

  lcd.clear();
}


// LOOP (essa parte fica sendo executada continuamente)
// Faz a leitura da temperatura, atualiza o LCD, controla o LED e envia para a nuvem.

void loop() {

  // Lê a temperatura do DHT22
  float temperatura = dht.readTemperature();

  // Verifica se a leitura é válida
  if (isnan(temperatura)) {

    lcd.setCursor(0, 0);
    lcd.print("Erro no sensor   ");

    delay(2000);

    return;
  }


  // DATA E HORA (obtém a data e a hora atuais)

  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {

    lcd.setCursor(0, 0);
    lcd.print("Erro na hora     ");

    delay(2000);

    return;
  }


  // LCD
  // Mostra a hora e a data na primeira linha, mostra a temperatura na segunda linha.

  lcd.setCursor(0, 0);

  if (timeinfo.tm_hour < 10) {
    lcd.print("0");
  }

  lcd.print(timeinfo.tm_hour);
  lcd.print(":");

  if (timeinfo.tm_min < 10) {
    lcd.print("0");
  }

  lcd.print(timeinfo.tm_min);

  lcd.print(" ");

  if (timeinfo.tm_mday < 10) {
    lcd.print("0");
  }

  lcd.print(timeinfo.tm_mday);
  lcd.print("/");

  if ((timeinfo.tm_mon + 1) < 10) {
    lcd.print("0");
  }

  lcd.print(timeinfo.tm_mon + 1);


  // Temperatura
  lcd.setCursor(0, 1);

  lcd.print("Temp: ");
  lcd.print(temperatura, 1);
  lcd.print((char)223);
  lcd.print("C   ");


  // CONTROLE DO LED (comportamento depende da temperatura)

  // Temperatura abaixo de 28 °C: (LED desligado)
  if (temperatura < LIMITE_ATENCAO) {

    digitalWrite(LED_PIN, LOW);

  }

  // Temperatura entre 28 °C e 30 °C: (LED piscando)
  else if (temperatura < LIMITE_ALERTA) {

    digitalWrite(LED_PIN, HIGH);

    delay(250);

    digitalWrite(LED_PIN, LOW);

  }

  // Temperatura igual ou superior a 30 °C: (LED ligado)
  else {

    digitalWrite(LED_PIN, HIGH);

  }


  // ENVIO DE DADOS PARA A NUVEM (THINGSPEAK)
  // Verifica se o Wi-Fi está conectado e se já se passou o intervalo de 20 segundos.

  if ((WiFi.status() == WL_CONNECTED) && (millis() - ultimoEnvioNuvem >= intervaloNuvem)) {

    HTTPClient http;

    // Monta a URL de envio: http://api.thingspeak.com/update?api_key=SUA_CHAVE&field1=VALOR
    String url = String(serverName) + "?api_key=" + apiKey + "&field1=" + String(temperatura, 1);

    http.begin(url);

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("Dados enviados para o ThingSpeak com sucesso! Codigo: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Erro ao enviar para o ThingSpeak: ");
      Serial.println(httpResponseCode);
    }

    http.end(); // Fecha a conexão HTTP

    // Atualiza o tempo do último envio
    ultimoEnvioNuvem = millis();
  }


  // Aguarda antes de fazer uma nova leitura.
  delay(1000);
}
