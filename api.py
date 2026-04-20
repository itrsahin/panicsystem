from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
from database import init_db
from services import list_open_alarms, list_events, close_alarm
from fastapi.middleware.cors import CORSMiddleware

from services import insert_event, get_open_alarm, create_alarm

import json
from database import get_connection


app = FastAPI(title="Panic System API", version="1.0.0")

init_db()




app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

class CloseAlarmRequest(BaseModel):
    note: str



class DeviceEventRequest(BaseModel):
    device_name: str
    ip: str
    zone: int
    event: str

@app.post("/device/event")
def device_event(payload: DeviceEventRequest):
    zone = str(payload.zone)
    event = payload.event.strip().upper()

    raw_data = (
        f'{{"device_name":"{payload.device_name}",'
        f'"ip":"{payload.ip}",'
        f'"zone":{payload.zone},'
        f'"event":"{event}"}}'
    )

    insert_event(zone, event, raw_data, payload.device_name, payload.ip)

    if event == "PANIC_TRIGGER":
        open_alarm = get_open_alarm(zone, "PANIC")

        if open_alarm:
            return {
                "message": "Bu zone icin zaten acik panic alarmi var",
                "zone": zone,
                "alarm_created": False
            }

        create_alarm(zone, "PANIC")
        return {
            "message": "Yeni panic alarmi acildi",
            "zone": zone,
            "alarm_created": True
        }

    elif event == "LINE_BREAK":
        open_alarm = get_open_alarm(zone, "LINE_BREAK")

        if open_alarm:
            return {
                "message": "Bu zone icin zaten acik line break alarmi var",
                "zone": zone,
                "alarm_created": False
            }

        create_alarm(zone, "LINE_BREAK")
        return {
            "message": "Yeni line break alarmi acildi",
            "zone": zone,
            "alarm_created": True
        }

    return {
        "message": "Event kaydedildi",
        "zone": zone,
        "alarm_created": False
    }

@app.get("/")
def root():
    return {"message": "Panic System API calisiyor"}


@app.get("/health")
def health():
    return {"status": "ok"}


@app.get("/alarms/open")
def get_open_alarms():
    return list_open_alarms()


@app.get("/events")
def get_events(limit: int = 50):
    conn = get_connection()
    cursor = conn.cursor()

    cursor.execute("""
        SELECT id, zone, event_type, event_time, device_name, ip
        FROM events
        ORDER BY id DESC
        LIMIT ?
    """, (limit,))

    rows = cursor.fetchall()
    conn.close()

    return [
        {
            "id": r[0],
            "zone": str(r[1]),
            "event": r[2],
            "time": r[3],
            "device": r[4],
            "ip": r[5]
        }
        for r in rows
    ]


@app.post("/alarms/{alarm_id}/close")
def api_close_alarm(alarm_id: int, payload: CloseAlarmRequest):
    affected = close_alarm(alarm_id, payload.note)

    if affected == 0:
        raise HTTPException(status_code=404, detail="Acik alarm bulunamadi")

    return {
        "message": "Alarm kapatildi",
        "alarm_id": alarm_id,
        "note": payload.note
    }


@app.get("/alarms/closed")
def get_closed_alarms(limit: int = 50):
    conn = get_connection()
    cursor = conn.cursor()

    cursor.execute("""
        SELECT id, zone, alarm_type, status, opened_at, closed_at, close_note
        FROM alarms
        WHERE status = 'CLOSED'
        ORDER BY id DESC
        LIMIT ?
    """, (limit,))

    rows = cursor.fetchall()
    conn.close()

    with open("zones.json", "r", encoding="utf-8") as f:
        zones = json.load(f)

    result = []
    for row in rows:
        zone = str(row[1])
        zone_info = zones.get(zone, {})

        result.append({
            "id": row[0],
            "zone": zone,
            "store_name": zone_info.get("store_name", "Bilinmeyen"),
            "floor": zone_info.get("floor", ""),
            "block": zone_info.get("block", ""),
            "alarm_type": row[2],
            "status": row[3],
            "opened_at": row[4],
            "closed_at": row[5],
            "close_note": row[6]
        })

    return result