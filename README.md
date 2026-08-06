# M5DinMeter-KilnController

**EN** — Open-source kiln controller for pottery / ceramics on a **M5Stack DinMeter** (ESP32-S3): PID with auto-correction, programmable temperature curves, full safety layer and a compact 240×135 UI driven by a single rotary knob.

**IT** — Controllore open-source per forno da ceramica su **M5Stack DinMeter** (ESP32-S3): PID con auto-correzione, curve di temperatura programmabili, strato di sicurezza completo e UI compatta 240×135 pilotata da un solo pomello.

---

## Table of contents / Indice
- [Features / Caratteristiche](#features)
- [Hardware & Wiring](#hardware--wiring)
- [User interface / Interfaccia](#user-interface)
- [PID & Autotune-less Correction / PID e auto-correzione](#pid--auto-correzione)
- [Safety / Sicurezza](#safety)
- [Build & upload / Build e caricamento](#build--upload)
- [Dependencies / Dipendenze](#dependencies)
- [Project structure / Struttura del progetto](#project-structure)
- [Documentation / documentazione](#documentation)
- [License / licenza](#license)

---

## Features / Caratteristiche

**EN**
- Enables full temperature curves: **2..20 break-point points** with a built-in graphical editor (no wizard steps)
- **PID** controller with **gain scheduling** (3 temperature bands), anti-windup, and on-field **self-correction** of PID gains while running (no relay/step-relay auto-tune required)
- **Time-proportional SSR** driving on a 10 s window (true DC blocks, safe for zero-cross SSR units)
- Real-time graph: programmed curve (yellow), measured temperature (green), live position marker
- Rotating encoder + single button (short/long press) navigation on a 240×135 display
- NVS persistence of programs and PID gains, with migration of legacy formats
- Slow-ramp watchdog ("salita lenta"), high-temperature absolute stop 1250°C, hardware watchdog and fail-safe SSR OFF

## Hardware & Wiring

**EN / IT**

| Component / Componente | Details / Dettagli |
|---|---|
| Board | **M5Stack DinMeter** (ESP32-S3), TFT 240×135 (`setRotation(1)`) |
| Encoder / Pomello | `DinMeter.Encoder`, ±2 counts/det, read via ISR (`readAndReset()`) |
| Button / Pulsante | `DinMeter.BtnA`, short press + long press (1 s) |
| SSR | **Fotek SSR-40 DA** (zero-cross, 40A, input **3-32 VDC**) driven by **GPIO 1 direct** (active HIGH) |
| Thermocouple | Type K via **KMeterISO** (MAX31855-compatible), **I2C address 0x66**, SDA=13, SCL=15, 400 kHz |
| Power supply | LiPo 3000 mAh (harness), USB 5 V recommended for the firing |

**Note (direct SSR drive):** the Fotek SSR-40 DA turns on with 3–32 VDC. The ESP32 GPIO provides **3.3 V**. The duty is delivered as a **full DC block** (time-proportional, not fast PWM), so the voltage is never reduced — the SSR always sees ~3.3 V when ON. Recommended: power from USB (5 V) during firings, keep the control wires short, single common ground. At boot the GPIO is floating for an instant: fit a pull-down (10 kΩ) between GPIO1 and GND if you want SSR OFF by construction during reset.

**IT**
- Board: **M5Stack DinMeter** (ESP32-S3), display TFT 240×135 (`setRotation(1)`)
- SSR: **Fotek SSR-40 DA** (zero-cross, 40 A, ingresso **3-32 VDC**) pilotato direttamente da **GPIO 1** (attivo alto)
- Termocoppia tipo K, KMeterISO compatibile MAX31855, **I2C 0x66**, SDA=13, SCL=15, 400 kHz
- **Nota sul pilotaggio SSR**: il Fotek richiede 3–32 VDC. L'ESP32 eroga **3.3 V**; il duty è un **blocco DC** (time-proportional a finestra di 10 s, non un PWM veloce) quindi la tensione non viene mai ridotta: quando l'SSR è on vede sempre ~3.3 V pieni. Consigli: alimentare da USB (5 V) in cottura, cavi di controllo corti, GND comune. Al boot il GPIO è fluttuante per un istante: se vuoi l'SSR spento per costruzione aggiungi una pull-down 10 kΩ tra GPIO1 e GND.

---

## User interface / Interfaccia

**EN.** Navigation uses only the rotary encoder + the single button:
- **Main menu:** program list (centered on the selection), big temperature at mid-screen on the right, red battery indicator top-left (size 2), last entries "Add Program" and "Turn off".
- **Add Program / Editor.** Insert name (encoder + short press add char, long press confirm), then enter the graphical curve editor directly: command bar `< T t > +P -P`:
  - `<` `>` select point; `T` edits temperature, `t` edits time (encoder ±5°C / ±5 min, short press confirm); `+P` adds a point (same T, midpoint in time); `-P` removes a point (min 2); `Salva`/`Annulla`.
  - P0 fixed = room temperature measured at firing start; times always increasing; temp 0..1200°C.
- **RUNNING** — 2-line header `Nome RAMPA/TENUTA n/n` + `SSR:XX%` (white, duty) + red battery, then `Tgt 950.0C` (yellow) and `TSens 523.4C` (green) + elapsed/total; full graph below with automatic X ticks (format `2h30m`), programmed curve (yellow), measured temperature (green) and position marker; on completion `FINE 4h00m SSR 2h10m`.

**ITAL.** Interazione con solo pomello + pulsante:
- **Menu principale** — lista programmi con selezione (righe 24px), temperatura ambiente grande a metà schermo a destra, batteria rossa in alto a sinistra (size 2), voci finali "Aggiungi Programma" e "Spegni".
- **Aggiungi Programma**: nome (alfabeto + short add char, long conferma), poi direttamente **editor curva** con barra comandi `< T t > +P -P`:
  - `<` `>` selezionano il punto, `T`/`t` entrano in modifica dei valori (±5°C / ±5 min al detent, short press conferma), `+P` aggiunge un punto dopo il selezionato (stessa T, tempo a metà), `-P` lo elimina (min. 2), `Salva`/`Annulla`.
  - P0 = temperatura ambiente misurata all'avvio della cottura; tempi sempre crescenti; temperatura 0..1200°C.
- **RUNNING**: testata 2 righe — nome + `RAMPA/TENUTA n/n`, `SSR:ON duty%`, batteria rossa; riga valori `Tgt 950.0°C` (giallo) / `TSens 523.4°C` (verde) + tempo trascorso/totale; grafico sottostante con etichette X a passo auto, curva gialla, misurata verde e pallino di posizione. A fine cottura: `FINE 4h00m SSR 2h10m`.

---

## PID & self-correction / PID e auto-correzione

**EN.** Classic positional PID with integration:
- 1 s base rate, derivative on the measured temperature (not on the setpoint)
- Anti-windup: integrates only when the output is not saturated and |error| > 0.2°C dead-band; integrator clamp ±300 °C·s
- Output clamp 0..100, then **time-proportional** on SSR window = 10 s
- **Gain scheduling** 3 bands: <400 / 400–900 / >900°C (configurable in NVS)
- **Self-correction while cooking**: observers on sectors (ramps and holdings) correct Kp/Ki/Kd automatically (min 1 correction per 120 s, clamp Kp 0.5..50, Ki 0.01..2, Kd 0..5)

**IT.** PID posizionale classico con drive a 1 s, derivata sulla misura, anti-windup integrale (±0.2 °C dead-band, anti-saturazion), clamp 0..100 e **time-proportional** su finestra SSR di 10 s. **Gain scheduling** a 3 fasce divisione <400/400–900/>900 °C e **auto-correzione** dei guadagni in cottura (via osservatori di settore) senza test a vuoto.

---

## Safety / Sicurezza

**EN**
- SSR **forced OFF** outside RUNNING, at start/stop, during ESTABLISH errors
- **Over-temperature stop**: 1250 °C threshold (hysteresis 10 °C) → SSR OFF, red screen, exit only by long press
- **Hardware watchdog** (`esp_task_wdt`, 10 s, panic reset) protects the loop
- **Slow-ramp safety** ("salita lenta"): if the temperature is >15 °C below the setpoint for 60 min → 3-choice screen: *Wait arrival* (freezes the setpoint, 1h cap then repeats), *Extend 30 min* (shift the remaining curve by 30 min), *Abort firing*
- Fail-safe principle: never heat on a stale/frozen temperature

**IT.** Tutta la safety è applicata nel file dedicato 07:
- SSR **off forzato** fuori RUNNING, a avvio/stop/con errore DB
- **Sovratemperatura 1250°C** → SSR off + schermata rossa (uscita solo long press)
- **Watchdog** su loop (10 s) per bloccare loop bloccati
- **Salita lenta**: T >15°C sotto la curva per 60 min → 3 scelte (Attendi arrivo / Allunga 30 min / Annulla)
- Regola d'oro: mai scaldare con misura congelata

---

## Build & upload / Build e caricamento

**Prerequisites / Prerequisiti**
- Arduino IDE / **`arduino-cli`** >= 1.x
- ESP32 core **3.3.8** for `m5stack` platform
- Libraries: **M5DinMeter**, **M5Unified**, **M5GFX**

```bash
# Add the m5stack board platform (once)
arduino-cli core install m5stack:esp32  # note: build tested with 3.3.8

# Compile / Compila
arduino-cli compile --fqbn m5stack:esp32:m5stack_dinmeter M5StackDin_KilnController

# Upload / Carica (set your port)
arduino-cli upload -p COM7 --fqbn m5stack:esp32:m5stack_dinmeter M5StackDin_KilnController
```

**EN.** Board `m5stack:esp32```:`m5stack_dinmeter`. Flash usage: ~42 % (≈555 KB), RAM ~16 %.

**IT.** Compilazione testata con core 3.3.8: flash ~42 % (≈555 KB), RAM ~16 %.

---

## Dependencies / Dipendenze (OS-level)

- Arduino ESP32 core ≥ 3.3.8 (`m5stack:esp32`)
- `M5DinMeter` (board HAL, includes the PJRC `Encoder` ISR library)
- `M5Unified`, `M5GFX` (display, buttons, power)

---

## Project structure / Struttura del progetto

```
M5StackDin_KilnController.ino   setup, loop, FSM, encoder, buttons, dashboard
kiln_ui.ino                     menu, editor curva, delete
kiln_run.ino                    RUNNING screen (header + graph)
kiln_programs.ino            curves, NVS persistence, migration
kiln_pid.ino                   PID, SSR, correction, observers
docs/
  00_flusso_di_lavoro.md        workflow & state table
  01_hardware.md                pins, wiring (SSR margin note)
  02_lettura_temperatura.md     KMeterISO read
  03_pid.md                     PID derails
  04_programmi_persistenza.md   NVS persistence
  05_interfaccia_grafica.md     UI specs (screens 1..6)
  06_inserimento_dati.md        curve editor
  07_sicurezza.md               failsafe, over-temp, watchdog, slow ramp
```

---

## Documentation / documentazione

The `docs/` folder contains the detailed specs (in Italian). Most useful to get started:
- `00_flusso_di_lavoro.md` — the big picture / la visione d'insieme
- `01_hardware.md` — pinout e note sul pilotaggio SSR
- `03_pid.md` — PID, anti-windup, gain scheduling, correction
- `05_interfaccia_grafica.md` — all screens
- `07_sicurezza.md` — safety layers

---

## License / Licenza

**CC BY-NC 4.0** — Attribution-NonCommercial 4.0 International.

**EN:**
- You may freely use, study, share, and modify this project **for non-commercial purposes** as long as you **credit Shark17e**.
- **Commercial use is strictly prohibited** unless I grant explicit permission.
- Full text: see [LICENSE](LICENSE) or https://creativecommons.org/licenses/by-nc/4.0/legalcode

**IT:**
- Uso **non commerciale** (studio, personale, didattico) libero **citando Shark17e**.
- **L'uso commerciale è vietato** senza esplicito consenso dello sviluppatore.
- Testo completo: vedi [LICENSE](LICENSE) o https://creativecommons.org/licenses/by-nc/4.0/legalcode.it

---

© 2026 Shark17e — [GitHub: Shark17e](https://github.com/Shark17e)

***This file is commented in two languages / File scritto in italiano e inglese.***