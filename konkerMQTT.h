#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "BlinkerID.h"
#include "parseURL.h"


//Trigger da configuracao via Json -- Tambem precisa ser uma variavel global para rodar em duas maquinas de estado.
bool shouldSaveConfig = 0;

//Buffer das mensagens MQTT
char bufferJ[256];



char in_topic[32];
char command_ts[32];
char data_topic[32];
char config_topic[32];
char config_break[2]="0";
String configured_mqtt_server;
char configured_device_login[32];
char configured_device_pass[32];

const char sub_dev_modifier[4] = "sub";
const char pub_dev_modifier[4] = "pub";
char data_channel[7] = "data";
char in_channel[7] = "in";
char config_channel[7] = "config";


//flag de entrada de msg
int received_msg=0;
//tópico de entrada MQTT;
char receivedTopic[32];
//Buffer de entrada MQTT;
char receivedTopicMsg[2048];


//Definindo os objetos de Wifi e MQTT
WiFiClient espClient;
PubSubClient client(espClient);

WiFiManager wifiManager;


// Funcao de Callback para o MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  int i;
  int state=0;

  Serial.print("Mensagem recebida [");
  Serial.print(topic);
  Serial.print("] ");
  for (i = 0; i < length; i++) {
    receivedTopicMsg[i] = payload[i];
    Serial.print((char)payload[i]);
  }
  receivedTopicMsg[i] = '\0';
  strcpy(receivedTopic, topic);
  received_msg = 1;
  Serial.println("");
}


int MQTTSUB(const char topic[]){
  Serial.print("Suscribing in " + String(topic));
  int result=client.subscribe(topic);
  Serial.println(", result=" + String(result));
  return result;
}


int testMQTTConn(String id){
  int result=0;
  int MQTTcode = client.state();

  if (MQTTcode==0){
     result= 1;
  }else{
    Serial.print("Connecting to MQTT broker ");
    //generate ramdom ID
    int i=0;
    int random_n = random(10000);
    String temp_id = id + String (random_n);
    const int diment = temp_id.length()+1;
    char newid[diment];
    temp_id.toCharArray(newid, temp_id.length()+1);


    //mqtt://mqtt.demo.konkerlabs.net:1883
    String strmqtt_server;
    String strpath;
    String strmqtt_port;
    parseURLtoURIePath(configured_mqtt_server,strmqtt_server, strpath, strmqtt_port);
    char mqtt_server[64];
    char mqtt_port[5];
    strmqtt_server.toCharArray(mqtt_server, 64);
    strmqtt_port.toCharArray(mqtt_port, 5);

    Serial.print(" URI:" + String(mqtt_server) + " Port:" + String(atoi(mqtt_port)) + ", ");

    client.setServer(mqtt_server, atoi(mqtt_port));
    client.setCallback(callback);
    //try to connect
    Serial.print(" U:" + String(configured_device_login) + " P:" + String(configured_device_pass));
    int connectCode=client.connect(newid, configured_device_login, configured_device_pass);
    Serial.print(", connectCode=" + connectCode);

    if (connectCode==1) { //Check the returning code
      return 1;
    }else{
      return 0;
    }

  }

  if (MQTTcode==0) { //Check the returning code
    return 1;
  }else{
    return 0;
  }

}



//----------------- Funcao para conectar ao broker MQTT e reconectar quando perder a conexao --------------------------------
int MQTTRetry(int retries, String id) {
  int i=0;

  //Loop ate estar conectado
  while (!testMQTTConn(id)) {
    Serial.print("Checando conexão MQTT com o server...");
    if (testMQTTConn(id)) {
      Serial.println("Conectado!");
      received_msg=0;
      return i;
    } else {
      Serial.println("Tentando conectar novamente em 3 segundos");
      // Esperando 3 segundos antes de re-conectar
      delay(3000);
      if (i==retries) break;
      i++;
    }
  }
  return i;
}



int checkConnection(int tentatives, String id){
  int result=0;
  if (WiFi.SSID()!="") {
    if (testMQTTConn(id)==0) {
      MQTTRetry(3, id);
      int auxTent=tentatives;
      while (tentatives>0 && testMQTTConn(id)==0) {
        Serial.println("Tentando reconectar.." + String(auxTent-tentatives+1));
        WiFi.begin();
        delay(3000);
        tentatives--;
      }
      if (tentatives<=0) {
        Serial.println("Fim de tentativas");
        result= 0;
      }else{
        result= 1;
        //se teve que reconectar, faça subscribe denovo
        MQTTSUB(in_topic);
        MQTTSUB(config_topic);
      }
    }else{
      result= 1;
    }
  }else{
    Serial.println("Wifi zerado");
    result= 0;
  }
  return result;
}





