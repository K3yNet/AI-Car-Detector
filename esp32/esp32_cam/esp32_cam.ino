#include "esp_camera.h"
#include "soc/soc.h"           // Disable brownout detector
#include "soc/rtc_cntl_reg.h"  // Disable brownout detector
#include "driver/rtc_io.h"     // Disable brownout detector
#include <WiFi.h>
#include <PubSubClient.h>

//Informacoes da rede WiFi
// const char* ssid     = "CangoDama2.4G";
// const char* password = "a1b2c3d4e5f6";
const char* ssid = "CangoDama2.4G";
const char* password = "a1b2c3d4e5f6";

// --- Configurações do Broker MQTT ---
// const char* mqtt_broker = "192.168.15.12";
const char* mqtt_broker = "192.168.15.17";
const int mqtt_port = 1883;// Porta padrao MQTT sem segurança
const char* mqtt_client_id = "ESP32_CAM_1"; // ID único para o seu cliente MQTT
const char* mqtt_user = ""; // Usuário MQTT criado no Broker
const char* mqtt_pass = "";

// --- Tópicos MQTT ---
const char* topic_picture = "esp32/camera/picture"; // Tópico para publicar a temperatura

// --- Variáveis de controle de tempo ---
long lastPhotoTime = 0;
const long photoInterval = 5000; // Intervalo de publicação (5 segundos)

#define LED_BUILTIN 4 

// CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

WiFiClient espClient;
PubSubClient client(espClient);

// ----- WIFI FUNCTIONS -----

void wifi_connect(){
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

// ----- MQTT FUNCTIONS -----
// Retorno de chamada para o cliente MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  // Esta função é chamada quando o cliente MQTT recebe uma mensagem em um tópico inscrito.
  // Neste exemplo, o ESP32-CAM apenas publica, então esta função pode permanecer vazia.
  Serial.print("Mensagem recebida no tópico: ");
  Serial.print(topic);
  Serial.print(". Mensagem: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

// Função para reconectar ao broker MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conexão MQTT...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("conectado!");
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

// ----- CAMERA FUNCTIONS -----
void setup_camera(){
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // Disable brownout detector

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG; // Or PIXFORMAT_RGB565, PIXFORMAT_GRAYSCALE, etc.
  
  // If PSRAM is available, you can use larger frame sizes
  if(psramFound()){
    // config.frame_size = FRAMESIZE_UXGA; // 1600x1200
    config.frame_size = FRAMESIZE_SVGA; // 320x240
    config.jpeg_quality = 20;           // 0-63, lower is higher quality
    config.fb_count = 1;                // Two frame buffers for smoother operation
  } else {
    config.frame_size = FRAMESIZE_QVGA; // 800x600 (without PSRAM)
    config.jpeg_quality = 20;
    config.fb_count = 1;
  }

  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
}

// ----- CONFIGURACAO INICIAL -----

void setup() {

  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  setup_camera();

  wifi_connect();

  // Configura o cliente MQTT
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);
  client.setBufferSize(32768);

  Serial.println("Done.");
}

// ----- LOOP PRINCIPAL -----

void loop() {
  client.loop(); // Mantém conexão com broker
  if (!client.connected()) {
    reconnect(); // Se desconectado, tenta reconectar e subscrever novamente
  }

  unsigned long currentMillis = millis();

  if (currentMillis - lastPhotoTime >= photoInterval) {
    lastPhotoTime = currentMillis;

    Serial.println("Capturando foto...");
    // digitalWrite(LED_BUILTIN, HIGH);
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Falha na captura da câmera");
      return;
    }

    // A imagem já está em JPEG devido à configuração da câmera
    // Então, podemos publicar diretamente o buffer
    Serial.printf("Foto capturada, tamanho: %u bytes\n", fb->len);

    delay(10);
    if (client.publish(topic_picture, (const uint8_t*)fb->buf, fb->len)) {
      Serial.println("Foto publicada com sucesso no MQTT!");
    } else {
      Serial.print("Falha ao publicar foto no MQTT, estado: ");
      Serial.println(client.state());
      delay(500);
    }
    digitalWrite(LED_BUILTIN, LOW);

    esp_camera_fb_return(fb); // Libera o buffer da câmera
  }
}