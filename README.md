# API de Detecção de Carros 🚗

Esta é uma API desenvolvida em Python com FastAPI que utiliza um modelo de Inteligência Artificial da Hugging Face para detectar se há carros em uma imagem enviada pelo usuário.

## Tecnologias Utilizadas

* **Python 3.10+**
* **FastAPI**: Framework web para a construção da API.
* **Uvicorn**: Servidor ASGI para rodar a API.
* **Hugging Face Transformers**: Para carregar e utilizar o modelo de detecção de objetos.
* **Pillow**: Para manipulação de imagens.

---

## ⚙️ Instalação e Configuração

Siga os passos abaixo para configurar e executar o projeto em sua máquina local.

#### 1. Clonar o Repositório

Primeiro, clone este repositório para a sua máquina.

```bash
git clone https://github.com/K3yNet/AI-Car-Detector.git
```

#### 2. Criar e Ativar o Ambiente Virtual

É uma boa prática usar um ambiente virtual (`venv`) para isolar as dependências do projeto.

```bash
# Criar o ambiente virtual (geralmente chamado de 'venv')
python -m venv venv
```

Agora, ative o ambiente. O comando varia dependendo do seu sistema operacional:

* **No Windows (PowerShell ou CMD):**
    ```bash
    .\venv\Scripts\activate
    ```

* **No Linux ou macOS:**
    ```bash
    source venv/bin/activate
    ```

Após a ativação, você verá `(venv)` no início da linha do seu terminal.

#### 3. Instalar as Dependências

Com o ambiente virtual ativo, instale todas as bibliotecas necessárias usando o arquivo `requirements.txt`.

```bash
pip install -r requirements.txt
```
Este comando irá ler o arquivo e instalar FastAPI, Uvicorn, Transformers, Torch, Pillow, etc.

---

## 🚀 Executando o Servidor

Após a instalação, você pode iniciar o servidor da API com o seguinte comando:

```bash
uvicorn main:app --reload
```

* `main`: refere-se ao arquivo `main.py`.
* `app`: refere-se ao objeto `app = FastAPI()` criado dentro do arquivo.
* `--reload`: reinicia o servidor automaticamente sempre que você fizer uma alteração no código.

Se tudo deu certo, você verá uma mensagem no terminal indicando que o servidor está rodando:
```
INFO:     Uvicorn running on [http://127.0.0.1:8000](http://127.0.0.1:8000) (Press CTRL+C to quit)
```

---

## 🧪 Como Usar a API

A maneira mais fácil de testar a API é através da documentação interativa (Swagger UI) que o FastAPI gera automaticamente.

1.  **Acesse a Documentação:** Com o servidor rodando, abra seu navegador e acesse o seguinte endereço:
    [http://127.0.0.1:8000/docs](http://127.0.0.1:8000/docs)

2.  **Selecione o Endpoint:** Você verá o endpoint `POST /detectar-carro/`. Clique nele para expandir os detalhes.

3.  **Abra a Interface de Teste:** Clique no botão cinza **"Try it out"**.

4.  **Envie uma Imagem:** Na seção "Parameters", um botão **"Choose File"** aparecerá. Clique nele para selecionar uma imagem (JPG ou PNG) do seu computador.

5.  **Execute:** Clique no botão azul **"Execute"**. A API irá processar a imagem.

6.  **Veja o Resultado:** Role a página para baixo até a seção **"Server response"**. Lá você encontrará a resposta em formato JSON, indicando se um carro foi detectado (`"car_detected": true/false`) e os detalhes de todos os objetos encontrados.