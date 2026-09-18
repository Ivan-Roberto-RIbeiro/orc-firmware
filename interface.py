import asyncio
import struct
import threading
import numpy as np
import tkinter as tk
from tkinter import ttk
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from collections import deque
from bleak import BleakClient
import sounddevice as sd

# ==========================================
# CONFIGURAÇÕES
# ==========================================
TIRESIAS_MAC            = "C0:6E:F3:83:4C:7F"
TIRESIAS_TELEMETRY_UUID = "12345678-1234-5678-1234-56789abcdef1"
TIRESIAS_CTRL_UUID      = "12345678-1234-5678-1234-56789abcdef2"
ADC_TO_MV = 3600.0 / 4095.0

MAX_HISTORY_SEC = 10
JANELA_SEGUNDOS = 10
FS_ADC = 2500
FS_IMU = 50 

MAX_ADC = MAX_HISTORY_SEC * FS_ADC
MAX_IMU = MAX_HISTORY_SEC * FS_IMU

# Buffers Circulares para o Gráfico
data_emg1 = deque(np.zeros(MAX_ADC), maxlen=MAX_ADC)
data_emg2 = deque(np.zeros(MAX_ADC), maxlen=MAX_ADC)
data_mic1 = deque(np.zeros(MAX_ADC), maxlen=MAX_ADC)
data_mic2 = deque(np.zeros(MAX_ADC), maxlen=MAX_ADC)
data_pitch = deque(np.zeros(MAX_IMU), maxlen=MAX_IMU)
data_roll  = deque(np.zeros(MAX_IMU), maxlen=MAX_IMU)

# Controle de estado
ble_loop = None
cmd_queue = None
is_connected = False

# ==========================================
# AUDIO ENGINE (SoundDevice)
# ==========================================
audio_lock = threading.Lock()
audio_buffer = np.zeros(0, dtype='int16')

def audio_callback(outdata, frames, time, status):
    global audio_buffer
    with audio_lock:
        if len(audio_buffer) >= frames:
            outdata[:, 0] = audio_buffer[:frames]
            audio_buffer = audio_buffer[frames:]
        else:
            n = len(audio_buffer)
            if n > 0:
                outdata[:n, 0] = audio_buffer
            outdata[n:, 0] = 0
            audio_buffer = np.zeros(0, dtype='int16')

audio_stream = sd.OutputStream(samplerate=FS_ADC, channels=1, dtype='int16', callback=audio_callback)
audio_stream.start()

# ==========================================
# BLUETOOTH TASK
# ==========================================
def notification_handler(sender, data):
    global audio_buffer
    if len(data) != 208: return
    unpacked = struct.unpack('<Ihh200b', data)
    data_pitch.append(unpacked[1])
    data_roll.append(unpacked[2])
    
    raw = unpacked[3:]
    for i in range(0, 200, 4):
        data_emg1.append((raw[i] * 256) * ADC_TO_MV)
        data_emg2.append((raw[i+1] * 256) * ADC_TO_MV)
        data_mic1.append((raw[i+2] * 256) * ADC_TO_MV)
        data_mic2.append((raw[i+3] * 256) * ADC_TO_MV)
        
    ch = listen_channel.get()
    if ch > 0 and ch <= 4:
        offset = ch - 1
        samples = np.array(raw[offset::4], dtype='int16') * 256
        with audio_lock:
            audio_buffer = np.concatenate((audio_buffer, samples))
            if len(audio_buffer) > int(FS_ADC * 0.5):
                audio_buffer = audio_buffer[-int(FS_ADC * 0.5):]

async def ble_task():
    global is_connected
    lbl_status.config(text="Status: Conectando...", fg="orange")
    try:
        async with BleakClient(TIRESIAS_MAC) as client:
            is_connected = True
            lbl_status.config(text="Status: Conectado e Aguardando", fg="green")
            await client.start_notify(TIRESIAS_TELEMETRY_UUID, notification_handler)
            while True:
                comando = await cmd_queue.get()
                if comando == "START":
                    lbl_status.config(text="Status: Coletando Dados AO VIVO", fg="blue")
                    await client.write_gatt_char(TIRESIAS_CTRL_UUID, bytearray([0x01]), response=False)
                elif comando == "STOP":
                    lbl_status.config(text="Status: Conectado (Em Pausa)", fg="green")
                    await client.write_gatt_char(TIRESIAS_CTRL_UUID, bytearray([0x02]), response=False)
                elif comando == "DISCONNECT":
                    break
            await client.stop_notify(TIRESIAS_TELEMETRY_UUID)
    except Exception as e:
        lbl_status.config(text=f"Erro: {e}", fg="red")
    finally:
        is_connected = False
        lbl_status.config(text="Status: Desconectado", fg="red")

def start_ble_loop():
    global cmd_queue, ble_loop
    ble_loop = asyncio.new_event_loop()
    asyncio.set_event_loop(ble_loop)
    cmd_queue = asyncio.Queue()
    ble_loop.run_until_complete(ble_task())

def send_cmd(cmd):
    if ble_loop and cmd_queue:
        ble_loop.call_soon_threadsafe(cmd_queue.put_nowait, cmd)

