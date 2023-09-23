import logging.config
import uvicorn
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from app import chatglm, porcupine

logging.config.fileConfig('logging_config.ini')

app = FastAPI(lifespan=chatglm.lifespan)


app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ChatGLM2B OpenAPI Style RESTFul API
app.include_router(chatglm.router)

# Porcupine Websockets Server
app.include_router(porcupine.router)


uvicorn.run(app, host='0.0.0.0', port=8000, workers=1)