void MQTTPUB(const char topic[], const char msg[]){
  if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
    int pubCode=-1;
    if (client.state()==0){
      pubCode=client.publish(topic, msg);
    }else{
      Serial.println("Not connected!");
    }

    Serial.println("Publish topic: " + String(topic) + "; Body: " + String(msg) + "; pubCode: " + String(pubCode));

    //http.end();   //Close connection
  }else{
    Serial.println("Not connected!");
  }
}

void MQTTPUB(String StringTopic, const char msg[]){
  char topic[32];
  StringTopic.toCharArray(topic, StringTopic.length()+1);

  if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status
    int pubCode=-1;
    if (client.state()==0){
      pubCode=client.publish(topic, msg);
    }else{
      Serial.println("Not connected!");
    }

    Serial.println("Publish topic: " + String(topic) + "; Body: " + String(msg) + "; pubCode: " + String(pubCode));

    //http.end();   //Close connection
  }else{
    Serial.println("Not connected!");
  }
}



String MQTTCheckMsgs(const char topic[]){
  String payload="";
  if(received_msg==1){
    if(strcmp(receivedTopic, topic)==0){
      payload= String(receivedTopicMsg);
    }
    received_msg=0;
  }

  if (payload!="[]"){
    return payload;   //Get the request response payload
  }else{
    return "null";
  }

}






//----------------- Funcao de Trigger para salvar configuracao no FS ---------------------------
void saveConfigCallback() {
  Serial.println("Salvar a Configuracao");
  shouldSaveConfig = 1;
}


//----------------- Copiando parametros configurados via HTML ---------------------------
void copyHTMLPar(char mqtt_server[], char device_login[], char device_pass[], WiFiManagerParameter custom_mqtt_server, WiFiManagerParameter custom_device_login, WiFiManagerParameter custom_device_pass){
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(device_login, custom_device_login.getValue());
  strcpy(device_pass, custom_device_pass.getValue());

}

//----------------- Montando o sistema de arquivo e lendo o arquivo config.json ---------------------------
void spiffsMount(char mqtt_server[], char device_login[], char device_pass[]) {


  if (SPIFFS.begin()) {
    //Serial.println("Sistema de Arquivos Montado");
    if (SPIFFS.exists("/config.json")) {
      //Serial.println("Arquivo já existente, lendo..");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        //Serial.println("Arquivo aberto com sucesso");
        size_t size = configFile.size();
        // Criando um Buffer para alocar os dados do arquivo.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        //json.printTo(Serial);
        if (json.success()) {
          //Serial.println("\nparsed json");

          if (json.containsKey("mqtt_server")) strcpy(mqtt_server, json["mqtt_server"]);
          if (json.containsKey("device_login")) strcpy(device_login, json["device_login"]);
          if (json.containsKey("device_pass")) strcpy(device_pass, json["device_pass"]);
          if (json.containsKey("command_ts")) strcpy(command_ts, json["command_ts"]);
          if (json.containsKey("in_topic")) strcpy(in_topic, json["in_topic"]);
          if (json.containsKey("config_topic")) strcpy(config_topic, json["config_topic"]);
          if (json.containsKey("data_topic")) strcpy(data_topic, json["data_topic"]);
          if (json.containsKey("break")) strcpy(config_break, json["break"]);

          configured_mqtt_server=String(mqtt_server);
          strcpy(configured_device_login,device_login);
          strcpy(configured_device_pass, device_pass);


        }
        else {
          Serial.println("Falha em ler o Arquivo");if (json.containsKey("config_topic")) strcpy(config_topic, json["config_topic"]);
        }
      }
    }
  } else {
    Serial.println("Falha ao montar o sistema de arquivos");
  }

}

//----------------- Salvando arquivo de configuracao ---------------------------
void saveConfigtoFile(char mqtt_server[], char device_login[], char device_pass[])
{
  String SString;
  SString = String(pub_dev_modifier) + String("/") + String(device_login) + String("/") + String(data_channel); //pub
  SString.toCharArray(data_topic, SString.length()+1);

  SString = String(sub_dev_modifier) + String("/") + String(device_login) + String("/") + String(in_channel); //sub
  SString.toCharArray(in_topic, SString.length()+1);

  SString = String(sub_dev_modifier) + String("/") + String(device_login) + String("/") + String(config_channel); //sub
  SString.toCharArray(config_topic, SString.length()+1);

  Serial.println("Salvando Configuracao");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  configured_mqtt_server=String(mqtt_server);
  strcpy(configured_device_login,device_login);
  strcpy(configured_device_pass,device_pass);
  json["mqtt_server"] = mqtt_server;
  json["device_login"] = device_login;
  json["device_pass"] = device_pass;
  json["command_ts"]=command_ts;
  json["in_topic"] = in_topic;
  json["config_topic"] = config_topic;
  json["data_topic"] = data_topic;
  json["break"] = config_break;



  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Falha em abrir o arquivo com permissao de gravacao");
  }

  //json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
}