# ==========================================
# FUNÇÕES DO OSCILOSCÓPIO
# ==========================================
def limpar_graficos():
    data_emg1.extend(np.zeros(MAX_ADC))
    data_emg2.extend(np.zeros(MAX_ADC))
    data_mic1.extend(np.zeros(MAX_ADC))
    data_mic2.extend(np.zeros(MAX_ADC))
    data_pitch.extend(np.zeros(MAX_IMU))
    data_roll.extend(np.zeros(MAX_IMU))

def mudar_escala_tempo(fator):
    global JANELA_SEGUNDOS
    nova = JANELA_SEGUNDOS * fator
    if 0.1 <= nova <= 10:
        JANELA_SEGUNDOS = nova
        lbl_time.config(text=f"Escala X: {JANELA_SEGUNDOS:.1f}s")
        for ax in axs:
            ax.set_xlim(-JANELA_SEGUNDOS, 0)

y_limites = [30000, 30000, 32000, 30000, 180]
y_labels_ui = []

def mudar_escala_y(idx, fator):
    y_limites[idx] = int(y_limites[idx] * fator)
    if y_limites[idx] < 10: y_limites[idx] = 10
    axs[idx].set_ylim(-y_limites[idx], y_limites[idx])
    y_labels_ui[idx].config(text=f"± {y_limites[idx]}")

# ==========================================
# INTERFACE GRÁFICA (Tkinter)
# ==========================================
root = tk.Tk()
root.title("Tiresias - Osciloscópio e Telemetria")
root.geometry("1300x850")

listen_channel = tk.IntVar(value=0)

# Barra Superior Principal
frame_top = tk.Frame(root, bg="#e0e0e0", bd=2, relief=tk.RAISED)
frame_top.pack(side=tk.TOP, fill=tk.X)

# Menu Superior (Osciloscópio Controls)
frame_menu = tk.Frame(frame_top, bg="#e0e0e0")
frame_menu.pack(side=tk.TOP, fill=tk.X, padx=5, pady=5)

btn_conn = tk.Button(frame_menu, text="⚡ Conectar", command=lambda: threading.Thread(target=start_ble_loop, daemon=True).start() if not is_connected else None, font=("Arial", 10, "bold"))
btn_conn.pack(side=tk.LEFT, padx=5)

btn_disc = tk.Button(frame_menu, text="⛔ Desconectar", command=lambda: send_cmd("DISCONNECT"), font=("Arial", 10, "bold"), bg="#ffdddd")
btn_disc.pack(side=tk.LEFT, padx=5)

tk.Label(frame_menu, text=" | ", bg="#e0e0e0").pack(side=tk.LEFT)

btn_start = tk.Button(frame_menu, text="▶️ START", command=lambda: send_cmd("START"), font=("Arial", 10, "bold"), bg="#90EE90")
btn_start.pack(side=tk.LEFT, padx=5)

btn_stop = tk.Button(frame_menu, text="⏸️ STOP", command=lambda: send_cmd("STOP"), font=("Arial", 10, "bold"), bg="#FFB6C1")
btn_stop.pack(side=tk.LEFT, padx=5)

tk.Label(frame_menu, text=" | ", bg="#e0e0e0").pack(side=tk.LEFT)

btn_clear = tk.Button(frame_menu, text="🗑️ Limpar Gráficos", command=limpar_graficos, font=("Arial", 10, "bold"), bg="#FFFACD")
btn_clear.pack(side=tk.LEFT, padx=5)

# Controles de Escala de Tempo Horizontal (Global)
tk.Label(frame_menu, text=" | Escala Horizontal (Tempo):", font=("Arial", 10, "bold"), bg="#e0e0e0").pack(side=tk.LEFT, padx=5)
btn_time_minus = tk.Button(frame_menu, text="➖ Zoom In", command=lambda: mudar_escala_tempo(0.5), font=("Arial", 9))
btn_time_minus.pack(side=tk.LEFT, padx=2)
lbl_time = tk.Label(frame_menu, text="Escala X: 10.0s", width=12, font=("Arial", 10, "bold"), bg="white", relief=tk.SUNKEN)
lbl_time.pack(side=tk.LEFT, padx=2)
btn_time_plus = tk.Button(frame_menu, text="➕ Zoom Out", command=lambda: mudar_escala_tempo(2.0), font=("Arial", 9))
btn_time_plus.pack(side=tk.LEFT, padx=2)

lbl_status = tk.Label(frame_menu, text="Status: Aguardando Conexão", font=("Arial", 11, "italic"), bg="#e0e0e0")
lbl_status.pack(side=tk.RIGHT, padx=20)


# Layout principal: Esquerda (Audio) + Centro (Gráficos) + Direita (Escalas Verticais)
frame_main = tk.Frame(root)
frame_main.pack(side=tk.TOP, fill=tk.BOTH, expand=True)

frame_left = tk.Frame(frame_main, width=120, bd=1, relief=tk.SUNKEN)
frame_left.pack(side=tk.LEFT, fill=tk.Y, padx=5, pady=5)
frame_left.pack_propagate(False)

