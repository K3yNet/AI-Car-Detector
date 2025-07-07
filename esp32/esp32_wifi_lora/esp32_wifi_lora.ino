#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h> // Certifique-se de ter a versão 6 ou superior instalada

// --- Configurações de WiFi ---
const char* ssid = "CangoDama2.4G";         // Substitua pelo nome da sua rede WiFi
const char* password = "a1b2c3d4e5f6"; // Substitua pela senha da sua rede WiFi

// --- Configurações de MQTT ---
const char* mqtt_server = "192.168.15.17"; // Substitua pelo IP do seu broker MQTT
const int mqtt_port = 1883;
const char* mqtt_topic_subscribe = "esp32/ai_api"; // Tópico para consumir as mensagens
const char* mqtt_topic_publish_state_main_avenue = "esp32/semaforo_state/main_avenue";
const char* mqtt_topic_publish_state_cross_street = "esp32/semaforo_state/cross_street";
const char* mqtt_topic_control = "esp32/control_semaforo";

// --- Configurações do LED ---
const int LED_PIN_VERDE = 16;
const int LED_PIN_AMARELO = 17;
const int LED_PIN_VERMELHO = 2;

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// --- Estados do Semáforo ---
enum SemaforoState {
  VERMELHO_PADRAO,      // Semáforo vermelho, aguardando detecção de carro
  VERDE_ATIVO,          // Semáforo verde
  AMARELO_TRANSICAO,    // Semáforo amarelo após detecção de carro
  VERMELHO_BLOQUEADO,   // Semáforo vermelho após ciclo, ignorando novas detecções
  SEGUNDO_AMARELO       // Semáforo oposto está amarelo
};

SemaforoState currentSemaforoState = VERMELHO_PADRAO; // Estado inicial
SemaforoState currentSemaforoState_MainAvenue = VERDE_ATIVO; // Estado inicial

// --- Variáveis de Temporização do Semáforo ---
unsigned long lastStateChangeTime = 0; // Tempo da última mudança de estado
const long AMARELO_DURATION = 5000;   // 5 segundos
const long VERDE_DURATION   = 20000;  // 20 segundos
const long VERMELHO_BLOQUEADO_DURATION = 30000; // 30 segundos

// --- Função para Atualizar o Estado dos LEDs ---
void updateSemaforoLEDs() {
  switch (currentSemaforoState) {
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

// Função para converter o estado do enum para uma string C-style
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
      return "DESCONHECIDO"; // Caso de segurança
  }
}

// Função para publicar o estado atual dos semáforos via MQTT
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

// Função para conectar ao WiFi
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

