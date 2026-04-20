# Panic System

Panic System, saha tarafindaki ESP32 cihazlarindan gelen olaylari kaydeden, alarm olusturan ve bunlari web arayuzu uzerinden izlemeyi hedefleyen bir projedir.

Repo su anda uc ana parcadan olusur:

- kokte duran Python backend
- `frontend/` altindaki React + Vite arayuzu
- `firmware/esp32/` altindaki ESP32 firmware kodu

## Klasor Yapisi

```text
panic_system/
|- api.py
|- services.py
|- database.py
|- alarm_okuyucu.py
|- zones.json
|- frontend/
|  |- src/
|  |- public/
|  |- package.json
|- firmware/
|  |- esp32/
|     |- sketch_apr20a/
|        |- sketch_apr20a.ino
```

## Backend

Backend FastAPI uzerinde calisir ve alarmlar ile event kayitlarini yonetir.

Temel dosyalar:

- `api.py`: API endpoint'leri
- `services.py`: alarm ve event islemleri
- `database.py`: SQLite baglantisi ve tablo kurulumu
- `alarm_okuyucu.py`: seri porttan veri okuyup log ve veritabani kaydi olusturan dinleyici
- `zones.json`: zone ile magaza eslestirmeleri

Calistirma ornegi:

```bash
python -m venv venv
venv\Scripts\activate
pip install fastapi uvicorn pydantic pyserial
uvicorn api:app --reload --host 0.0.0.0 --port 8000
```

Faydali endpoint'ler:

- `GET /health`
- `GET /alarms/open`
- `GET /alarms/closed`
- `GET /events`
- `POST /device/event`
- `POST /alarms/{alarm_id}/close`

Notlar:

- SQLite veritabani `db/panic.db` altinda olusur.
- Uretilen log dosyalari `logs/` altina yazilir.
- `db/*.db` ve `logs/*.csv` dosyalari git'e dahil edilmez.

## Frontend

Frontend, backend API ile haberlesen React + TypeScript + Vite uygulamasidir.

Calistirma:

```bash
cd frontend
npm install
npm run dev
```

Uretim build'i:

```bash
cd frontend
npm run build
```

Frontend icin ayrintili notlar `frontend/README.md` icindedir.

## Firmware

ESP32 firmware dosyasi repo icinde `firmware/esp32/sketch_apr20a/` altinda tutulur.

Bu dosya Arduino IDE ile acilip ESP32 cihaza tekrar yuklenebilir.

Firmware tarafindaki baslica ozellikler:

- W5500 Ethernet kullanimi
- DHCP veya statik IP ayari
- Basic Auth ile korunan yonetim paneli
- `/api/status`, `/api/reboot`, `/api/reset` endpoint'leri
- cihaz ayarlarini `Preferences` ile saklama
- reset butonuyla fabrika ayarina donus

Yukleme notu:

1. Arduino IDE'de `firmware/esp32/sketch_apr20a/sketch_apr20a.ino` dosyasini acin.
2. Dogru ESP32 kartini ve portunu secin.
3. Gerekli kutuphanelerin kurulu oldugunu kontrol edin.
4. Kodu cihaza yukleyin.

## Bu Repoda Ne Var

Bu repoya su bolumler birlikte eklendi:

- backend kaynak kodu
- frontend uygulamasi
- ESP32 firmware dosyasi

Boylece sistemin ana parcasi tek repoda birlikte tutuluyor.
