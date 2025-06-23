import io
from fastapi import FastAPI, File, UploadFile, HTTPException
from PIL import Image
from transformers import pipeline
import json

print("Carregando o modelo de detecção de objetos...")
detector = pipeline("object-detection", model="facebook/detr-resnet-50")
print("Modelo carregado com sucesso!")


app = FastAPI(
    title="API de Detecção de Carros",
    description="Faça upload de uma imagem e a API dirá se um carro foi detectado.",
    version="1.0.0"
)

def analisar_imagem(img: Image.Image, confianca_minima: float = 0.9):
    if img.mode != "RGB":
        img = img.convert("RGB")
        
    resultados_raw = detector(img)
    
    resultados_limpos = [
        {
            "score": round(obj["score"], 4),
            "label": obj["label"],
            "box": obj["box"]
        } for obj in resultados_raw
    ]

    carro_detectado = False
    for obj in resultados_limpos:
        if obj['label'] == 'car' and obj['score'] >= confianca_minima:
            carro_detectado = True
            break
            
    return {"car_detected": carro_detectado, "details": resultados_limpos}


@app.post("/detectar-carro/", summary="Detecta objetos em uma imagem enviada")
async def detectar_objetos_em_imagem(file: UploadFile = File(...)):

    if file.content_type not in ["image/jpeg", "image/png"]:
        raise HTTPException(status_code=400, detail="Tipo de arquivo inválido. Por favor, envie uma imagem JPG ou PNG.")
        
    try:
        conteudo_arquivo = await file.read()

        imagem = Image.open(io.BytesIO(conteudo_arquivo))
        
        resultado_analise = analisar_imagem(imagem)
        
        return resultado_analise

    except Exception as e:
        raise HTTPException(status_code=500, detail=f"Ocorreu um erro ao processar a imagem: {e}")


@app.get("/")
def read_root():
    return {"status": "API online. Acesse /docs para ver a documentação."}