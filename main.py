import io
import json
import base64
import sys
import datetime
from PIL import Image, ImageDraw, ImageFont
from transformers import pipeline
import paho.mqtt.client as mqtt

MQTT_BROKER = "localhost"
MQTT_PORT = 1883
MQTT_TOPIC_IMAGENS = "esp32/camera/picture"
MQTT_TOPIC_RESULTADOS = "esp32/ai_api"

def desenhar_caixas_e_rotulos(img: Image.Image, detalhes_analise: list) -> Image.Image:
    draw = ImageDraw.Draw(img)
    try:
        font = ImageFont.truetype("arial.ttf", 15)
    except IOError:
        font = ImageFont.load_default()

    for obj in detalhes_analise:
        box = obj['box']
        coordenadas = [box['xmin'], box['ymin'], box['xmax'], box['ymax']]
        cor = "yellow"
        if obj['label'] == 'car':
            cor = "lime"
        elif obj['label'] == 'bus':
            cor = 'orange'
        
        draw.rectangle(coordenadas, outline=cor, width=3)
        texto_rotulo = f"{obj['label']}: {obj['score']:.2f}"
        posicao_texto = (box['xmin'], box['ymin'] - 15)
        draw.text(posicao_texto, texto_rotulo, fill=cor, font=font)
        
    return img

def analisar_imagem(img: Image.Image, confianca_minima: float = 0.5):
    veiculos_alvo = ['car', 'bus', 'motorcycle'] 

    if img.mode != "RGB":
        img = img.convert("RGB")
        
    resultados_raw = detector(img)
    
    resultados_filtrados = [
        {
            "score": round(obj["score"], 4),
            "label": obj["label"],
            "box": obj["box"]
        } for obj in resultados_raw if obj['label'] in veiculos_alvo and obj['score'] >= confianca_minima
    ]

    veiculo_detectado = len(resultados_filtrados) > 0
    
    return {"vehicle_detected": veiculo_detectado, "details": resultados_filtrados}

def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        client.subscribe(MQTT_TOPIC_IMAGENS)
    else:
        print(f"Falha ao conectar ao MQTT, código de retorno: {rc}\n", file=sys.stderr)

def on_message(client, userdata, msg):
    try:
        imagem_original = Image.open(io.BytesIO(msg.payload))
        resultado_analise = analisar_imagem(imagem_original)
        
        if resultado_analise["vehicle_detected"]:
            imagem_com_caixas = desenhar_caixas_e_rotulos(
                imagem_original.copy(), 
                resultado_analise["details"]
            )
            
            buffer = io.BytesIO()
            imagem_com_caixas.save(buffer, format="JPEG")
            bytes_imagem = buffer.getvalue()

            imagem_base64 = base64.b64encode(bytes_imagem).decode('utf-8')
            timestamp_atual = datetime.datetime.now().isoformat()
            
            payload_resposta = {
                "timestamp_utc": timestamp_atual,
                "image_processed_base64": imagem_base64
            }

            client.publish(MQTT_TOPIC_RESULTADOS, json.dumps(payload_resposta), retain=True)

    except Exception as e:
        print(f"[!] Erro ao processar a imagem: {e}", file=sys.stderr)

if __name__ == "__main__":
    try:
        detector = pipeline("object-detection", model="facebook/detr-resnet-50")
    except Exception as e:
        print(f"Falha fatal ao carregar o modelo de IA: {e}", file=sys.stderr)
        sys.exit(1)

    mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message

    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    except Exception as e:
        print(f"Não foi possível conectar ao broker MQTT em {MQTT_BROKER}:{MQTT_PORT}. Erro: {e}", file=sys.stderr)
        sys.exit(1)

    try:
        mqtt_client.loop_forever()
    except KeyboardInterrupt:
        mqtt_client.disconnect()