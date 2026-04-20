import json
import serial
from datetime import datetime
from pathlib import Path
from services import insert_event, get_open_alarm, create_alarm
from database import init_db

PORT = "COM5"      # Kendi COM portunu yaz
BAUD = 115200

init_db()


# Zone bilgilerini oku
with open("zones.json", "r", encoding="utf-8") as f:
    zones = json.load(f)

# Log klasörü oluştur
log_dir = Path("logs")
log_dir.mkdir(exist_ok=True)

# Günlük log dosyası
log_file = log_dir / f"events_{datetime.now().strftime('%Y%m%d')}.csv"

# Dosya yoksa başlık yaz
if not log_file.exists():
    with open(log_file, "w", encoding="utf-8") as f:
        f.write("timestamp,zone,store_code,store_name,floor,block,event\n")

# Seri port aç
ser = serial.Serial(PORT, BAUD, timeout=1)

print("Alarm dinleniyor...")



while True:
    try:
        line = ser.readline().decode(errors="ignore").strip()

        if not line:
            continue

        print("Ham veri:", line)

        # JSON mu kontrol et
        try:
            data = json.loads(line)
        except json.JSONDecodeError:
            print("JSON degil, gecildi.")
            continue

        # event + zone zorunlu
        if "event" not in data or "zone" not in data:
            print("Eksik veri, gecildi.")
            continue

        event = str(data["event"]).strip().upper()
        zone = str(data["zone"]).strip()
        zaman = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

        # Zone bilgisi var mı
        zone_info = zones.get(zone)
        if zone_info is None:
            print(f"[{zaman}] Bilinmeyen Zone {zone} -> {event}")
            continue



        store_code = zone_info.get("store_code", "")
        store_name = zone_info.get("store_name", f"Zone {zone}")
        floor = zone_info.get("floor", "")
        block = zone_info.get("block", "")

        # Konsola yaz
        print(f"[{zaman}] {store_name} / {floor} / {block} -> {event}")

        # CSV log
        
        with open(log_file, "a", encoding="utf-8") as f:
            f.write(
                f"{zaman},{zone},{store_code},{store_name},{floor},{block},{event}\n"
            )

        # DB'ye event kaydet
        insert_event(zone, event, line)

        # Alarm yönetimi
        if event == "PANIC_TRIGGER":
            open_alarm = get_open_alarm(zone, "PANIC")

            if open_alarm:
                print(f"Zone {zone} icin zaten acik panic alarmi var, tekrar acilmadi.")
            else:
                create_alarm(zone, "PANIC")
                print(f"🔥 Zone {zone} icin yeni panic alarmi acildi.")

        elif event == "LINE_BREAK":
            open_alarm = get_open_alarm(zone, "LINE_BREAK")

            if open_alarm:
                print(f"Zone {zone} icin zaten acik line break alarmi var, tekrar acilmadi.")
            else:
                create_alarm(zone, "LINE_BREAK")
                print(f"🛠️ Zone {zone} icin yeni hat kopuk alarmi acildi.")

    except serial.SerialException as e:
        print("Seri port hatasi:", e)
        break

    except KeyboardInterrupt:
        print("\nProgram durduruldu.")
        break

    except Exception as e:
        print("Beklenmeyen hata:", e)