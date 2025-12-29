import { useEffect, useRef } from "react";

export default function useQueueSocket({ onStatus, onUpdate }) {
  const wsRef = useRef(null);
  const onStatusRef = useRef(onStatus);
  const onUpdateRef = useRef(onUpdate);

  // simpan callback terbaru
  useEffect(() => {
    onStatusRef.current = onStatus;
    onUpdateRef.current = onUpdate;
  }, [onStatus, onUpdate]);

  useEffect(() => {
    if (wsRef.current) return; // cegah double connect

    const ws = new WebSocket("ws://192.168.0.10:8000/ws");
    wsRef.current = ws;

    ws.onopen = () => {
      console.log("WS CONNECTED");
      onStatusRef.current?.("Online");
    };

    let timer;
    ws.onmessage = (event) => {
      clearTimeout(timer);
      timer = setTimeout(() => {
        const msg = JSON.parse(event.data);
        onUpdateRef.current?.(msg);
      }, 100); // update tiap 100ms max
    };

    ws.onerror = () => {
      onStatusRef.current?.("Error");
    };

    ws.onclose = () => {
      console.log("WS CLOSED");
      onStatusRef.current?.("Offline");
      wsRef.current = null;
    };

    return () => {
      ws.close();
    };
  }, []);

  return wsRef;
}