JsonObject& readConfigFile(){

  if (SPIFFS.begin()) {
    //Serial.println("Sistema de Arquivos Montado");
    if (SPIFFS.exists("/config.json")) {
      //Serial.println("Arquivo já existente, lendo..");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        //Serial.println("Arquivo aberto com sucesso");
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(configFile.readString());
        //json.printTo(Serial);
        if (json.success()) {
          //Serial.println("\nparsed json");

          return json;

        }
        else {
          Serial.println("Falha em ler o Arquivo");
        }
      }
    }
  } else {
    Serial.println("Falha ao montar o sistema de arquivos");
  }

}




//----------------- Salvando arquivo de configuracao ---------------------------
int saveJSONToFile(JsonObject& jsonToSave)
{
  int saved=0;
  JsonObject& filejson = readConfigFile();

  //Serial.println("Checando valores recebidos..");
  for (JsonObject::iterator it=jsonToSave.begin(); it!=jsonToSave.end(); ++it)
  {
     String keyNameToSave=it->key;
     if (filejson.containsKey(keyNameToSave)){
      // Serial.print("Key json encontrada: " + keyNameToSave);
       String fileValue=filejson[keyNameToSave];
       if(fileValue!=(it->value) ){
         filejson[keyNameToSave]=(it->value);
         saved=1;
         Serial.println(", valor atualizado!");
       }else{
         //Serial.println(", sem alterações de valor.");
       }
     }
  }


  if(saved==1){
    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("Falha em abrir o arquivo com permissao de gravacao");
    }

    filejson.printTo(Serial);
    filejson.printTo(configFile);
    configFile.close();
  }
}



char *buildJSONmsg(JsonObject& jsonMSG)
{

  jsonMSG.printTo(bufferJ, sizeof(bufferJ));
  return bufferJ;
}



//----------------- Decodificacao da mensagem Json In -----------------------------
char *parse_in_command_JSON(const char json[])
{
  const char *cmd = "null";
  StaticJsonBuffer<2048> jsonBuffer;
  JsonObject& jsonMSG = jsonBuffer.parseObject(json);
    if (jsonMSG.containsKey("command")) cmd = jsonMSG["command"];
  char *command = (char*)cmd;
  return command;
}

char *parse_JSON_item(String json, const char itemName[])
{
  const char *ival = "null";
  StaticJsonBuffer<2048> jsonBuffer;
  JsonObject& jsonMSG = jsonBuffer.parseObject(json);
    if (jsonMSG.containsKey(itemName)) ival = jsonMSG[itemName];
  char *itemval = (char*)ival;
  return itemval;
}

char *parse_JSON_timestamp(String json)
{
  const char *ival = "null";
  StaticJsonBuffer<2048> jsonBuffer;
  JsonObject& jsonMSG = jsonBuffer.parseObject(json);
  if (jsonMSG.containsKey("timestamp")) ival = jsonMSG["timestamp"];
  char *itemval = (char*)ival;
  return itemval;
}




void startKonkerAP(String apNome, char mqtt_server[], char device_login[], char device_pass[], WiFiManagerParameter custom_mqtt_server, WiFiManagerParameter custom_device_login, WiFiManagerParameter custom_device_pass){
  if(wifiManager.autoConnect(blinkMyID(apNome))==0) {
    //Copiando parametros
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(device_login, custom_device_login.getValue());
    strcpy(device_pass, custom_device_pass.getValue());

    delay(3000);
    ESP.reset();
  }
}

void checkFWUpdates(String SERVER_URI, int PORT, String BIN_PATH){
    Serial.println(SERVER_URI);
    Serial.print(BIN_PATH);
    Serial.println("");
    t_httpUpdate_return ret = ESPhttpUpdate.update(SERVER_URI, PORT, BIN_PATH);
    switch(ret) {
        case HTTP_UPDATE_FAILED:
            Serial.println("[update] Update failed.");
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[update] Update no Update.");
            break;
        case HTTP_UPDATE_OK:
            Serial.println("[update] Update ok."); // may not called we reboot the ESP
            break;
    }
  }


void checkFWUpdates(String SERVER_URI, String BIN_PATH){
  checkFWUpdates(SERVER_URI,80,BIN_PATH);
}