// Função de callback para mensagens MQTT recebidas
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida no tópico: [");
  Serial.print(topic);
  Serial.print("] ");

  // Crie um buffer temporário para o payload e garanta que seja nulo-terminado
  // O +1 é para o caractere nulo ('\0') que indica o fim da string
  char payloadBuffer[length + 1];
  strncpy(payloadBuffer, (char*)payload, length);
  payloadBuffer[length] = '\0'; // Adiciona o terminador nulo

  Serial.println(payloadBuffer); // Para depuração, imprime o payload recebido

  // --- Lógica para detecção de carro (tópico original) ---
  // Use strcmp para comparar C-strings
  if (strcmp(topic, mqtt_topic_subscribe) == 0) {
    if (currentSemaforoState_CrossStreet == VERMELHO_PADRAO) {
      currentSemaforoState_CrossStreet = SEGUNDO_AMARELO;
      currentSemaforoState_MainAvenue = AMARELO_TRANSICAO; // Av. principal vai para amarelo
      lastStateChangeTime = millis();
      updateSemaforoLEDs();
      publishSemaforoState();
      Serial.println("Carro detectado na rua transversal! Semáforo transversal -> SEGUNDO_AMARELO, Semáforo principal -> AMARELO_TRANSICAO");
    } else {
      Serial.println("Carro detectado, mas semáforo transversal não está em VERMELHO_PADRAO. Ignorando.");
    }
  }
  // --- Lógica para controle manual (novo tópico) ---
  else if (strcmp(topic, mqtt_topic_control) == 0) {
    // Compare o payloadBuffer diretamente com os comandos esperados
    if (strcmp(payloadBuffer, "VERDE") == 0) {
      // Comando para forçar a rua transversal para VERDE
      if (currentSemaforoState_CrossStreet != VERDE_ATIVO) {
        currentSemaforoState_CrossStreet = VERDE_ATIVO;
        currentSemaforoState_MainAvenue = VERMELHO_PADRAO; // Se transversal está verde, principal deve estar vermelho
        lastStateChangeTime = millis(); // Reinicia o timer para o novo estado
        updateSemaforoLEDs();
        publishSemaforoState();
        Serial.println("Comando 'VERDE' recebido! Semáforo transversal -> VERDE_ATIVO (modo manual)");
      } else {
        Serial.println("Comando 'VERDE' recebido, semáforo transversal já está VERDE.");
      }
    } else if (strcmp(payloadBuffer, "VERMELHO") == 0) {
      // Comando para forçar a rua transversal para VERMELHO
      if (currentSemaforoState_CrossStreet != VERMELHO_PADRAO) {
        currentSemaforoState_CrossStreet = VERMELHO_PADRAO;
        currentSemaforoState_MainAvenue = VERDE_ATIVO; // Se transversal está vermelho, principal deve estar verde
        lastStateChangeTime = millis(); // Reinicia o timer
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


// Função para reconectar ao MQTT
void reconnect() {
  // Loop até estar conectado
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT...");
    // Tenta conectar
    // Você pode adicionar um ID de cliente único aqui, ex: "ESP32Client-" + String(random(0xffff), HEX)
    if (client.connect("ESP32ConsumerClient")) {
      Serial.println("conectado!");
      // Uma vez conectado, assina o tópico
      client.subscribe(mqtt_topic_subscribe);
      Serial.print("Assinado ao tópico: ");
      Serial.println(mqtt_topic_subscribe);
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos");
      // Espera 5 segundos antes de tentar novamente
      delay(5000);
    }
  }

  publishSemaforoState();
}

void setup() {
  Serial.begin(115200); // Inicializa a comunicação serial para depuração
  pinMode(LED_PIN_VERDE, OUTPUT);
  pinMode(LED_PIN_AMARELO, OUTPUT); 
  pinMode(LED_PIN_VERMELHO, OUTPUT);

  // Inicializa os LEDs no estado padrão (VERMELHO)
  updateSemaforoLEDs();
  Serial.println("Semáforo inicializado no estado VERMELHO.");

  setup_wifi(); // Conecta ao WiFi

  client.setServer(mqtt_server, mqtt_port); // Configura o servidor MQTT
  client.setCallback(callback); // Define a função de callback para mensagens recebidas
  client.setBufferSize(32768);

  // Opcional: Aumentar o tamanho do buffer MQTT se as mensagens forem muito grandes
  // Embora o JSON seja pequeno, é uma boa prática se as mensagens pudessem ser maiores.
  // client.setBufferSize(512); // Exemplo: 512 bytes, padrão é 256
}

void loop() {
  if (!client.connected()) {
    reconnect(); // Se não estiver conectado, tenta reconectar
  }
  client.loop(); // Processa as mensagens MQTT e mantém a conexão

  unsigned long currentTime = millis();

  // --- Lógica da Máquina de Estados do Semáforo ---
  switch (currentSemaforoState) {
    case VERMELHO_PADRAO:
      // Permanece em VERMELHO_PADRAO até receber uma mensagem
      break;

    case SEGUNDO_AMARELO:
      if (currentTime - lastStateChangeTime >= AMARELO_DURATION) {
        currentSemaforoState_MainAvenue = VERMELHO_PADRAO;
        currentSemaforoState = VERDE_ATIVO;
        updateSemaforoLEDs();
        publishSemaforoState();
        Serial.println("Tempo de SEGUNDO_AMARELO acabou. Semáforo -> VERDE_ATIVO");
      }

    case VERDE_ATIVO:
      if (currentTime - lastStateChangeTime >= VERDE_DURATION) {
        currentSemaforoState = AMARELO_TRANSICAO;
        currentSemaforoState_MainAvenue = VERMELHO_PADRAO;
        lastStateChangeTime = currentTime; // Reseta o timer
        updateSemaforoLEDs();
        publishSemaforoState();
        Serial.println("Tempo de VERDE acabou. Semáforo -> VERMELHO_BLOQUEADO");
      }
      break;

    case AMARELO_TRANSICAO:
      if (currentTime - lastStateChangeTime >= AMARELO_DURATION) {
        currentSemaforoState = VERMELHO_BLOQUEADO;
        currentSemaforoState_MainAvenue = VERDE_ATIVO;
        lastStateChangeTime = currentTime; // Reseta o timer
        updateSemaforoLEDs();
        publishSemaforoState();
        Serial.println("Tempo de AMARELO acabou. Semáforo -> VERDE");
      }
      break;

    case VERMELHO_BLOQUEADO:
      if (currentTime - lastStateChangeTime >= VERMELHO_BLOQUEADO_DURATION) {
        currentSemaforoState = VERMELHO_PADRAO;
        currentSemaforoState_MainAvenue = VERDE_ATIVO;
        lastStateChangeTime = currentTime; // Reseta o timer
        updateSemaforoLEDs();
        publishSemaforoState();
        Serial.println("Tempo de VERMELHO_BLOQUEADO acabou. Semáforo -> VERMELHO_PADRAO");
      }
      break;
  }
  
  delay(10); // Pequeno atraso para outras tarefas
}