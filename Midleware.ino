/*
 * Nome do projeto: 
 * colossus the working middleware of low energy cost(CWMLEC) - 
 * colosso o middleware de trabalho de baixo custo de energia
*/
#include <ESP8266WiFi.h>            //uso do módulo wifi
#include <ESP8266HTTPClient.h>      //uso da lib http
#include <PubSubClient.h>           //funcionalidades do protocolo pub/sub
#include "user_interface.h"         //interface para habilitar o LIGHT SLEEP wifi_set_sleep_type
#include <ArduinoJson.h>
#include "DHT.h"                    // Dht Arafruit Library
#include <OneWire.h>
#include <DallasTemperature.h>            

#define DHTPIN D2                   // pino que estamos conectado
#define DHTTYPE DHT11               // tipo do sensor que estámos usando

// BEGIN DEFINIÇÃO DE PINOS //
#define pin_18b20_sensor D1
#define pin_ldr_sensor   0
// END DEFINIÇÃO DE PINOS   // 

DHT dht(DHTPIN, DHTTYPE, 15);       // Objeto de DHT.h 

const  char* IDNODE  = "ESP-6";     //Identificador do nó

WiFiClient WiFiclient;              //instÂncia do Clientewifi
PubSubClient client(WiFiclient);    //instância do cliente mqtt

IPAddress ip(10, 77, 32, 31);
IPAddress dns(8, 8, 8, 8);          // Configurações de Acesso a rede Wifi UBCOMP.
IPAddress gateway(10, 77, 32, 1);   // Alteração feita no Método tryConnectWifi().
IPAddress subnet(255, 255, 248, 0);

// Variáveis para Recuperar informações do DHT11
float temp = 0;
float humi = 0;

// Inciando OneWire (18b20)
OneWire pino(pin_18b20_sensor);
DallasTemperature barramento(&pino);
DeviceAddress sensor;

long valueData = 0;

long previousMillis = 0;            //Variável de controle do tempo
long sensorInterval = 1 * 60000;    //Tempo em ms do intervalo a ser executado a função salveSensorDate
//1 * 60000000 = 1minuto
void startWifi();                   //Inicia o wifi
void startMqtt();                   //inicia o Mqtt
bool tryConnectWifi();              //Tenta conectar a uma rede wifi
bool tryConnectMqtt();              //Tenta conectar ao servidor MQTT
void salveSensorDate();             //Chama o método cadastrar do middleware para salvar os dados dos sensores

void saveSensorData();              //Chama o método cadastrar do middleware para salvar os dados dos sensores (Reimplementação)

/*Seta a configurações necessárias para conectar o wifi*/
void startWifi() {
    Serial.println("======begin startWifi=========\n");
    WiFi.persistent(false);
    WiFi.mode(WIFI_OFF); //  Force the ESP into client-only mode (padrão)
    WiFi.mode(WIFI_STA);
    Serial.println("======END startWifi=========\n");
}

/*Seta os parâmetros para conexão com o servidor broker*/
void startMqtt() {
    Serial.println("======begin startMqtt========\n");
    const char* BROKER   = "mqtt.eclipse.org";//Endereço do broker 
    const int   PORT     = 1883;       //porta padrão usada pelo mqtt para o broker 1883 
    client.setServer(BROKER, PORT);    //seta a porta e o endereço do broker
    client.setCallback(callBackMqtt); //faz uma chamada ao broker
    Serial.println("======END startMqtt==========\n");
}

//Tenta conectar à rede wifi, se deu certo retorna true
bool tryConnectWifi() {
    Serial.println("======begin tryConnectWifi=========\n");
    int cont = 0, tempo = 1000;
    const char* SSID     = "*"; //Nome da rede wifi
    const char* PASSWD   = "*"; 
    //WiFi.config(ip, dns, gateway, subnet);
    WiFi.begin(SSID, PASSWD);
    while (WiFi.status() != WL_CONNECTED) {
        Serial.println("tentando reconectar");
        delay(tempo);
        cont += tempo;
        if (cont == (tempo * 8)){//tenta por 4s
            Serial.println("ERRO AO CONECTAR");
            return false;
        }
    }
    Serial.println("======END tryConnectWifi=========\n");
    Serial.println("CONEXÃO BEM SUCEDIDA");
    return true;
}

