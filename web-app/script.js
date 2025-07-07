// script.js
const BROKER_IP = "192.168.15.17";
const WEBSOCKET_PORT = 9001;
const TOPIC_IMAGEM = "esp32/ai_api";
const TOPIC_SEMAFORO_PRINCIPAL = "esp32/semaforo_state/main_avenue";
const TOPIC_SEMAFORO_TRANSVERSAL = "esp32/semaforo_state/cross_street";

const semaforoPrincipalEl = document.getElementById('semaforo-principal');
const semaforoTransversalEl = document.getElementById('semaforo-transversal');
const imagemEl = document.getElementById('imagem-rua');
const timestampEl = document.getElementById('timestamp');
const imagemOverlayEl = document.getElementById('imagem-overlay');
const forceTriggerBtn = document.getElementById('force-trigger-btn');

const clientId = "webapp_" + Math.random().toString(16).slice(2, 10);
const client = new Paho.MQTT.Client(BROKER_IP, WEBSOCKET_PORT, clientId);

client.onConnectionLost = onConnectionLost;
client.onMessageArrived = onMessageArrived;

function getConnectOptions() {
    return {
        timeout: 10,
        keepAliveInterval: 30,
        cleanSession: true,
        useSSL: false,
        onSuccess: onConnect,
        onFailure: onFailure,
    };
}

client.connect(getConnectOptions());

function onConnect() {
    console.log("Conectado ao Broker MQTT via WebSockets!");
    client.subscribe(TOPIC_IMAGEM);
    client.subscribe(TOPIC_SEMAFORO_PRINCIPAL);
    client.subscribe(TOPIC_SEMAFORO_TRANSVERSAL);
}

function onFailure(response) {
    console.error("Falha ao conectar: " + response.errorMessage);
}

function onConnectionLost(responseObject) {
    if (responseObject.errorCode !== 0) {
        console.log("Conexão perdida: " + responseObject.errorMessage);
        console.log("Tentando reconectar...");
        client.connect(getConnectOptions());
    }
}

function onMessageArrived(message) {
    const topic = message.destinationName;
    const payload = message.payloadString;

    switch (topic) {
        case TOPIC_IMAGEM:
            if (payload) {
                atualizarImagem(payload);
            } else {
                imagemOverlayEl.style.display = 'none';
                imagemEl.src = 'https://placehold.co/600x400/png';
                timestampEl.innerText = "Aguardando nova detecção...";
            }
            break;
        case TOPIC_SEMAFORO_PRINCIPAL:
            atualizarSemaforo(semaforoPrincipalEl, payload);
            break;
        case TOPIC_SEMAFORO_TRANSVERSAL:
            atualizarSemaforo(semaforoTransversalEl, payload);
            break;
    }
}

function atualizarSemaforo(element, status) {
    status = status.toUpperCase();
    element.className = 'card semaforo';
    const statusEl = element.querySelector('.status');
    switch (status) {
        case 'VERDE':
            element.classList.add('verde');
            statusEl.innerText = 'VERDE';
            break;
        case 'AMARELO':
            element.classList.add('amarelo');
            statusEl.innerText = 'AMARELO';
            break;
        case 'VERMELHO':
            element.classList.add('vermelho');
            statusEl.innerText = 'VERMELHO';
            break;
        default:
            element.classList.add('desconhecido');
            statusEl.innerText = 'DESCONHECIDO';
    }
}

function atualizarImagem(jsonPayload) {
    try {
        const data = JSON.parse(jsonPayload);
        const imageBase64 = data.image_processed_base64;
        const timestampUtcString = data.timestamp_utc;
        imagemOverlayEl.style.display = 'none';
        imagemEl.src = `data:image/jpeg;base64,${imageBase64}`;
        const date = new Date(timestampUtcString);
        const options = {
            year: 'numeric', month: '2-digit', day: '2-digit',
            hour: '2-digit', minute: '2-digit', second: '2-digit',
            hour12: false
        };
        timestampEl.innerText = `Atualização: ${date.toLocaleString('pt-BR', options)}`;
    } catch (e) {
        console.error("Erro ao processar o payload da imagem:", e);
        imagemOverlayEl.innerHTML = "<p>Erro ao carregar imagem.</p>";
        imagemOverlayEl.style.display = 'flex';
    }
}

function forcarDeteccao() {
    if (!client.isConnected()) {
        alert("Erro: Não foi possível enviar o comando. Verifique a conexão com o broker.");
        return;
    }
    const message = new Paho.MQTT.Message("");
    message.destinationName = TOPIC_IMAGEM;
    client.send(message);
    alert("Sinal para forçar nova detecção foi enviado!");
}

forceTriggerBtn.addEventListener('click', forcarDeteccao);