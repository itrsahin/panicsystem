import { useEffect, useMemo, useRef, useState } from "react";
import {
  Building2,
  Clock3,
  FileText,
  MapPin,
  RefreshCw,
  Search,
  Volume2,
  VolumeX,
  ShieldAlert,
  CheckCircle2,
  BellRing,
} from "lucide-react";
import { motion, AnimatePresence } from "framer-motion";
import "./App.css";

const API_BASE = "http://127.0.0.1:8000";

type Alarm = {
  id: number;
  zone: string;
  store_name?: string;
  floor?: string;
  block?: string;
  alarm_type: string;
  status: string;
  opened_at: string;
  closed_at?: string | null;
  close_note?: string | null;
};

type EventItem = {
  id: number;
  zone: string;
  event?: string;
  event_type?: string;
  time?: string;
  event_time?: string;
  device?: string;
  ip?: string;
  raw_data?: string;
};

function formatDateTime(value?: string | null) {
  if (!value) return "-";
  const parsed = new Date(value.replace(" ", "T"));
  if (Number.isNaN(parsed.getTime())) return value;
  return parsed.toLocaleString("tr-TR");
}

function getAlarmLabel(alarmType: string) {
  if (alarmType === "PANIC") return "Panik";
  if (alarmType === "LINE_BREAK") return "Hat Kopuk";
  return alarmType;
}

function getEventLabel(item: EventItem) {
  const e = (item.event || item.event_type || "").toUpperCase();
  if (e === "PANIC_TRIGGER") return "Panik tetiklendi";
  if (e === "LINE_BREAK") return "Hat koptu";
  if (e === "LINE_OK") return "Hat normale döndü";
  return e || "Bilinmeyen olay";
}

function getEventTime(item: EventItem) {
  return item.time || item.event_time || "";
}

function secondsSince(dateStr?: string | null) {
  if (!dateStr) return 0;
  const dt = new Date(dateStr.replace(" ", "T"));
  if (Number.isNaN(dt.getTime())) return 0;
  return Math.max(0, Math.floor((Date.now() - dt.getTime()) / 1000));
}

function formatDuration(totalSeconds: number) {
  const h = Math.floor(totalSeconds / 3600);
  const m = Math.floor((totalSeconds % 3600) / 60);
  const s = totalSeconds % 60;

  if (h > 0) return `${h} sa ${m} dk`;
  if (m > 0) return `${m} dk ${s} sn`;
  return `${s} sn`;
}

