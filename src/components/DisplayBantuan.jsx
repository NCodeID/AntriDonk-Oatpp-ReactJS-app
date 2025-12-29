import React, { useEffect, useState, useRef } from "react";

const WS_URL = "ws://192.168.20.1:8000/ws"; 
const API_STATUS_URL = "http://192.168.20.1:8000/queue/status"; 

export default function RecipientDisplay({ initialNIK = "" }) {
  const [nik, setNik] = useState(initialNIK);
  const [connected, setConnected] = useState(false);
  const [statusMsg, setStatusMsg] = useState("Terhubung: menunggu data");
  const [data, setData] = useState(null); 
  const wsRef = useRef(null);
  const reconnectTimer = useRef(null);

  
  const connectWS = (nikToSubscribe) => {
    if (wsRef.current) {
      try { wsRef.current.close(); } catch {}
      wsRef.current = null;
    }

    try {
      const ws = new WebSocket(WS_URL);
      wsRef.current = ws;

      ws.onopen = () => {
        setConnected(true);
        setStatusMsg("WebSocket terhubung");
        if (nikToSubscribe) {
          ws.send(JSON.stringify({ type: "subscribe", NIK: nikToSubscribe }));
        }
      };

      ws.onmessage = (ev) => {
        try {
          const msg = JSON.parse(ev.data);
          if (msg.type === "update") {
            setData({
              nomorAntrian: msg.nomorAntrian,
              loket: msg.loket,
              status: msg.status,
              message: msg.message || "",
            });
            setStatusMsg(msg.message || "Ada pembaruan");
          } else if (msg.type === "ack") {
            setStatusMsg(msg.message || "Subscribed");
          }
        } catch (e) {
          console.warn("Invalid WS message", e);
        }
      };

      ws.onclose = () => {
        setConnected(false);
        setStatusMsg("WebSocket terputus, mencoba reconnect...");
        if (reconnectTimer.current) clearTimeout(reconnectTimer.current);
        reconnectTimer.current = setTimeout(() => connectWS(nikToSubscribe), 2000);
      };

      ws.onerror = (err) => {
        console.error("WS error", err);
        ws.close();
      };
    } catch (e) {
      console.error("WS connect failed", e);
      setConnected(false);
    }
  };

  const pollStatus = async (nikToQuery) => {
    if (!nikToQuery) return;
    try {
      const res = await fetch(`${API_STATUS_URL}?NIK=${encodeURIComponent(nikToQuery)}`);
      if (!res.ok) return;
      const json = await res.json();
      if (json && json.nomorAntrian !== undefined) {
        setData({
          nomorAntrian: json.nomorAntrian,
          loket: json.loket,
          status: json.status,
          message: json.message || "",
        });
        setStatusMsg(json.message || "Terupdate (polling)");
      }
    } catch (e) {
      console.warn("Polling failed", e);
    }
  };

  useEffect(() => {
    if (nik) connectWS(nik);
    const pollInterval = setInterval(() => {
      if (!connected && nik) pollStatus(nik);
    }, 3000);

    return () => {
      if (wsRef.current) try { wsRef.current.close(); } catch {}
      clearInterval(pollInterval);
      if (reconnectTimer.current) clearTimeout(reconnectTimer.current);
    };
  }, [nik]);

  const primaryRed = "#aa2200";

  return (
    <div className="min-h-screen flex items-center justify-center bg-white p-6">
      <div className="w-full max-w-xl bg-white shadow-md rounded-2xl border" style={{ borderColor: "#f0f0f0" }}>
        <div className="p-6 border-b" style={{ borderColor: "#f7f7f7" }}>
          <h1 className="text-2xl font-bold" style={{ color: primaryRed }}>Layar Penerima Bansos</h1>
          <p className="text-sm text-gray-600 mt-1">Lihat nomor antrian dan loket secara real-time</p>
        </div>

        <div className="p-6">
          <div className="mb-4">
            <label className="block text-sm font-medium text-gray-700">Masukkan NIK</label>
            <div className="mt-2 flex gap-2">
              <input
                value={nik}
                onChange={(e) => setNik(e.target.value)}
                placeholder="3501234..."
                className="flex-1 px-3 py-2 border rounded-md"
              />
              <button
                onClick={() => {
                  if (!nik) return alert("Masukkan NIK untuk subscribe");
                  connectWS(nik);
                  setStatusMsg("Mencoba subscribe...");
                }}
                className="px-4 py-2 rounded-md text-white"
                style={{ background: primaryRed }}
              >
                Subscribe
              </button>
            </div>
            <p className="text-xs text-gray-500 mt-2">Status koneksi: <strong>{connected ? "Terhubung" : "Tidak terhubung"}</strong> — {statusMsg}</p>
          </div>

          <div className="mt-6">
            <div className="text-xs text-gray-500 mb-2">Nomor Antrian</div>
            <div className="flex items-center gap-6">
              <div className="flex-1 bg-black text-white rounded-xl p-6 text-center">
                <div className="text-sm opacity-80">Nomor Anda</div>
                <div className="text-5xl font-extrabold mt-2">{data?.nomorAntrian ?? "-"}</div>
              </div>

              <div className="w-40 bg-white border rounded-xl p-4 text-center" style={{ borderColor: "#eee" }}>
                <div className="text-sm text-gray-500">Loket</div>
                <div className="text-3xl font-bold" style={{ color: primaryRed }}>{data?.loket ?? "-"}</div>
              </div>
            </div>

            <div className="mt-4 p-4 rounded-lg" style={{ background: "#fff7f6", border: "1px solid rgba(170,34,0,0.06)" }}>
              <div className="text-sm text-gray-700 font-medium">Status</div>
              <div className="mt-1 text-sm text-gray-800">{data?.message ?? "Menunggu panggilan..."}</div>
            </div>
          </div>

          <div className="mt-6 flex gap-3">
            <button
              onClick={() => {
                // manual refresh via polling
                if (!nik) return alert("Masukkan NIK terlebih dahulu");
                pollStatus(nik);
                setStatusMsg("Meminta update via polling...");
              }}
              className="flex-1 py-2 rounded-md text-white"
              style={{ background: primaryRed }}
            >
              Refresh
            </button>
            <button
              onClick={() => {
                if (wsRef.current) {
                  try { wsRef.current.close(); } catch {}
                }
                setConnected(false);
                setStatusMsg("Disconnected");
              }}
              className="py-2 px-4 rounded-md border"
            >
              Disconnect
            </button>
          </div>
        </div>

        <div className="p-4 border-t text-xs text-gray-500" style={{ borderColor: "#f7f7f7" }}>
          Tips: Pastikan layar ini tetap terbuka agar notifikasi panggilan diterima.
        </div>
      </div>
    </div>
  );
}
