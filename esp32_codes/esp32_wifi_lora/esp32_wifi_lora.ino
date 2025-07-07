#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const char* ssid = ""; // SSID do WiFi
const char* password = ""; // Senha do WiFi, deixe vazio se não houver senha

const char* mqtt_server = ""; // Endereço IP do broker MQTT
const int mqtt_port = 1883;
const char* mqtt_topic_subscribe = "esp32/ai_api";
const char* mqtt_topic_publish_state_main_avenue = "esp32/semaforo_state/main_avenue";
const char* mqtt_topic_publish_state_cross_street = "esp32/semaforo_state/cross_street";
const char* mqtt_topic_control = "esp32/control_semaforo";

const int LED_PIN_VERDE = 16;
const int LED_PIN_AMARELO = 17;
const int LED_PIN_VERMELHO = 2;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

enum SemaforoState {
  VERMELHO_PADRAO,
  VERDE_ATIVO,
  AMARELO_TRANSICAO,
  VERMELHO_BLOQUEADO,
  SEGUNDO_AMARELO
};

SemaforoState currentSemaforoState_CrossStreet = VERMELHO_PADRAO;
SemaforoState currentSemaforoState_MainAvenue = VERDE_ATIVO;

unsigned long lastStateChangeTime = 0;
const long AMARELO_DURATION = 5000;
const long VERDE_DURATION   = 20000;
const long VERMELHO_BLOQUEADO_DURATION = 30000;

void updateSemaforoLEDs() {
  switch (currentSemaforoState_CrossStreet) {
    case VERMELHO_PADRAO:
    case SEGUNDO_AMARELO:
    case VERMELHO_BLOQUEADO:
      digitalWrite(LED_PIN_VERMELHO, HIGH);
      digitalWrite(LED_PIN_AMARELO, LOW);
      digitalWrite(LED_PIN_VERDE, LOW);
      break;
    case AMARELO_TRANSICAO:
      digitalWrite(LED_PIN_VERMELHO, LOW);
      digitalWrite(LED_PIN_AMARELO, HIGH);
      digitalWrite(LED_PIN_VERDE, LOW);
      break;
    case VERDE_ATIVO:
      digitalWrite(LED_PIN_VERMELHO, LOW);
      digitalWrite(LED_PIN_AMARELO, LOW);
      digitalWrite(LED_PIN_VERDE, HIGH);
      break;
  }
}

const char* getStateString(SemaforoState state) {
  switch (state) {
    case VERMELHO_PADRAO:
    case VERMELHO_BLOQUEADO:
      return "VERMELHO";
    case AMARELO_TRANSICAO:
    case SEGUNDO_AMARELO:
      return "AMARELO";
    case VERDE_ATIVO:
      return "VERDE";
    default:
      return "DESCONHECIDO";
  }
}

