from fastapi import FastAPI, HTTPException
import sqlite3
from src.iot_receiver.mqtt_receiver import DATABASE_PATH

app = FastAPI()

@app.get("/health")
def health():
    return {"status": "ok"}

@app.get("/api/v1/readings/latest")
def latest_reading():
    connection = sqlite3.connect(DATABASE_PATH)
    connection.row_factory = sqlite3.Row
    cursor = connection.cursor()
    sql = """
        SELECT id, sensor_id AS sensorId, timestamp, value, unit
        FROM readings
        ORDER BY id DESC
        LIMIT 1
    """
    cursor.execute(sql)
    reading = cursor.fetchone()
    connection.close()
    if reading is None:
        raise HTTPException(status_code=404, detail="Reading not available.")
    return dict(reading)

@app.get("/api/v1/readings")
def get_readings(limit: int = 100):
    connection = sqlite3.connect(DATABASE_PATH)
    connection.row_factory = sqlite3.Row
    cursor = connection.cursor()
    sql = """
        SELECT id, sensor_id AS sensorId, timestamp, value, unit
        FROM readings
        ORDER BY id DESC
        LIMIT ?
    """
    cursor.execute(sql, (limit,))
    readings = cursor.fetchall()
    connection.close()
    result_list = []
    for reading in readings:
        result = dict(reading)
        result_list.append(result)
    return result_list