/*Tenta conectar ao servidor broker, retorna true se tudo ocorrer com sucesso*/
bool tryConnectMqtt() {
    int cont = 0, tempo = 1000;
    Serial.println("======begin tryConnectMqtt=========\n");
    while (!client.connected()) {
        if (client.connect(IDNODE)) {
            client.subscribe("mpm2ee/energyPolicy");      
            Serial.println("inscrito");
        } else {
            delay(tempo);
            cont += tempo;
            if (cont == (tempo * 8)){//tenta por 4s
                Serial.println("Falha ao Reconectar\n");
                return false;  
            } 
        }
    }
    Serial.println("======END tryConnectMqtt=========\n");
    return true;
}

void salveSensorDate() {
    Serial.println("======begin salveSensorDate=========\n");
    HTTPClient http;                                    //Declara um objeto da classe HTTPClient
    delay(10000); 
    
    http.begin("http://192.168.1.107:8080/mpm2ee/esp");     //Especifica o destino da solicitação (endereço do serviço)
  
    http.addHeader("Content-Type", "application/json"); //Informa os detalhes da requisição (json)
    http.addHeader("Accept", "application/json");       

    //Informa os dados do node
    StaticJsonBuffer<300> jsonBuffer;
    JsonObject& node = jsonBuffer.createObject();
    node["id"] = "esp-5";
    node["regiao"] = "Interna";
    node["data"] = "2018-05-08";

    //Informa cada um dos sensores conectados ao node
    JsonObject& energia = jsonBuffer.createObject();
    energia["tipo"] = "energia";
    energia["dado"] = "48";

    JsonObject& agua = jsonBuffer.createObject();
    agua["tipo"] = "agua";
    agua["dado"] = "48";

    JsonObject& ph = jsonBuffer.createObject();
    ph["tipo"] = "PH";
    ph["dado"] = "98";

    //Insere os dados dos sensores ao node
    JsonArray& sensores = node.createNestedArray("sensores");
    sensores.add(ph);
    sensores.add(agua);
    sensores.add(energia);
    
    String sms;
    node.printTo(sms);             //Printa os dados dos sensores
    Serial.println(sms);
    //Envia o pedido com os dados dos sensores a serem salvos no firebase e recebe a resposta do serviço
    int httpCode = http.POST(sms);
    Serial.print("http-Code: ");
    Serial.println(httpCode);
    //String payload = http.getString();   //Get the request response payload
    http.end();                   //Encerra a conexão http
    delay(10000); 
    Serial.println("======END salveSensorDate=========\n");
}

/* *
 * Função chamada sempre que alguma mensagem é publicada no tópico em que o node foi inscrito.
 * Recebe a política que foi publicada e em seguida aplica, modificando o estado de sono e trabalho
 * do node.
 */
void callBackMqtt(char* topic, byte* payload, unsigned int length) {
    String mensagem;//mensagem do broker mqtt
    for (int i = 0; i < length; i++) {
        char c = (char)payload[i];
        mensagem += c;
    }Serial.println("Mensagem: " + mensagem);//String(topic)
    Serial.flush(); //Aguarda pela transmissão de dados seriais de saída para concluir.

    if (mensagem.equals("deepsleep")) {
          Serial.println("deepSleep");
          /*Onde o “time_in_us” é o tempo em microssegundos em que o ESP estará em modo 
          deep sleep. O valor máximo é de 4,294,967,295 microssegundos (2^32), o que 
          corresponde a aproximadamente 70 minutos.*/
          ESP.deepSleep( 5 * 60000000 ); 
    } else if (mensagem.equals("lightsleep")){
        Serial.println("LightSleep");
        //Ativa o light sleep
        WiFi.forceSleepBegin(); //força o wifi a dormir
        WiFi.mode(WIFI_STA);
        wifi_set_sleep_type(LIGHT_SLEEP_T);
//        wifi_set_sleep_type(LIGHT_SLEEP_T); //esse modo está parando tudo no node
//        wifi_fpm_set_sleep_type(LIGHT_SLEEP_T);
//        WiFi.disconnect();
        WiFi.forceSleepWake(); //força o wifi a acordar

    } else if (mensagem.equals("modemsleep")) {
        Serial.println("ModemSleep");
        //Força o modo MODEMSLEEP, mas por padrão já trabalha nesse modo, se der delay também entra nele, 
        //porém o delay interrompe o fluxo da aplicação
        wifi_set_sleep_type(MODEM_SLEEP_T);
        wifi_fpm_set_sleep_type(MODEM_SLEEP_T);
    } else if(mensagem.equals("1")){
        Serial.println("LED ON");
        //digitalWrite(led, HIGH);
    } else {
      Serial.println("LED ON");
      //digitalWrite(led, LOW);  
    }
}

