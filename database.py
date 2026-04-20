import sqlite3
from pathlib import Path

DB_PATH = Path("db/panic.db")

def get_connection():
    return sqlite3.connect(DB_PATH)

def init_db():
    conn = get_connection()
    cursor = conn.cursor()

    cursor.execute("""
    CREATE TABLE IF NOT EXISTS events (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        zone TEXT,
        event_type TEXT,
        event_time TEXT,
        raw_data TEXT
    )
    """)

    cursor.execute("""
    CREATE TABLE IF NOT EXISTS alarms (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        zone TEXT,
        alarm_type TEXT,
        status TEXT,
        opened_at TEXT,
        closed_at TEXT,
        close_note TEXT
    )
    """)

    conn.commit()
    conn.close()