void publishSemaforoState() {
  const char* crossStreetStateStr = getStateString(currentSemaforoState_CrossStreet);
  const char* mainAvenueStateStr = getStateString(currentSemaforoState_MainAvenue);

  if (client.connected()) {
    client.publish(mqtt_topic_publish_state_cross_street, crossStreetStateStr);
    client.publish(mqtt_topic_publish_state_main_avenue, mainAvenueStateStr);
    Serial.print("Estado semáforo transversal publicado: ");
    Serial.println(crossStreetStateStr);
    Serial.print("Estado semáforo avenida principal publicado: ");
    Serial.println(mainAvenueStateStr);
  } else {
    Serial.println("Cliente MQTT não conectado, não foi possível publicar o estado.");
  }
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado!");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida no tópico: [");
  Serial.print(topic);
  Serial.print("] ");

  char payloadBuffer[length + 1];
  strncpy(payloadBuffer, (char*)payload, length);
  payloadBuffer[length] = '\0';

  Serial.println(payloadBuffer);

  if (strcmp(topic, mqtt_topic_subscribe) == 0) {
    if (currentSemaforoState_CrossStreet == VERMELHO_PADRAO) {
      currentSemaforoState_CrossStreet = SEGUNDO_AMARELO;
      currentSemaforoState_MainAvenue = AMARELO_TRANSICAO;
      lastStateChangeTime = millis();
      updateSemaforoLEDs();
      publishSemaforoState();
      Serial.println("Carro detectado na rua transversal! Semáforo transversal -> SEGUNDO_AMARELO, Semáforo principal -> AMARELO_TRANSICAO");
    } else {
      Serial.println("Carro detectado, mas semáforo transversal não está em VERMELHO_PADRAO. Ignorando.");
    }
  }
  else if (strcmp(topic, mqtt_topic_control) == 0) {
    if (strcmp(payloadBuffer, "VERDE") == 0) {
      if (currentSemaforoState_CrossStreet != VERDE_ATIVO) {
        currentSemaforoState_CrossStreet = VERDE_ATIVO;
        currentSemaforoState_MainAvenue = VERMELHO_PADRAO;
        lastStateChangeTime = millis();
        updateSemaforoLEDs();
        publishSemaforoState();
        Serial.println("Comando 'VERDE' recebido! Semáforo transversal -> VERDE_ATIVO (modo manual)");
      } else {
        Serial.println("Comando 'VERDE' recebido, semáforo transversal já está VERDE.");
      }
    } else if (strcmp(payloadBuffer, "VERMELHO") == 0) {
      if (currentSemaforoState_CrossStreet != VERMELHO_PADRAO) {
        currentSemaforoState_CrossStreet = VERMELHO_PADRAO;
        currentSemaforoState_MainAvenue = VERDE_ATIVO;
        lastStateChangeTime = millis();
        updateSemaforoLEDs();
        publishSemaforoState();
        Serial.println("Comando 'VERMELHO' recebido! Semáforo transversal -> VERMELHO_PADRAO (modo manual)");
      } else {
        Serial.println("Comando 'VERMELHO' recebido, semáforo transversal já está VERMELHO.");
      }
    } else {
      Serial.print("Comando desconhecido recebido no tópico de controle: ");
      Serial.println(payloadBuffer);
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT...");
    if (client.connect("ESP32ConsumerClient")) {
      Serial.println("conectado!");
      client.subscribe(mqtt_topic_subscribe);
      Serial.print("Assinado ao tópico: ");
      Serial.println(mqtt_topic_subscribe);
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos");
      delay(5000);
    }
  }
  publishSemaforoState();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN_VERDE, OUTPUT);
  pinMode(LED_PIN_AMARELO, OUTPUT);
  pinMode(LED_PIN_VERMELHO, OUTPUT);

  updateSemaforoLEDs();
  Serial.println("Semáforo inicializado no estado VERMELHO.");

  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(32768);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long currentTime = millis();

  switch (currentSemaforoState_CrossStreet) {
    case VERMELHO_PADRAO:
      break;
    case SEGUNDO_AMARELO:
      if (currentTime - lastStateChangeTime >= AMARELO_DURATION) {
        currentSemaforoState_MainAvenue = VERMELHO_PADRAO;
        currentSemaforoState_CrossStreet = VERDE_ATIVO;
        lastStateChangeTime = currentTime;
        updateSemaforoLEDs();
        publishSemaforoState();
        Serial.println("Tempo de SEGUNDO_AMARELO acabou. Semáforo transversal -> VERDE_ATIVO");
      }
      break;
    case VERDE_ATIVO:
      if (currentTime - lastStateChangeTime >= VERDE_DURATION) {
        currentSemaforoState_CrossStreet = AMARELO_TRANSICAO;
        currentSemaforoState_MainAvenue = VERMELHO_PADRAO;
        lastStateChangeTime = currentTime;
        updateSemaforoLEDs();
        publishSemaforoState();
        Serial.println("Tempo de VERDE acabou. Semáforo transversal -> AMARELO_TRANSICAO");
      }
      break;
    case AMARELO_TRANSICAO:
      if (currentTime - lastStateChangeTime >= AMARELO_DURATION) {
        currentSemaforoState_CrossStreet = VERMELHO_BLOQUEADO;
        currentSemaforoState_MainAvenue = VERDE_ATIVO;
        lastStateChangeTime = currentTime;
        updateSemaforoLEDs();
        publishSemaforoState();
        Serial.println("Tempo de AMARELO acabou. Semáforo transversal -> VERMELHO_BLOQUEADO");
      }
      break;
    case VERMELHO_BLOQUEADO:
      if (currentTime - lastStateChangeTime >= VERMELHO_BLOQUEADO_DURATION) {
        currentSemaforoState_CrossStreet = VERMELHO_PADRAO;
        currentSemaforoState_MainAvenue = VERDE_ATIVO;
        lastStateChangeTime = currentTime;
        updateSemaforoLEDs();
        publishSemaforoState();
        Serial.println("Tempo de VERMELHO_BLOQUEADO acabou. Semáforo transversal -> VERMELHO_PADRAO");
      }
      break;
  }
  delay(10);
}