FROM node:20-slim AS builder

WORKDIR /app

COPY index.html .

RUN npm init -y && \
    npm install tailwindcss@3 @fontsource/fira-code

RUN printf "@tailwind base;\n@tailwind components;\n@tailwind utilities;\n" > input.css

RUN npx tailwindcss init && \
    sed -i "s|content: \[\]|content: \['./index.html'\]|" tailwind.config.js

RUN mkdir static && \
    npx tailwindcss -i input.css -o static/style.css --minify

RUN cp -r node_modules/@fontsource/fira-code static/fira-code

RUN sed -i 's|<script src="https://cdn.tailwindcss.com"></script>|<link href="/static/style.css" rel="stylesheet">\n    <link href="/static/fira-code/index.css" rel="stylesheet">|g' index.html && \
    sed -i '/@import url.*Fira+Code.*/d' index.html

FROM python:3.11-slim

WORKDIR /app

RUN pip install --no-cache-dir fastapi "uvicorn[standard]" pyserial pyserial-asyncio

COPY --from=builder /app/index.html .
COPY --from=builder /app/static ./static

COPY orchestrator.py .

RUN sed -i '/from fastapi.responses import FileResponse/a from fastapi.staticfiles import StaticFiles' orchestrator.py && \
    sed -i '/app = FastAPI(title="AirChat V2 Orchestrator")/a app.mount("/static", StaticFiles(directory="static"), name="static")' orchestrator.py

EXPOSE 8000

CMD ["uvicorn", "orchestrator:app", "--host", "0.0.0.0", "--port", "8000"]