void saveSensorData(){
  //Cliente Http
  HTTPClient http;                                   //Declara um objeto da classe HTTPClient
  http.begin("http://192.168.1.107:8080/mpm2ee/esp");  //Especifica o destino da solicitação (endereço do serviço)
  http.addHeader("Content-Type", "application/json"); //Informa os detalhes da requisição (json)
  http.addHeader("Accept", "application/json");       // 
  
  //Informa os dados do node
  StaticJsonBuffer<300> jsonBuffer;
  JsonObject& node = jsonBuffer.createObject();
  node["id"] = "ESP-4";
  node["regiao"] = "Interna";
  node["duracao"] = "2";
  node["politica"] = "deepsleep";

  //Informa cada um dos sensores conectados ao node
  JsonObject& temperatura = jsonBuffer.createObject();
  temperatura["tipo"] = "temperatura";
  temperatura["dado"] = temp;

  JsonObject& umidade = jsonBuffer.createObject();
  umidade["tipo"] = "umidade";
  umidade["dado"] = humi;

  JsonObject& luminosidade = jsonBuffer.createObject();
  luminosidade["tipo"] = "luminosidade";
  luminosidade["dado"] = saveLumi();

  JsonObject& temperaturaagua = jsonBuffer.createObject();
  temperaturaagua["tipo"] = "temperaturaagua";
  temperaturaagua["dado"] = saveTempAgua();
  

  //Insere os dados dos sensores ao node
  JsonArray& sensores = node.createNestedArray("sensores");
  sensores.add(temperatura);
  sensores.add(umidade);
  sensores.add(luminosidade);
  sensores.add(temperaturaagua);
    
  String sms;
  node.printTo(sms);             //Printa os dados dos sensores
  Serial.println(sms);
  //Envia o pedido com os dados dos sensores a serem salvos no firebase e recebe a resposta do serviço
  int httpCode = http.POST(sms);
  Serial.print("http-Code: ");
  Serial.println(httpCode);
  http.end();                   //Encerra a conexão http
  Serial.println("======END salveSensorDate=========\n");
}

void setup() {
    Serial.begin(115200); //velocidade da comunicação serial
    //WiFi.config(ip, dns, gateway, subnet);
    startMqtt();       //inicia o Mqtt
    tryConnectWifi();  //tenta conectar o wifi
    tryConnectMqtt();  //tenta conectar ao mqtrouter (servidor broker)

    // Definição de Pinos
    barramento.begin();
    barramento.getAddress(sensor, 0);
}

void loop() {
    
    while (WiFi.status() != WL_CONNECTED) {                   //Se não estiver conectado ao wifi
        tryConnectWifi();                                     //Tenta reconectar
    }
    
    while ( !client.connected() ) {                           //Se o clienteMQTT não estiver desconectado
        tryConnectMqtt();                                     //Tenta conectar ao servidor MQTT
    }

    saveSensorData();
    client.loop();

////  Serial.println(saveLumi());
////  delay(200);
//
////  Serial.println(saveTempAgua());
//  saveTempHumd();
//  Serial.println(temp);
//  Serial.println(humi);
//
//  Serial.println(saveTempAgua());
//
//  Serial.println(saveLumi());
//
//  delay(500);
  
    
}

// Função DHT
void saveTempHumd(){

  temp = 2147483647;
  humi = 2147483647;

  while(temp == 2147483647 && humi == 2147483647){
    temp = dht.readTemperature();
    humi = dht.readHumidity();
    Serial.println("Falhou");
  }

  Serial.println("DHT -> SUCESSFULL");
}

// Função LDR
float saveLumi(){
  Serial.println("LDR -> SUCESSFULL");
  return analogRead(pin_ldr_sensor);  
}

// Função 18b20
float saveTempAgua(){
  barramento.requestTemperatures();
  while(barramento.getTempC(sensor) == -127){
    Serial.println("18b20 -> FALHOU");
  }
  Serial.println("18b20 -> SUCESSFULL");
  return barramento.getTempC(sensor);
}
