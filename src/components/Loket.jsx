import React, { useState, useCallback, useEffect } from "react";
import useQueueSocket from "../hooks/useQueueSocket";

const API_URL = "http://localhost:8000";

export default function Loket() {
  const [queueItems, setQueueItems] = useState([]);
  const [currentPerson, setCurrentPerson] = useState(null);
  const [status, setStatus] = useState("Offline");
  const [loading, setLoading] = useState(false);
  const [form, setForm] = useState({ nama: "", NIK: "" });

  useQueueSocket({
    onStatus: setStatus,
    onUpdate: (msg) => {
      if (msg.type === "queue:update") {
        setQueueItems(msg.queue?.items || []);
      }
      if (msg.type === "queue:current") {
        setCurrentPerson(msg.person || null);
        setQueueItems(msg.queue?.items || []);
      }
    },
  });

  const action = useCallback((type) => {
    setLoading(true);
    fetch(`${API_URL}/${type}`, { method: "POST" })
      .finally(() => setLoading(false));
  }, []);

  const enqueue = useCallback(
    (e) => {
      e.preventDefault();
      if (!form.nama || !form.NIK) return;

      setLoading(true);
      const payload = { ...form };
      setForm({ nama: "", NIK: "" });

      fetch(`${API_URL}/enqueue`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      }).finally(() => setLoading(false));
    },
    [form]
  );
  useEffect(() => {
    fetch(`${API_URL}/queue`)
      .then(res => res.json())
      .then(data => {
        setQueueItems(data.items || []);
        setCurrentPerson(data.currentPerson || null);
      })
      .catch(err => console.error("Gagal load queue awal", err));
  }, []);

  return (
    <div className="min-h-screen p-6 bg-white text-black">
      <header className="max-w-5xl mx-auto mb-8">
        <h1 className="text-2xl font-bold">AntriDonk</h1>
        <p className="text-sm text-gray-600">
          Dashboard Operator • {status}
        </p>
      </header>

      <main className="max-w-5xl mx-auto grid grid-cols-1 md:grid-cols-3 gap-6">
        {/* Sedang Dilayani */}
        <section className="bg-gray-50 p-5 rounded-xl">
          <h3 className="text-sm font-semibold text-gray-500 mb-3">
            Sedang Dilayani
          </h3>
          {currentPerson ? (
            <>
              <div className="text-4xl font-bold text-red-700">
                A-{currentPerson.nomorAntrian}
              </div>
              <div className="mt-2 font-semibold">{currentPerson.nama}</div>
              <div className="text-sm text-gray-500">
                NIK: {currentPerson.NIK}
              </div>
            </>
          ) : (
            <p className="italic text-gray-400">Belum ada</p>
          )}
        </section>

        {/* Tombol Aksi */}
        <section className="bg-gray-50 p-5 rounded-xl space-y-3">
          <button
            disabled={loading}
            onClick={() => action("dequeue")}
            className="w-full py-3 bg-red-700 text-white rounded-md disabled:opacity-60"
          >
            Panggil
          </button>
          <button
            disabled={loading}
            onClick={() => action("skip")}
            className="w-full py-3 border border-red-700 text-red-700 rounded-md"
          >
            Skip
          </button>
        </section>

        {/* Form Tambah */}
        <section className="bg-gray-50 p-5 rounded-xl">
          <h3 className="font-semibold mb-3">Daftar Baru</h3>
          <form onSubmit={enqueue} className="space-y-3">
            <input
              className="w-full p-3 border rounded-md"
              placeholder="Nama"
              value={form.nama}
              onChange={(e) => setForm({ ...form, nama: e.target.value })}
            />
            <input
              className="w-full p-3 border rounded-md"
              placeholder="NIK"
              value={form.NIK}
              onChange={(e) => setForm({ ...form, NIK: e.target.value })}
            />
            <button
              disabled={loading}
              className="w-full py-3 bg-black text-white rounded-md"
            >
              Tambah
            </button>
          </form>
        </section>
      </main>

      {/* Daftar Antrian */}
      <section className="max-w-5xl mx-auto mt-8 bg-white rounded-xl border">
        <div className="p-4 font-semibold border-b">
          Antrian Menunggu ({queueItems.length})
        </div>
        <div>
          {queueItems.slice(0, 5).map((q, idx) => (
            <div
              key={q.nomorAntrian}
              className="p-2 flex justify-between border-b text-sm"
            >
              <span>{q.nomorAntrian}. {q.nama}</span>
              <span className="text-gray-400">#{idx + 1}</span>
            </div>
          ))}
          {queueItems.length === 0 && (
            <p className="italic text-gray-400 p-2">Tidak ada antrian</p>
          )}
        </div>
      </section>
    </div>
  );
}