export default function App() {
  const [openAlarms, setOpenAlarms] = useState<Alarm[]>([]);
  const [closedAlarms, setClosedAlarms] = useState<Alarm[]>([]);
  const [events, setEvents] = useState<EventItem[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");
  const [alarmQuery, setAlarmQuery] = useState("");
  const [eventQuery, setEventQuery] = useState("");
  const [selectedOpenAlarm, setSelectedOpenAlarm] = useState<Alarm | null>(null);
  const [selectedClosedAlarm, setSelectedClosedAlarm] = useState<Alarm | null>(null);
  const [closeNote, setCloseNote] = useState("");
  const [closing, setClosing] = useState(false);
  const [muted, setMuted] = useState(false);
  const [tick, setTick] = useState(0);

  const previousOpenCount = useRef(0);
  const audioRef = useRef<HTMLAudioElement | null>(null);

  useEffect(() => {
    audioRef.current = new Audio("/alarm.mp3");
    audioRef.current.preload = "auto";
  }, []);

  useEffect(() => {
    const t = setInterval(() => setTick((x) => x + 1), 1000);
    return () => clearInterval(t);
  }, []);

  const fetchData = async () => {
    setLoading(true);
    setError("");

    try {
      const [openRes, closedRes, eventsRes] = await Promise.all([
        fetch(`${API_BASE}/alarms/open`),
        fetch(`${API_BASE}/alarms/closed?limit=50`),
        fetch(`${API_BASE}/events?limit=50`),
      ]);

      if (!openRes.ok) throw new Error("Açık alarmlar alınamadı.");
      if (!closedRes.ok) throw new Error("Kapanan alarmlar alınamadı.");
      if (!eventsRes.ok) throw new Error("Event listesi alınamadı.");

      const openJson = await openRes.json();
      const closedJson = await closedRes.json();
      const eventsJson = await eventsRes.json();

      const newOpen = Array.isArray(openJson) ? openJson : [];
      const newClosed = Array.isArray(closedJson) ? closedJson : [];
      const newEvents = Array.isArray(eventsJson) ? eventsJson : [];

      if (previousOpenCount.current < newOpen.length && !muted && audioRef.current) {
        audioRef.current.currentTime = 0;
        audioRef.current.play().catch(() => {});
      }
      previousOpenCount.current = newOpen.length;

      setOpenAlarms(newOpen);
      setClosedAlarms(newClosed);
      setEvents(newEvents);
    } catch (err) {
      setError(err instanceof Error ? err.message : "Bilinmeyen hata oluştu.");
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchData();
    const timer = setInterval(fetchData, 5000);
    return () => clearInterval(timer);
  }, [muted]);

  const filteredOpenAlarms = useMemo(() => {
    const q = alarmQuery.trim().toLowerCase();
    if (!q) return openAlarms;
    return openAlarms.filter((alarm) =>
      [
        alarm.store_name,
        alarm.zone,
        alarm.floor,
        alarm.block,
        alarm.alarm_type,
        alarm.status,
      ]
        .join(" ")
        .toLowerCase()
        .includes(q)
    );
  }, [openAlarms, alarmQuery]);

  const filteredClosedAlarms = useMemo(() => {
    const q = alarmQuery.trim().toLowerCase();
    if (!q) return closedAlarms;
    return closedAlarms.filter((alarm) =>
      [
        alarm.store_name,
        alarm.zone,
        alarm.floor,
        alarm.block,
        alarm.alarm_type,
        alarm.status,
        alarm.close_note,
      ]
        .join(" ")
        .toLowerCase()
        .includes(q)
    );
  }, [closedAlarms, alarmQuery]);

  const filteredEvents = useMemo(() => {
    const q = eventQuery.trim().toLowerCase();
    if (!q) return events;
    return events.filter((item) =>
      [
        item.zone,
        item.event,
        item.event_type,
        item.device,
        item.ip,
        item.raw_data,
      ]
        .join(" ")
        .toLowerCase()
        .includes(q)
    );
  }, [events, eventQuery]);

  const panicCount = openAlarms.filter((a) => a.alarm_type === "PANIC").length;
  const lineCount = openAlarms.filter((a) => a.alarm_type === "LINE_BREAK").length;

  const handleCloseAlarm = async () => {
    if (!selectedOpenAlarm || !closeNote.trim()) return;

    setClosing(true);
    setError("");

    try {
      const res = await fetch(`${API_BASE}/alarms/${selectedOpenAlarm.id}/close`, {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({ note: closeNote.trim() }),
      });

      if (!res.ok) {
        const data = await res.json().catch(() => null);
        throw new Error(data?.detail || "Alarm kapatılamadı.");
      }

      setSelectedOpenAlarm(null);
      setCloseNote("");
      await fetchData();
    } catch (err) {
      setError(err instanceof Error ? err.message : "Alarm kapatma hatası.");
    } finally {
      setClosing(false);
    }
  };

  return (
    <div className="page">
      <div className="container">
        <div className="topbar">
          <div>
            <h1>Panik Alarm Yönetimi</h1>
            <p>Aktif panikleri izle, mağazayı tespit et, açıklama ile kapat ve geçmişi görüntüle.</p>
          </div>

          <div className="topbar-actions">
            <button
              className="btn btn-secondary"
              onClick={() => setMuted((v) => !v)}
              title={muted ? "Sesi aç" : "Sesi kapat"}
            >
              {muted ? <VolumeX size={16} /> : <Volume2 size={16} />}
              {muted ? "Sessiz" : "Ses Açık"}
            </button>

            <button className="btn btn-primary" onClick={fetchData} disabled={loading}>
              <RefreshCw size={16} className={loading ? "spin" : ""} />
              Yenile
            </button>
          </div>
        </div>

        {error ? <div className="error-box">{error}</div> : null}

        <div className="stats-grid">
          <div className="stat-card">
            <div className="stat-head">
              <ShieldAlert size={18} />
              <span className="stat-label">Açık Alarm</span>
            </div>
            <div className="stat-value">{openAlarms.length}</div>
            <div className="stat-note">Şu anda operatör müdahalesi bekleyen kayıtlar</div>
          </div>

          <div className="stat-card danger">
            <div className="stat-head">
              <BellRing size={18} />
              <span className="stat-label">Açık Panik</span>
            </div>
            <div className="stat-value danger-text">{panicCount}</div>
            <div className="stat-note">Öncelikli güvenlik alarmı</div>
          </div>

          <div className="stat-card ok">
            <div className="stat-head">
              <CheckCircle2 size={18} />
              <span className="stat-label">Kapanan Alarm</span>
            </div>
            <div className="stat-value success-text">{closedAlarms.length}</div>
            <div className="stat-note">Liste üzerinde son kapanan kayıtlar gösteriliyor</div>
          </div>
        </div>

        <div className="content-grid">
          <div className="panel">
            <div className="panel-header">
              <h2>Açık Alarmlar</h2>
              <p>Kırmızı kartlar aktif alarmı gösterir. Aynı arama kutusu kapanan alarmlarda da çalışır.</p>
            </div>

            <div className="panel-toolbar">
              <div className="search-box">
                <Search size={16} />
                <input
                  type="text"
                  placeholder="Açık/kapanan alarmlarda ara"
                  value={alarmQuery}
                  onChange={(e) => setAlarmQuery(e.target.value)}
                />
              </div>
            </div>

            <div className="panel-body">
              {filteredOpenAlarms.length === 0 ? (
                <div className="empty-box">Açık alarm bulunmuyor.</div>
              ) : (
                <div className="alarm-list">
                  {filteredOpenAlarms.map((alarm) => {
                    const isPanic = alarm.alarm_type === "PANIC";
                    const elapsed = formatDuration(secondsSince(alarm.opened_at) + tick - tick);

                    return (
                      <motion.div
                        key={alarm.id}
                        initial={{ opacity: 0, y: 6 }}
                        animate={{ opacity: 1, y: 0 }}
                        className={`alarm-card alarm-card-danger ${isPanic ? "alarm-blink" : ""}`}
                      >
                        <div className="alarm-main">
                          <div className="alarm-tags">
                            <span className="badge badge-danger">🔴 AÇIK</span>
                            <span className={`badge ${isPanic ? "badge-danger" : "badge-default"}`}>
                              {getAlarmLabel(alarm.alarm_type)}
                            </span>
                            <span className="badge badge-outline">Zone {alarm.zone}</span>
                          </div>

                          <h3 className="danger-text">{alarm.store_name || `Zone ${alarm.zone}`}</h3>

                          <div className="alarm-meta">
                            <span><Building2 size={15} /> {alarm.block || "-"}</span>
                            <span><MapPin size={15} /> {alarm.floor || "-"}</span>
                            <span><Clock3 size={15} /> {formatDateTime(alarm.opened_at)}</span>
                            <span><BellRing size={15} /> {elapsed} aktif</span>
                          </div>
                        </div>

                        <div className="alarm-actions">
                          <button
                            className="btn btn-danger"
                            onClick={() => {
                              setSelectedOpenAlarm(alarm);
                              setCloseNote("");
                            }}
                          >
                            Alarmı Kapat
                          </button>
                        </div>
                      </motion.div>
                    );
                  })}
                </div>
              )}
            </div>
          </div>

          <div className="panel">
            <div className="panel-header">
              <h2>İşlem Geçmişi</h2>
              <p>JSON yerine okunabilir formatta son olaylar gösterilir. Bu kutuda ayrı arama var.</p>
            </div>

            <div className="panel-toolbar">
              <div className="search-box">
                <Search size={16} />
                <input
                  type="text"
                  placeholder="Event geçmişinde ara"
                  value={eventQuery}
                  onChange={(e) => setEventQuery(e.target.value)}
                />
              </div>
            </div>

            <div className="panel-body compact-body">
              {filteredEvents.length === 0 ? (
                <div className="empty-box">Event bulunmuyor.</div>
              ) : (
                <div className="event-list">
                  {filteredEvents.map((item) => (
                    <div key={item.id} className="event-card">
                      <div className="event-top">
                        <div className="event-title">
                          <strong>Zone {item.zone}</strong> → {getEventLabel(item)}
                        </div>
                        <span className="badge badge-outline">{formatDateTime(getEventTime(item))}</span>
                      </div>

                      <div className="event-sub">
                        <span><FileText size={14} /> {item.device || "Cihaz yok"}</span>
                        <span>{item.ip || "-"}</span>
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </div>
          </div>
        </div>

        <div className="bottom-grid">
          <div className="panel">
            <div className="panel-header">
              <h2>Kapanan Alarmlar</h2>
              <p>Yeşil etiketli kayıtlar. Tıklayınca kapanış notu açılır.</p>
            </div>

            <div className="panel-body compact-body">
              {filteredClosedAlarms.length === 0 ? (
                <div className="empty-box">Kapanan alarm yok.</div>
              ) : (
                <div className="alarm-list">
                  {filteredClosedAlarms.map((alarm) => (
                    <div
                      key={alarm.id}
                      className="alarm-card alarm-card-closed clickable"
                      onClick={() => setSelectedClosedAlarm(alarm)}
                    >
                      <div className="alarm-main">
                        <div className="alarm-tags">
                          <span className="badge badge-success">🟢 KAPALI</span>
                          <span className="badge badge-outline">{getAlarmLabel(alarm.alarm_type)}</span>
                          <span className="badge badge-outline">Zone {alarm.zone}</span>
                        </div>

                        <h3 className="success-text">{alarm.store_name || `Zone ${alarm.zone}`}</h3>

                        <div className="alarm-meta">
                          <span><Building2 size={15} /> {alarm.block || "-"}</span>
                          <span><MapPin size={15} /> {alarm.floor || "-"}</span>
                          <span><Clock3 size={15} /> {formatDateTime(alarm.closed_at || alarm.opened_at)}</span>
                        </div>
                      </div>
                    </div>
                  ))}
                </div>
              )}
            </div>
          </div>

          <div className="panel">
            <div className="panel-header">
              <h2>Durum Özeti</h2>
              <p>Kısa operatör görünümü.</p>
            </div>
            <div className="panel-body">
              <div className="summary-list">
                <div className="summary-row">
                  <span>Açık panik</span>
                  <strong>{panicCount}</strong>
                </div>
                <div className="summary-row">
                  <span>Açık hat kopuk</span>
                  <strong>{lineCount}</strong>
                </div>
                <div className="summary-row">
                  <span>Kapanan alarm</span>
                  <strong>{closedAlarms.length}</strong>
                </div>
                <div className="summary-row">
                  <span>Event kaydı</span>
                  <strong>{events.length}</strong>
                </div>
              </div>
            </div>
          </div>
        </div>

        <AnimatePresence>
          {selectedOpenAlarm && (
            <div className="modal-overlay" onClick={() => setSelectedOpenAlarm(null)}>
              <motion.div
                className="modal"
                onClick={(e) => e.stopPropagation()}
                initial={{ opacity: 0, y: 12 }}
                animate={{ opacity: 1, y: 0 }}
                exit={{ opacity: 0, y: 12 }}
              >
                <div className="modal-header">
                  <h2>Alarm Kapat</h2>
                  <p>Alarmı kapatmadan önce açıklama gir. Bu bilgi veritabanında saklanır.</p>
                </div>

                <div className="modal-info">
                  <div className="modal-store">{selectedOpenAlarm.store_name || `Zone ${selectedOpenAlarm.zone}`}</div>
                  <div className="modal-meta">
                    Zone {selectedOpenAlarm.zone} • {selectedOpenAlarm.block || "-"} • {selectedOpenAlarm.floor || "-"}
                  </div>
                  <div className="modal-meta">
                    Alarm: {getAlarmLabel(selectedOpenAlarm.alarm_type)}
                  </div>
                </div>

                <label className="textarea-label">Açıklama</label>
                <textarea
                  className="textarea"
                  value={closeNote}
                  onChange={(e) => setCloseNote(e.target.value)}
                  placeholder="Örn: Mağaza ile görüşüldü, yanlış alarm. Güvenlik yerinde kontrol etti."
                />

                <div className="modal-actions">
                  <button className="btn btn-secondary" onClick={() => setSelectedOpenAlarm(null)}>
                    Vazgeç
                  </button>
                  <button
                    className="btn btn-primary"
                    onClick={handleCloseAlarm}
                    disabled={closing || !closeNote.trim()}
                  >
                    {closing ? "Kaydediliyor..." : "Kaydet ve Kapat"}
                  </button>
                </div>
              </motion.div>
            </div>
          )}

          {selectedClosedAlarm && (
            <div className="modal-overlay" onClick={() => setSelectedClosedAlarm(null)}>
              <motion.div
                className="modal"
                onClick={(e) => e.stopPropagation()}
                initial={{ opacity: 0, y: 12 }}
                animate={{ opacity: 1, y: 0 }}
                exit={{ opacity: 0, y: 12 }}
              >
                <div className="modal-header">
                  <h2>Kapanış Detayı</h2>
                  <p>Kapanan alarm için operatör notu.</p>
                </div>

                <div className="modal-info">
                  <div className="modal-store">{selectedClosedAlarm.store_name || `Zone ${selectedClosedAlarm.zone}`}</div>
                  <div className="modal-meta">
                    Zone {selectedClosedAlarm.zone} • {selectedClosedAlarm.block || "-"} • {selectedClosedAlarm.floor || "-"}
                  </div>
                  <div className="modal-meta">
                    Alarm: {getAlarmLabel(selectedClosedAlarm.alarm_type)}
                  </div>
                  <div className="modal-meta">
                    Kapanış: {formatDateTime(selectedClosedAlarm.closed_at)}
                  </div>
                </div>

                <label className="textarea-label">Açıklama</label>
                <div className="note-box">
                  {selectedClosedAlarm.close_note || "Açıklama girilmemiş."}
                </div>

                <div className="modal-actions">
                  <button className="btn btn-primary" onClick={() => setSelectedClosedAlarm(null)}>
                    Kapat
                  </button>
                </div>
              </motion.div>
            </div>
          )}
        </AnimatePresence>
      </div>
    </div>
  );
}