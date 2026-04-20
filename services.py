from database import get_connection
import json

with open("zones.json", "r", encoding="utf-8") as f:
    ZONES = json.load(f)


def insert_event(zone, event, raw, device_name=None, ip=None):
    conn = get_connection()
    cursor = conn.cursor()

    cursor.execute("""
        INSERT INTO events (zone, event_type, event_time, raw_data, device_name, ip)
        VALUES (?, ?, datetime('now'), ?, ?, ?)
    """, (zone, event, raw, device_name, ip))

    conn.commit()
    conn.close()

def get_open_alarm(zone, alarm_type):
    conn = get_connection()
    cursor = conn.cursor()

    cursor.execute("""
        SELECT id, zone, alarm_type, status, opened_at, closed_at, close_note
        FROM alarms
        WHERE zone = ? AND alarm_type = ? AND status = 'OPEN'
    """, (zone, alarm_type))

    result = cursor.fetchone()
    conn.close()
    return result

def create_alarm(zone, alarm_type="PANIC"):
    conn = get_connection()
    cursor = conn.cursor()

    cursor.execute("""
        INSERT INTO alarms (zone, alarm_type, status, opened_at)
        VALUES (?, ?, 'OPEN', datetime('now'))
    """, (zone, alarm_type))

    conn.commit()
    conn.close()

def list_open_alarms():
    conn = get_connection()
    cursor = conn.cursor()

    cursor.execute("""
        SELECT id, zone, alarm_type, status, opened_at, closed_at, close_note
        FROM alarms
        WHERE status = 'OPEN'
        ORDER BY id DESC
    """)

    rows = cursor.fetchall()
    conn.close()

    results = []

    for row in rows:
        zone = row[1]
        zone_info = ZONES.get(zone, {})

        results.append({
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

    return results


def list_events():
    conn = get_connection()
    cursor = conn.cursor()

    cursor.execute("""
        SELECT id, zone, event_type, event_time, device_name, ip
        FROM events
        ORDER BY id DESC
        LIMIT 50
    """)

    rows = cursor.fetchall()
    conn.close()

    return [
        {
            "id": r[0],
            "zone": r[1],
            "event": r[2],
            "time": r[3],
            "device": r[4],
            "ip": r[5]
        }
        for r in rows
    ]

def close_alarm(alarm_id, note):
    conn = get_connection()
    cursor = conn.cursor()

    cursor.execute("""
        UPDATE alarms
        SET status = 'CLOSED',
            closed_at = datetime('now'),
            close_note = ?
        WHERE id = ? AND status = 'OPEN'
    """, (note, alarm_id))

    conn.commit()
    affected = cursor.rowcount
    conn.close()
    return affected