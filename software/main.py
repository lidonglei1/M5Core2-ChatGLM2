import logging.config
from traceback import print_exception

import uvicorn
from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from starlette.requests import Request
from starlette.responses import Response

from app import chatglm, porcupine


"""
Swagger UI：http://localhost:8000/docs
ReDoc：http://localhost:8000/redoc
"""

logging.config.fileConfig('logging_config.ini')

app = FastAPI(lifespan=chatglm.lifespan)


async def catch_exceptions_middleware(request: Request, call_next):
    try:
        return await call_next(request)
    except Exception as e:
        # logging here
        print_exception(type(e), e, e.__traceback__)
        return Response("Internal server error", status_code=500)


app.middleware('http')(catch_exceptions_middleware)

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

uvicorn.run(app, host='0.0.0.0', port=8000, workers=1, log_level=logging.DEBUG)
