# M5DinMeter-KilnController

Open-source kiln controller for pottery / ceramics based on the **M5Stack DinMeter** (ESP32-S3).
Controllore open-source per forno da ceramica basato su **M5Stack DinMeter** (ESP32-S3).

This file is bilingual: the full English documentation comes first, followed by the Italian
version with the same structure.
Questo file è bilingue: prima la documentazione completa in inglese, poi la versione italiana
con identica struttura.

- [English documentation](#english-documentation)
- [Documentazione italiana](#documentazione-italiana)
- [License / Licenza](#license--licenza)

---

# English documentation

## 1. Overview

**M5DinMeter-KilnController** is a complete control system for ceramic/pottery kilns
(up to **1200 °C**). It runs on an M5Stack DinMeter — a compact ESP32-S3 device with a
240×135 TFT display, a rotary encoder and a single button — and manages the whole firing:
programmed temperature curves, PID regulation with on-field self-correction, and a full
safety layer.

The system is designed to be used exactly like an industrial kiln programmer: select a
program, press, and walk away. The PID tunes itself during real firings, so no empty-furnace
auto-tune test is ever needed.

## 2. Features

- **Temperature curves**: 2..20 break-point points (`cumSec, temp`), edited with a built-in
  **graphical editor** — no step wizard, no intermediate steps: the slope between points is
  implicit (ramps) and a hold is simply two points at the same temperature.
- **PID controller** with:
  - fixed 1 s cycle time, derivative computed on the *measured* temperature (no kick);
  - anti-windup (integrates only when the output is not saturated and |error| > 0.2 °C),
    integrator clamp ±300 °C·s;
  - **gain scheduling** on 3 temperature bands (<400 / 400–900 / >900 °C);
  - **self-correction** of Kp/Ki/Kd during real firings through passive observers on holds
    and ramps (no relay/step-relay auto-tune).
- **Time-proportional SSR driving** on a fixed 10 s window — full DC blocks, safe for
  zero-cross SSRs, no fast PWM (see Hardware).
- **Real-time RUNNING screen**: programmed curve (yellow), measured temperature (green),
  live position marker, automatic X-axis tick steps, SSR duty %, battery %.
- **Complete safety layer**: absolute temperature stop at 1250 °C, hardware watchdog,
  slow-ramp watchdog, fail-safe SSR OFF on sensor error.
- **NVS persistence**: programs and PID gains survive reboots; automatic migration of the
  legacy program format.
- Minimal input devices: rotary encoder + single button (short/long press) only.

## 3. Hardware & wiring

### 3.1 Bill of materials

| Component | Details |
|---|---|
| Board | **M5Stack DinMeter** (ESP32-S3), TFT 240×135, `setRotation(1)` |
| Rotary encoder | `DinMeter.Encoder` (GPIO 40/41), ±2 counts per detent, interrupt-driven (PJRC `Encoder` library) |
| Button | `DinMeter.BtnA` — short press / long press (1 s) |
| SSR | **Fotek SSR-40 DA** (zero-cross, 40 A, control input **3–32 VDC**), driven directly by **GPIO 1**, active HIGH |
| Thermocouple | Type K, read via **KMeterISO** module (MAX31855-compatible), **I2C address 0x66**, SDA=13, SCL=15, 400 kHz |
| Power | LiPo 3000 mAh; **USB 5 V recommended during firings** |

### 3.2 SSR direct drive — important notes

The Fotek SSR-40 DA control input needs **3–32 VDC**. The ESP32 GPIO provides **3.3 V**,
which sits ~0.3 V above the absolute minimum. Two points make this acceptable:

- The duty is applied as **time-proportional DC blocks** (SSR ON for `duty%` of a 10 s
  window), **not** as a fast PWM: while ON, the SSR control input always sees a full
  ~3.3 V level, the voltage is never reduced by modulation.
- Firmware sets the pin drive capability to the maximum (`GPIO_DRIVE_CAP_3`) so the output
  can source the SSR LED current (~10 mA) without collapsing.

Recommended practice:
- power the device from USB (5 V) during firings — on battery only the margin is lower;
- keep the control wires short and use a single common ground;
- fit a **10 kΩ pull-down between GPIO 1 and GND** if you want the SSR guaranteed OFF by
  construction during boot/reset (the GPIO floats for an instant before `setup()`).

### 3.3 KMeterISO temperature probe

- I2C address **0x66**, bus 400 kHz, `Wire.begin(13, 15, 400000L)` **after** `M5.begin()`.
- Register `0x00`: temperature, int32 big-endian, in **hundredths of °C** (raw 52345 = 523.45 °C).
- Register `0x20`: status; **0 = valid**.
- Boot probe: 5 attempts, then the firmware blocks (`while(true)`) if the module is absent.

### 3.4 Known issue with the KMeterISO library (I2C repeated-start bug)

> This is a documented pitfall that may save you hours if you use a KMeterISO module
> (or any MAX31855-based I2C clone) with the official M5Unit library.

**Symptom**: temperatures are wrong/erratic; the temperature "rises" even with the SSR off
and the heating elements cold. In our project this looked like an SSR failure — it was bad
telemetry all along.

**Root cause**: a bare `Wire.endTransmission()` (no argument) issues a **repeated start**
between the register write and the read. The KMeterISO module requires an **explicit STOP**
between the two phases. Without the STOP the module does not respond and the read returns
invalid data. Two additional flaws made it dangerous: no status-register check and no
handling of failed reads — so corrupted values were used directly.

**Workaround used in this project (manual I2C helper, no library)**:

1. `Wire.endTransmission(true)` — explicit STOP **before every** `Wire.requestFrom`; never
   repeated start;
2. `delay(50µs)` after writing the register (module settling time);
3. check status first: `kmReadReg(0x20) == 0` → only then read the temperature;
4. sentinel: if `Wire.available() < 4`, return `INT32_MIN` and discard the reading;
5. scale: `currentTemp = raw / 100.0f`;
6. failure counter: each failed read increments `sensorFailCount`; at
   `SENSOR_FAIL_THRESHOLD = 5` (~5 s) → `sensorError = true`;
7. **never drive the SSR with a stale/corrupted temperature**: `sensorError` forces the
   SSR OFF (fail-safe, see Safety).

## 4. User interface

Input is only the **rotary encoder** (rotate, short press, long press 1 s).

### 4.1 Main menu

- Program list, 24 px rows, ~4 visible, centered on the selection (blue highlight);
  encoder scrolls.
- Red battery % (size 2) at the top-left.
- Measured temperature **large (size 4)** on the right, vertically centered; updated every
  400 ms with partial redraw.
- Last entries: `Aggiungi Programma` (new program) and `Spegni` (power off).
- **Long press on a program** → delete confirmation screen (short = confirm, long = cancel).

### 4.2 New program — name

- Alphabet `A-Z 0-9 - _` + space, max 20 characters.
- Encoder scrolls the current character; short press appends it; long press confirms the name.

### 4.3 Curve editor (graphical)

After the name you land directly in the editor — there is **no "how many points" question**:
you start with 2 points and add/remove them as needed.

- Graph preview on top: current curve, grey **P0** (fixed, not editable) = the temperature
  measured when the firing starts.
- Command bar (ASCII only — the 6x8 font has no Unicode arrows):
  - `<` / `>` — select previous/next point;
  - `T` — edit **temperature** (encoder ±5 °C per detent, short press to confirm);
  - `t` — edit **time** (encoder ±5 min per detent, short press to confirm);
  - `+P` — add a point after the selected one (same temperature, time at midpoint toward
    the next point), then select it;
  - `-P` — delete the selected point (disabled below 2 points);
  - `Salva` — save; `Annulla` — discard (long press anywhere also cancels).
- Constraints: temperature 0..1200 °C; times strictly increasing (min +5 min between
  consecutive points); total program duration ≤ 24 h; 2..20 points.
- The editor **is** the review: what you see is exactly what will be executed.

### 4.4 RUNNING screen

Layout (all texts size 1, per specification):

```
| Name RAMPA 2/5        SSR:ON 65%   78% |   R1: name + segment | SSR duty | battery (red)
| Tgt 950.0C  TSens 523.4C   1h23m/4h00m |   R2: target (yellow) + measured (green) | time
| 1200┤                                  |   graph 210x98
|     │  (yellow curve, green history,   |   Y labels 0/300/600/900/1200
|     │   white position dot)            |
|    0└──────── 1h    2h    3h          |   X labels, automatic step (min 45 px)
```

- **R1**: program name + `RAMPA/TENUTA n/n` (segment type and index), `SSR:ON/OFF + duty %`,
  red battery percentage (red so it can't be confused with the SSR %).
- **R2**: `Tgt 950.0C` in yellow, `TSens 523.4C` in green (TSens = the value read by the
  thermocouple), elapsed/total time on the right.
- **Graph**: programmed curve (yellow polyline), measured temperature history (green),
  white dot marking the current position on the curve, Y-axis temperature labels,
  X-axis time labels with automatic "nice" step (1m/5m/10m/15m/30m/1h/2h/4h/6h/12h/24h,
  minimum 45 px between labels, format `2h30m`).
- **Sensor error** → R2 turns red `SENSORE ERRORE!`, SSR forced OFF.
- **Firing complete** → `FINE 4h00m SSR 2h10m` (total duration + SSR on-time).

### 4.5 Special states

- **SLOW_RISE** (slow ramp detected, see Safety): 3 choices via encoder + short press —
  `Attendi arrivo` / `Allunga 30 min` / `Annulla`.
- **SOVRATEMP** (over-temperature, see Safety): red full screen; exit only with long press.

## 5. PID & self-correction

### 5.1 Controller

Positional PID, fixed **dt = 1 s** (aligned with the thermocouple tick):

```
e(t)      = setpoint - currentTemp
integral += e * dt                  (only if anti-windup conditions are met)
derivative = (currentTemp - prevTemp) / dt     ← derivative of the MEASURE, not of the error
duty      = Kp·e + Ki·integral + Kd·derivative
duty      = clamp(duty, 0, 100)     (%)
```

Anti-kick / anti-windup rules:
1. derivative on the measure → no kick when the setpoint steps;
2. integrate only when the output is not saturated (duty not at 0/100) **and** |e| > deadband;
3. deadband 0.2 °C → no integrator "creeping" at steady state;
4. integrator clamp ±300 °C·s;
5. output clamp 0..100 (an SSR can only heat).

Defaults: `Kp = 10.0, Ki = 0.2, Kd = 0.1` (PIDKiln-like orders of magnitude for kiln + SSR).

### 5.2 SSR actuation — time-proportional

A zero-cross SSR cannot modulate: fixed **10 s window**, SSR ON for `duty × window`:

```
SSR_WINDOW_MS = 10000
cyclePos = millis() % SSR_WINDOW_MS
ssrOn    = cyclePos < SSR_WINDOW_MS * duty / 100
```

- Resolution: 1 s tick / 10 s window = **10 % step**; max ~1 switch per second → negligible
  SSR wear.

### 5.3 Gain scheduling (3 bands)

Process gain changes dramatically with temperature (conduction/convection below ~400 °C,
T⁴ radiation above ~800 °C — real variation up to 7:1). The band is selected by the
**current target** (the setpoint, which is stable):

```
Band 1: target <  400 °C  → Kp1 Ki1 Kd1
Band 2: target 400–900 °C → Kp2 Ki2 Kd2
Band 3: target >  900 °C  → Kp3 Ki3 Kd3
```

### 5.4 Self-correction (observers on real firings)

No auto-tune library is used (relay/Ziegler-Nichols methods force oscillations and require
empty-furnace tests, which are forbidden here). Instead, passive **industrial diagnostics**
observe real firings:

**In HOLD (dwell), sliding 120 s window:**

| Observation | Meaning | Correction |
|---|---|---|
| ≥ 4 error zero-crossings in the window | Kp too strong (oscillation) | `Kp × 0.9` |
| Peak above target > 3 °C after entering hold | Ki too strong (residual windup) | `Ki × 0.9` |
| Mean \|error\| over 120 s > 2 °C, flat temperature | Ki too weak (residual offset) | `Ki × 1.1` |
| Duty swings ±20 % between ticks | Kd too high (noise amplification) | `Kd × 0.9` |

**In RAMP (last 120 s):**

| Observation | Meaning | Correction |
|---|---|---|
| ≥ 4 crossings of the ramp line | "tug-of-war" PID, sawtooth growth | `Kp × 0.9` **and** `Kd × 1.1` |
| Above line > 8 °C for 60 s | too much accumulated heat | `Ki × 0.9` |
| Below line > 8 °C for 120 s (after the first 10 min of ramp) | not enough power | `Kp × 1.1` |

**Gate** (all corrections require): sensor OK; stationary temperature in hold
(max-min over 120 s < 2 °C); |error| < 5 °C in hold started ≥ 3 min ago; ramp in progress
> 10 min; **max 1 correction every 120 s** (holds and ramps share the same ticket).

**Safety clamps** (a correction can never exceed):

```
Kp ∈ [0.5, 50]      Ki ∈ [0.01, 2]       Kd ∈ [0, 5]
```

Three layers of defense: strict gate (never correct on dirty data) + ±10 % per step
(≈7 valid corrections = ~15 min to double a parameter) + absolute clamps.
Only the band of the current hold/ramp is corrected. Persisted immediately to NVS
(`pid_kp_1..3`, `pid_ki_1..3`, `pid_kd_1..3`). Switch: `AUTOTUNE_ENABLED = true` in code.
The integrator is reset at program start and at each curve-segment change.

## 6. Safety (fail-safe by construction)

1. **SSR forced OFF** outside RUNNING, at program start/stop, and on sensor error.
2. **Over-temperature stop**: `OVER_TEMP_C = 1250 °C` (1200 operating + 50 margin),
   hysteresis 10 °C → SSR OFF, full red screen (SOVRATEMP), exit only by long press
   (deliberate human action).
3. **Hardware watchdog**: `esp_task_wdt`, 10 s timeout, panic reset — a stuck loop cannot
   leave the SSR in an unknown state.
4. **Slow-ramp watchdog** ("salita lenta"): if the temperature stays **> 15 °C below the
   curve** for **60 minutes**, the controller can no longer assume normal behavior and
   opens a 3-choice screen:
   - `Attendi arrivo` — freeze the setpoint at the current curve value, wait until the
     temperature arrives (±2 °C), with a 1 h cap: after that the choice is asked again;
   - `Allunga 30 min` — insert a flat 30 min point at the current position and shift the
     rest of the curve accordingly (`SLOW_RISE_EXTEND_S = 1800`);
   - `Annulla` — abort the firing.
5. **Fail-safe principle**: never heat on a stale/frozen temperature reading.

## 7. Data model & persistence

```
struct Point   { uint32_t cumSec; float temp; }     // cumulative time from start (s), °C
struct Program { String name; Point points[20]; uint8_t pointCount; }
Program programs[10]; uint8_t programCount;
```

- **P0 is virtual**: at firing start the curve begins at `(0, measured temperature)` —
  the first ramp always starts from the real temperature of the cold kiln (no seasonal gap).
- Duration is **implicit** (difference between consecutive `cumSec`).
- Limits: 10 programs, ≤ 20 points, name ≤ 20 chars, segment ≤ 24 h, total ≤ 24 h.

NVS keys (namespace `kilnprogs`):

```
count
progX_name     (string)
progX_points   (uint) → pointCount
progX_pYc      (ulong  cumSec of point Y)
progX_pYt      (float  temp of point Y)
```

**Migration** from the legacy format (old steps `temp,duration` → 2 points each:
hold `A=(cum, temp_i)` to `B=(cum+duration, temp_i)`, then jump to the next step).
Fallback only: if `progX_points` is missing (0xFFFFFFFF), load and convert in RAM the old
keys; after the first save the new keys exist and the fallback never triggers again.
Over 20 points → keep the first 20.

Default factory program (if NVS is empty): 4 points
`(0,200) → (1h,200) → (3h,500) → (4h,500)`.

## 8. Build & upload

Prerequisites:

- `arduino-cli` (or Arduino IDE 2.x)
- ESP32 core **3.3.8** (`m5stack:esp32`)
- Libraries: **M5DinMeter**, **M5Unified**, **M5GFX** (board package installs them)

```bash
# one-time: install the board platform
arduino-cli core install m5stack:esp32

# compile
arduino-cli compile --fqbn m5stack:esp32:m5stack_dinmeter M5StackDin_KilnController

# upload (set your port)
arduino-cli upload -p COM7 --fqbn m5stack:esp32:m5stack_dinmeter M5StackDin_KilnController
```

Tested build: ~555 KB flash (~42 % of 1.3 MB), ~54 KB RAM (~16 %).

## 9. Dependencies

- `M5DinMeter` — board HAL (includes the PJRC `Encoder` interrupt library)
- `M5Unified` / `M5GFX` — display, button, power management
- `m5stack:esp32` core 3.3.8

## 10. Project structure

```
M5StackDin_KilnController.ino   setup, main loop, FSM, encoder, button, I2C helpers
kiln_ui.ino                     main menu, curve editor, delete confirmation
kiln_run.ino                    RUNNING screen (header + graph + axis labels)
kiln_programs.ino               curve execution, NVS persistence, migration
kiln_pid.ino                    PID, SSR actuation, observers, self-correction
docs/                           00_flusso_di_lavoro.md … 07_sicurezza.md
```

## 11. Documentation

The `docs/` folder holds the full design docs (in Italian):
`00` workflow, `01` hardware & SSR notes, `02` temperature reading & the I2C bug,
`03` PID & self-correction, `04` persistence & migration, `05` UI specifications,
`06` curve editor, `07` safety.

---

# Documentazione italiana

## 1. Panoramica

**M5DinMeter-KilnController** è un sistema di controllo completo per forni da ceramica
(fino a **1200 °C**). Gira su un M5Stack DinMeter — un dispositivo compatto ESP32-S3 con
display TFT 240×135, pomello rotativo e un solo pulsante — e gestisce l'intera cottura:
curve di temperatura programmabili, regolazione PID con auto-correzione sul campo e uno
strato di sicurezza completo.

Il sistema è pensato per essere usato come un programmatore industriale: si seleziona un
programma, si preme e ci si allontana. Il PID si accorda da solo durante le cotture reali,
senza mai richiedere test a vuoto del forno.

## 2. Caratteristiche

- **Curve di temperatura**: 2..20 punti di svolta (`cumSec, temp`), editati con un
  **editor grafico** integrato — niente wizard a step: la pendenza tra i punti è implicita
  (rampe) e una tenuta è semplicemente due punti alla stessa temperatura.
- **PID** con:
  - ciclo fisso di 1 s, derivata sulla temperatura *misurata* (niente kick);
  - anti-windup (integra solo se l'uscita non è satura e |errore| > 0.2 °C), clamp
    integrale ±300 °C·s;
  - **gain scheduling** su 3 fasce (<400 / 400–900 / >900 °C);
  - **auto-correzione** di Kp/Ki/Kd durante le cotture reali tramite osservatori passivi
    su tenute e rampe (niente autotune a relè).
- **Attuazione SSR time-proportional** su finestra fissa di 10 s — blocchi DC pieni,
  sicuri per SSR a zero-cross, niente PWM veloce (vedi Hardware).
- **Schermata RUNNING real-time**: curva programmata (gialla), temperatura misurata
  (verde), pallino di posizione, etichette X a passo automatico, duty % SSR, % batteria.
- **Sicurezza completa**: stop assoluto a 1250 °C, watchdog hardware, watchdog salita
  lenta, fail-safe SSR OFF su errore sensore.
- **Persistenza NVS**: programmi e guadagni PID sopravvivono ai riavvii; migrazione
  automatica del formato legacy.
- Ingressi minimi: solo pomello + pulsante (short/long press).

## 3. Hardware e cablaggio

### 3.1 Lista componenti

| Componente | Dettagli |
|---|---|
| Scheda | **M5Stack DinMeter** (ESP32-S3), TFT 240×135, `setRotation(1)` |
| Pomello | `DinMeter.Encoder` (GPIO 40/41), ±2 conteggi per detent, lettura via interrupt (libreria PJRC `Encoder`) |
| Pulsante | `DinMeter.BtnA` — short press / long press (1 s) |
| SSR | **Fotek SSR-40 DA** (zero-cross, 40 A, ingresso di controllo **3–32 VDC**), pilotato direttamente da **GPIO 1**, attivo alto |
| Termocoppia | Tipo K, letta via modulo **KMeterISO** (compatibile MAX31855), **I2C indirizzo 0x66**, SDA=13, SCL=15, 400 kHz |
| Alimentazione | LiPo 3000 mAh; **USB 5 V consigliata durante le cotture** |

### 3.2 Pilotaggio SSR diretto — note importanti

L'ingresso di controllo del Fotek SSR-40 DA richiede **3–32 VDC**. Il GPIO dell'ESP32
fornisce **3.3 V**, circa 0.3 V sopra il minimo assoluto. Due punti rendono la cosa
accettabile:

- Il duty è applicato come **blocchi DC time-proportional** (SSR on per `duty%` di una
  finestra di 10 s), **non** come PWM veloce: quando è on, l'ingresso vede sempre ~3.3 V
  pieni, la tensione non viene mai ridotta dalla modulazione.
- Il firmware imposta la drive capability massima (`GPIO_DRIVE_CAP_3`) perché l'uscita
  possa alimentare la corrente del LED dell'SSR (~10 mA) senza crollare.

Consigli pratici:
- alimentare da USB (5 V) in cottura — con la sola batteria il margine si riduce;
- cavi di controllo corti e GND comune;
- **pull-down 10 kΩ tra GPIO 1 e GND** se si vuole l'SSR spento per costruzione durante
  boot/reset (il GPIO è flottante per un istante prima di `setup()`).

### 3.3 Sonda KMeterISO

- Indirizzo I2C **0x66**, bus 400 kHz, `Wire.begin(13, 15, 400000L)` **dopo** `M5.begin()`.
- Registro `0x00`: temperatura, int32 big-endian, in **centesimi di °C** (raw 52345 = 523.45 °C).
- Registro `0x20`: stato; **0 = valido**.
- Probe al boot: 5 tentativi, poi blocco (`while(true)`) se il modulo è assente.

### 3.4 Bug noto della libreria KMeterISO (repeated-start I2C)

> Trappola documentata che può far risparmiare ore a chi usa un modulo KMeterISO
> (o qualsiasi clone I2C basato su MAX31855) con la libreria ufficiale M5Unit.

**Sintomo**: temperature errate/sballate; la temperatura "sale" anche con SSR off ed
elementi freddi. Nel nostro progetto sembrava un guasto dell'SSR — era telemetria inaffidabile.

**Causa**: una `Wire.endTransmission()` senza argomento emette un **repeated start** tra la
scrittura del registro e la lettura. Il modulo KMeterISO richiede una **STOP esplicita** tra
le due fasi. Senza STOP il modulo non risponde e la lettura restituisce dati non validi.
Due difetti aggiuntivi la rendevano pericolosa: nessun controllo del registro di stato e
nessuna gestione delle letture fallite — quindi valori corrotti usati direttamente.

**Workaround usato in questo progetto (helper I2C manuale, niente libreria)**:

1. `Wire.endTransmission(true)` — STOP esplicita **prima di ogni** `Wire.requestFrom`;
   mai repeated start;
2. `delay(50µs)` dopo la scrittura del registro (assestamento del modulo);
3. controllo stato prima: `kmReadReg(0x20) == 0` → solo allora leggere la temperatura;
4. sentinella: se `Wire.available() < 4`, restituire `INT32_MIN` e scartare la lettura;
5. scala: `currentTemp = raw / 100.0f`;
6. contatore fallimenti: ogni lettura fallita incrementa `sensorFailCount`; a
   `SENSOR_FAIL_THRESHOLD = 5` (~5 s) → `sensorError = true`;
7. **mai pilotare l'SSR con una temperatura stantia/corrotta**: `sensorError` forza
   l'SSR OFF (fail-safe, vedi Sicurezza).

## 4. Interfaccia utente

Ingresso solo con **pomello rotativo** (ruota, short press, long press 1 s).

### 4.1 Menu principale

- Lista programmi, righe da 24 px, ~4 visibili, centrata sulla selezione (evidenza blu);
  il pomello scorre.
- % batteria rossa (size 2) in alto a sinistra.
- Temperatura misurata **grande (size 4)** a destra, centrata verticalmente; aggiornata
  ogni 400 ms con ridisegno parziale.
- Ultime voci: `Aggiungi Programma` (nuovo programma) e `Spegni` (power off).
- **Long press su un programma** → schermata di conferma cancellazione (short = conferma,
  long = annulla).

### 4.2 Nuovo programma — nome

- Alfabeto `A-Z 0-9 - _` + spazio, massimo 20 caratteri.
- Il pomello scorre il carattere corrente; short press lo aggiunge; long press conferma il nome.

### 4.3 Editor curva (grafico)

Dopo il nome si entra direttamente nell'editor — **niente domanda "quanti punti"**:
si parte con 2 punti e li si aggiungono/eliminano a seconda delle necessità.

- Anteprima grafica in alto: curva corrente, **P0** grigio fisso (non editabile) = la
  temperatura misurata all'avvio della cottura.
- Barra comandi (solo ASCII — il font 6x8 non ha frecce Unicode):
  - `<` / `>` — seleziona il punto precedente/successivo;
  - `T` — modifica la **temperatura** (pomello ±5 °C per detent, short press per confermare);
  - `t` — modifica il **tempo** (pomello ±5 min per detent, short press per confermare);
  - `+P` — aggiunge un punto dopo quello selezionato (stessa temperatura, tempo a metà
    verso il punto successivo), poi lo seleziona;
  - `-P` — elimina il punto selezionato (disattivo sotto i 2 punti);
  - `Salva` — salva; `Annulla` — scarta (long press ovunque annulla).
- Vincoli: temperatura 0..1200 °C; tempi strettamente crescenti (min +5 min tra punti
  consecutivi); durata totale ≤ 24 h; 2..20 punti.
- L'editor **è** la review: quello che vedi è quello che verrà eseguito.

### 4.4 Schermata RUNNING

Layout (tutti i testi size 1, da specifica):

```
| Nome RAMPA 2/5         SSR:ON 65%   78% |   R1: nome + segmento | duty SSR | batteria (rossa)
| Tgt 950.0C  TSens 523.4C    1h23m/4h00m |   R2: target (giallo) + misurata (verde) | tempo
| 1200┤                                   |   grafico 210x98
|     │  (curva gialla, storia verde,     |   etichette Y 0/300/600/900/1200
|     │   pallino posizione bianco)       |
|    0└──────── 1h    2h    3h           |   etichette X, passo automatico (min 45 px)
```

- **R1**: nome programma + `RAMPA/TENUTA n/n` (tipo di segmento e indice), `SSR:ON/OFF +
  duty %`, % batteria rossa (rossa per non confonderla con la % SSR).
- **R2**: `Tgt 950.0C` in giallo, `TSens 523.4C` in verde (TSens = valore letto dalla
  termocoppia), tempo trascorso/totale a destra.
- **Grafico**: curva programmata (gialla), storia temperatura misurata (verde), pallino
  bianco di posizione sulla curva, etichette Y della temperatura, etichette X a passo
  "carino" automatico (1m/5m/10m/15m/30m/1h/2h/4h/6h/12h/24h, minimo 45 px tra le
  etichette, formato `2h30m`).
- **Errore sensore** → R2 rossa `SENSORE ERRORE!`, SSR forzato OFF.
- **Cottura terminata** → `FINE 4h00m SSR 2h10m` (durata totale + on-time SSR).

### 4.5 Stati speciali

- **SLOW_RISE** (salita lenta rilevata, vedi Sicurezza): 3 scelte con pomello + short
  press — `Attendi arrivo` / `Allunga 30 min` / `Annulla`.
- **SOVRATEMP** (sovratemperatura, vedi Sicurezza): schermata rossa piena; uscita solo con
  long press.

## 5. PID e auto-correzione

### 5.1 Controllore

PID posizionale, **dt = 1 s fisso** (allineato al tick della termocoppia):

```
e(t)      = setpoint - currentTemp
integrale += e * dt                  (solo se le condizioni anti-windup sono soddisfatte)
derivata  = (currentTemp - prevTemp) / dt     ← derivata della MISURA, non dell'errore
duty      = Kp·e + Ki·integrale + Kd·derivata
duty      = clamp(duty, 0, 100)      (%)
```

Regole anti-kick / anti-windup:
1. derivata sulla misura → niente kick quando il setpoint fa un gradino;
2. si integra solo se l'uscita non è satura (duty non a 0/100) **e** |e| > deadband;
3. deadband 0.2 °C → niente "creeping" dell'integrale a regime;
4. clamp integrale ±300 °C·s;
5. clamp uscita 0..100 (un SSR può solo scaldare).

Default: `Kp = 10.0, Ki = 0.2, Kd = 0.1` (ordini di grandezza stile PIDKiln per forno + SSR).

### 5.2 Attuazione SSR — time-proportional

Un SSR a zero-cross non modula: finestra fissa di **10 s**, SSR on per `duty × finestra`:

```
SSR_WINDOW_MS = 10000
cyclePos = millis() % SSR_WINDOW_MS
ssrOn    = cyclePos < SSR_WINDOW_MS * duty / 100
```

- Risoluzione: tick 1 s / finestra 10 s = **passo del 10 %**; al massimo ~1 commutazione
  al secondo → usura SSR trascurabile.

### 5.3 Gain scheduling (3 fasce)

Il guadagno del processo cambia molto con la temperatura (conduzione/convezione sotto
~400 °C, radiazione T⁴ sopra ~800 °C — variazione reale fino a 7:1). La fascia è scelta dal
**target corrente** (il setpoint, stabile):

```
Fascia 1: target <  400 °C  → Kp1 Ki1 Kd1
Fascia 2: target 400–900 °C → Kp2 Ki2 Kd2
Fascia 3: target >  900 °C  → Kp3 Ki3 Kd3
```

### 5.4 Auto-correzione (osservatori sulle cotture reali)

Nessuna libreria di autotune (i metodi a relè/Ziegler-Nichols forzano oscillazioni e
richiedono test a vuoto, vietati). Si usano invece **diagnostiche industriali passive**
che osservano le cotture reali:

**In TENUTA, finestra scorrevole di 120 s:**

| Osservazione | Significato | Correzione |
|---|---|---|
| ≥ 4 attraversamenti dello zero dell'errore nella finestra | Kp troppo forte (oscillazione) | `Kp × 0.9` |
| Picco sopra il target > 3 °C dopo l'ingresso in tenuta | Ki troppo forte (windup residuo) | `Ki × 0.9` |
| Media \|errore\| su 120 s > 2 °C, temperatura piatta | Ki troppo debole (offset residuo) | `Ki × 1.1` |
| Duty che oscilla ±20 % tra tick successivi | Kd troppo alto (amplifica rumore) | `Kd × 0.9` |

**In RAMPA (ultimi 120 s):**

| Osservazione | Significato | Correzione |
|---|---|---|
| ≥ 4 attraversamenti della linea della rampa | PID "tira e molla", crescita a dente di sega | `Kp × 0.9` **e** `Kd × 1.1` |
| Sopra la linea > 8 °C per 60 s | troppo calore accumulato | `Ki × 0.9` |
| Sotto la linea > 8 °C per 120 s (dopo i primi 10 min di rampa) | potenza insufficiente | `Kp × 1.1` |

**Gate** (tutte le correzioni richiedono): sensore ok; temperatura stazionaria in tenuta
(max−min su 120 s < 2 °C); |errore| < 5 °C in tenuta iniziata da ≥ 3 min; rampa in corso
> 10 min; **massimo 1 correzione ogni 120 s** (tenute e rampe condividono lo stesso ticket).

**Clamp di sicurezza** (la correzione non può superare):

```
Kp ∈ [0.5, 50]      Ki ∈ [0.01, 2]       Kd ∈ [0, 5]
```

Tre livelli di difesa: gate severo (mai correggere su dati sporchi) + passo ±10 % alla
volta (≈7 correzioni valide = ~15 min per raddoppiare un parametro) + clamp assoluti.
Viene corretta SOLO la fascia della tenuta/rampa in corso. Persistenza immediata in NVS
(`pid_kp_1..3`, `pid_ki_1..3`, `pid_kd_1..3`). Interruttore: `AUTOTUNE_ENABLED = true` nel
codice. Integrale azzerato ad avvio programma e a ogni cambio segmento curva.

## 6. Sicurezza (fail-safe per costruzione)

1. **SSR forzato OFF** fuori dal RUNNING, ad avvio/stop programma, e su errore sensore.
2. **Stop sovratemperatura**: `OVER_TEMP_C = 1250 °C` (1200 di esercizio + 50 di margine),
   isteresi 10 °C → SSR off, schermata rossa piena (SOVRATEMP), uscita solo con long press
   (azione umana deliberata).
3. **Watchdog hardware**: `esp_task_wdt`, timeout 10 s, panic reset — un loop bloccato non
   può lasciare l'SSR in uno stato ignoto.
4. **Watchdog salita lenta**: se la temperatura resta **> 15 °C sotto la curva** per
   **60 minuti**, il controllore non può più assumere un comportamento normale e apre la
   schermata a 3 scelte:
   - `Attendi arrivo` — congela il setpoint al valore corrente della curva, attende
     l'arrivo della temperatura (±2 °C), con tetto di 1 h: dopo, la scelta viene
     riproposta;
   - `Allunga 30 min` — inserisce un punto piatto da 30 min alla posizione attuale e
     slitta il resto della curva (`SLOW_RISE_EXTEND_S = 1800`);
   - `Annulla` — annulla la cottura.
5. **Principio fail-safe**: mai scaldare con una lettura di temperatura stantia/congelata.

## 7. Modello dati e persistenza

```
struct Point   { uint32_t cumSec; float temp; }     // tempo cumulato dall'inizio (s), °C
struct Program { String name; Point points[20]; uint8_t pointCount; }
Program programs[10]; uint8_t programCount;
```

- **P0 è virtuale**: all'avvio la curva parte da `(0, temperatura misurata)` — la prima
  rampa parte sempre dalla temperatura reale del forno freddo (niente gap stagionale).
- La durata è **implicita** (differenza tra `cumSec` consecutivi).
- Limiti: 10 programmi, ≤ 20 punti, nome ≤ 20 caratteri, segmento ≤ 24 h, totale ≤ 24 h.

Chiavi NVS (namespace `kilnprogs`):

```
count
progX_name     (string)
progX_points   (uint) → pointCount
progX_pYc      (ulong  cumSec del punto Y)
progX_pYt      (float  temp del punto Y)
```

**Migrazione** dal formato legacy (vecchi step `temp,durata` → 2 punti ciascuno: tenuta
`A=(cum, temp_i)` → `B=(cum+durata, temp_i)`, poi salto allo step successivo).
Fallback: se `progX_points` manca (0xFFFFFFFF), carica e converte in RAM le chiavi vecchie;
al primo salvataggio le chiavi nuove esistono e il fallback non scatta più.
Oltre 20 punti → si conservano i primi 20.

Programma di fabbrica (se NVS vuoto): 4 punti
`(0,200) → (1h,200) → (3h,500) → (4h,500)`.

## 8. Build e caricamento

Prerequisiti:

- `arduino-cli` (o Arduino IDE 2.x)
- Core ESP32 **3.3.8** (`m5stack:esp32`)
- Librerie: **M5DinMeter**, **M5Unified**, **M5GFX** (installate dal package della scheda)

```bash
# una tantum: installa la piattaforma della scheda
arduino-cli core install m5stack:esp32

# compila
arduino-cli compile --fqbn m5stack:esp32:m5stack_dinmeter M5StackDin_KilnController

# carica (imposta la tua porta)
arduino-cli upload -p COM7 --fqbn m5stack:esp32:m5stack_dinmeter M5StackDin_KilnController
```

Build testata: ~555 KB di flash (~42 % di 1.3 MB), ~54 KB di RAM (~16 %).

## 9. Dipendenze

- `M5DinMeter` — HAL della scheda (include la libreria interrupt del `Encoder` PJRC)
- `M5Unified` / `M5GFX` — display, pulsante, gestione alimentazione
- Core `m5stack:esp32` 3.3.8

## 10. Struttura del progetto

```
M5StackDin_KilnController.ino   setup, loop principale, FSM, encoder, pulsante, helper I2C
kiln_ui.ino                     menu principale, editor curva, conferma cancellazione
kiln_run.ino                    schermata RUNNING (testata + grafico + etichette assi)
kiln_programs.ino               esecuzione curva, persistenza NVS, migrazione
kiln_pid.ino                    PID, attuazione SSR, osservatori, auto-correzione
docs/                           00_flusso_di_lavoro.md … 07_sicurezza.md
```

## 11. Documentazione

La cartella `docs/` contiene la documentazione di progetto completa (in italiano):
`00` flusso di lavoro, `01` hardware e note SSR, `02` lettura temperatura e il bug I2C,
`03` PID e auto-correzione, `04` persistenza e migrazione, `05` specifiche UI,
`06` editor curva, `07` sicurezza.

---

Demo:
https://github.com/Shark17e/M5DinMeter-KilnController/blob/main/C0008.MP4

Working demo:
https://github.com/user-attachments/assets/3adb103c-6c93-481f-8417-2194b0528034


# License / Licenza

**CC BY-NC 4.0** — Creative Commons Attribution-NonCommercial 4.0 International.

**EN:**
- You may freely use, study, share and modify this project **for non-commercial purposes**
  as long as you credit the author (**Shark17e**) and link back to this source.
- **Commercial use is strictly prohibited** without explicit permission from the author.
- Full text: [LICENSE](LICENSE) or https://creativecommons.org/licenses/by-nc/4.0/legalcode

**IT:**
- Uso **non commerciale** (studio, personale, didattico) libero citando l'autore
  (**Shark17e**) con link alla fonte.
- **L'uso commerciale è vietato** senza l'esplicito consenso dell'autore.
- Testo completo: [LICENSE](LICENSE) o https://creativecommons.org/licenses/by-nc/4.0/legalcode.it

© 2026 Shark17e — https://github.com/Shark17e