frame_center = tk.Frame(frame_main)
frame_center.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

frame_right = tk.Frame(frame_main, width=150, bd=1, relief=tk.SUNKEN)
frame_right.pack(side=tk.RIGHT, fill=tk.Y, padx=5, pady=5)
frame_right.pack_propagate(False)

# --- Coluna da Esquerda (ÁUDIO) ---
tk.Label(frame_left, text="🎧 Áudio PC", font=("Arial", 10, "bold")).pack(side=tk.TOP, pady=10)
tk.Radiobutton(frame_left, text="Mudo", variable=listen_channel, value=0).pack(expand=True)
tk.Radiobutton(frame_left, text="Escutar\nEMG 1", variable=listen_channel, value=1).pack(expand=True)
tk.Radiobutton(frame_left, text="Escutar\nEMG 2", variable=listen_channel, value=2).pack(expand=True)
tk.Radiobutton(frame_left, text="Escutar\nMic P2", variable=listen_channel, value=3).pack(expand=True)
tk.Radiobutton(frame_left, text="Escutar\nMic Placa", variable=listen_channel, value=4).pack(expand=True)
tk.Label(frame_left, text="IMU").pack(expand=True) 

# --- Coluna da Direita (ESCALA VERTICAL / AMPLITUDE) ---
tk.Label(frame_right, text="↕ Escala Vertical\n(Zoom Y)", font=("Arial", 10, "bold")).pack(side=tk.TOP, pady=10)

nomes_graficos = ["EMG 1", "EMG 2", "Mic P2", "Mic Placa", "IMU"]
for i in range(5):
    f_ctrl = tk.Frame(frame_right)
    f_ctrl.pack(expand=True)
    tk.Label(f_ctrl, text=nomes_graficos[i], font=("Arial", 9, "bold")).pack(side=tk.TOP)
    
    f_btns = tk.Frame(f_ctrl)
    f_btns.pack(side=tk.TOP)
    
    tk.Button(f_btns, text="➖", command=lambda idx=i: mudar_escala_y(idx, 0.5)).pack(side=tk.LEFT)
    lbl_y = tk.Label(f_btns, text=f"± {y_limites[i]}", width=8, bg="white", relief=tk.SUNKEN)
    lbl_y.pack(side=tk.LEFT, padx=2)
    tk.Button(f_btns, text="➕", command=lambda idx=i: mudar_escala_y(idx, 2.0)).pack(side=tk.LEFT)
    
    y_labels_ui.append(lbl_y)

# ==========================================
# GRÁFICOS (Matplotlib dentro do Tkinter)
# ==========================================
plt.style.use('fast')
fig, axs = plt.subplots(5, 1, figsize=(10, 8))

x_adc = np.linspace(-MAX_HISTORY_SEC, 0, MAX_ADC)
x_imu = np.linspace(-MAX_HISTORY_SEC, 0, MAX_IMU)

lines = []
lines.append(axs[0].plot(x_adc, data_emg1, color='#1f77b4', lw=1)[0])
lines.append(axs[1].plot(x_adc, data_emg2, color='#17becf', lw=1)[0])
lines.append(axs[2].plot(x_adc, data_mic1, color='#ff7f0e', lw=1)[0])
lines.append(axs[3].plot(x_adc, data_mic2, color='#2ca02c', lw=1)[0])
lines.append(axs[4].plot(x_imu, data_pitch, color='#d62728', lw=1.5, label='Pitch')[0])
lines.append(axs[4].plot(x_imu, data_roll, color='#9467bd', lw=1.5, label='Roll')[0])

titulos = ['EMG 1 (mV)', 'EMG 2 (mV)', 'Mic Line-In (P2) (mV)', 'Mic Placa (mV)', 'IMU (°)']

for i, ax in enumerate(axs):
    ax.set_xlim(-JANELA_SEGUNDOS, 0)
    ax.set_ylim(-y_limites[i], y_limites[i])
    ax.set_ylabel(titulos[i])
    ax.grid(True, linestyle='--', alpha=0.5)

axs[4].set_xlabel('Tempo (segundos)')
axs[4].legend(loc='upper right')
fig.tight_layout()

canvas = FigureCanvasTkAgg(fig, master=frame_center)
canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)

toolbar = NavigationToolbar2Tk(canvas, frame_center)
toolbar.update()
canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)

def update_plot(frame):
    # Slice arrays based on current window to simulate zooming
    # Since x_adc is fixed from -10 to 0, setting xlim limits the view correctly.
    lines[0].set_ydata(data_emg1)
    lines[1].set_ydata(data_emg2)
    lines[2].set_ydata(data_mic1)
    lines[3].set_ydata(data_mic2)
    lines[4].set_ydata(data_pitch)
    lines[5].set_ydata(data_roll)
    return lines

ani = FuncAnimation(fig, update_plot, interval=50, blit=True, cache_frame_data=False)

def ao_fechar():
    send_cmd("DISCONNECT")
    audio_stream.stop()
    audio_stream.close()
    root.destroy()

root.protocol("WM_DELETE_WINDOW", ao_fechar)
root.mainloop()