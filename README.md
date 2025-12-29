# AntriDonk

> **Dashboard & Display Sistem Antrian Bansos**  
> Dibangun dengan **React (frontend)** dan **Oat++ (backend C++)** 

---

## Fitur Utama
- **Dashboard Operator**: panggil, skip, dan tambah penerima dengan sekali klik.
-  **WebSocket Real‑time**: update status antrian langsung tanpa refresh.
-  **Display Penerima**: layar khusus penerima untuk melihat nomor antrian & loket.
-  **CORS & API Ready**: backend aman untuk diakses dari berbagai origin (Kayaknya :`)).

---

##  Teknologi
| Bagian        | Teknologi |
|---------------|-----------|
| **Frontend**  | React + TailwindCSS |
| **Backend**   | Oat++ (C++), WebSocket |
| **Transport** | REST API + WebSocket |
| **Build Tool**| Vite, CMake |

---

## Cara Menjalankan

### Backend
```bash
cd backend
mkdir build && cd build
cmake ..
make
./ServerApp
