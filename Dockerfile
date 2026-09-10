FROM python:3.13-slim
WORKDIR /app
COPY requirements.txt .
RUN python -m pip install --no-cache-dir -r requirements.txt
COPY src ./src
CMD ["python", "-m", "uvicorn", "src.iot_receiver.api:app", "--host", "0.0.0.0", "--port", "8000"]
