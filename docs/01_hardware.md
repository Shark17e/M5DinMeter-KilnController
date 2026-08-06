# 01 — SPECIFICHE HARDWARE (M5 DinMeter)
Ultimo aggiornamento: 2026-08-06
Stato: CONFERMATO — dati forniti dall'utente

## Dispositivo
- **M5Stack DinMeter** (`#include <M5DinMeter.h>`, base M5Unified)
- Nota: "M5Dial" nel primo messaggio era un refuso — si resta su DinMeter

## Componenti e pinout
| Componente | Dettagli |
|---|---|
| Display TFT | 240×135 px, `setRotation(1)` (landscape) |
| Encoder rotativo | `DinMeter.Encoder`, ±2 counts per detent — **VEDI PROBLEMA SOTTO** |
| Pulsante | `DinMeter.BtnA`, short press + long press (1s) |
| Alimentazione | LiPo 3000 mAh, `DinMeter.Power.powerOff()` dal menu |
| SSR | **Fotek SSR-40 DA** (zero-cross, 40A, ingresso 3-32VDC), pilotato da **transistor NPN** su GPIO 1 (attivo alto) — vedi "Driver SSR" sotto |
| Elementi riscaldanti | resistenze, **max 1200°C** |
| Termocoppia | **tipo K** (KMeterISO = compatibile MAX31855) |
| KMeterISO | I2C addr **0x66**, SDA=**13**, SCL=**15**, 400 kHz |
| Serial | 115200 baud (debug) |
| Extra | **nessuno** (niente buzzer/LED) |

## Nota SSR / zero-cross
- SSR-40 DA è zero-cross per costruzione: commuta vicino allo zero di rete; con carico resistivo **nessun problema di inrush** → si può accendere/spegnere liberamente (time-proportional)
- Vincolo reale dello zero-cross: minimo tempo di accensione (> ~1 semi-periodo di rete) — irrilevante con la finestra SSR da 10s del PID (03)
- 40A di rating coprono tranquillamente qualche kW di elementi

## DRIVER SSR (necessario — ANNALISI 2026-08-06)
Il controllo del Fotek SSR-40 DA richiede **3-32 VDC** (~10 mA). Il GPIO dell'ESP32 dà **3.3 V**: solo 0.3 V di margine sul minimo, e sotto carico (corrente del LED interno + caduta sui cavi lung) il livello può scendere sotto 3 V → SSR non innesca o lo fa in modo intermittente. **Non affidabile pilotare l'SSR direttamente dal GPIO.**

**Circuito consigliato (NPN low-side, standard):**
```
GPIO1 ── 1kΩ ──┬─ Base NPN (2N2222 / BC547)
                └─ 10kΩ ── GND          (pull-down: SSR OFF a boot/reset)
Emitter  ────────────── GND
5V (USB/VIN) ──(+4) SSR-40 DA(−4)── collector
```
Quando GPIO1 è HIGH il transistor satura: il terminale − dell'ingresso SSR scende a ~0.2V, quindi il controllo (morsetto + a 5V, morsetto − al collector) vede **~4.8V**, ben sopra i 3V minimi. Si mantiene lo switching time-proportional (03): l'SSR resta un interruttore, non un PWM veloce.
- Alternativa equivalente: N-MOSFET **2N7000** (gate 100Ω + pull-down 10kΩ).
- **GND comune** ESP32–base dell'SSR obbligatorio.
- Se si opera da sola batteria LiPo 3.7V: attraverso il transistor si vedono ~3.5V (sopra la soglia ma con poco margine) → **per la cottura usare l'alimentazione USB (5V)**.

## PROBLEMA ENCODER (da risolvere nel refactor)
1. **Direzione invertita**: oggi la lettura aumenta ruotando in senso ANTIORARIO, richiesta è ORARIO → invertire il segno in `handleEncoder` (`detents = -diff / COUNTS_PER_DETENT`)
2. **Lettura non affidabile a rotazione veloce**: perde conteggi (o torna indietro) → l'Encoder è campionato nel loop con `delay(10)` (~100 letture/s): se i detent scattano più veloci si perdono → ipotesi: lettura ISR dedicata sui pin del quadrature o libreria encoder migliore. **CAMPO DI LAVORO, non risolto ora**

## Regole d'uso acquisite
- `Wire.begin(13, 15, 400000L)` **dopo** `M5.begin()`/`DinMeter.begin()`
- Probe del KMeter al boot: 5 tentativi, poi blocco `while(true)` se assente
- Encoder: `diff / COUNTS_PER_DETENT` → detents reali; valore attuale tenuto in `lastEncoderValue`
- Pulsante: debounce con `ignoreButtonUntil` (300ms), long press = 1000ms (`LONG_PRESS_DURATION`)

## Limiti fisici che impattano la UI
- Schermo 240×135 px: la temperatura "in grande a destra" (req. 05) ruba spazio al grafico — layout da progettare coi numeri veri dei font (M5GFX font 1 = 6×8px a size 1; "123.4C" a size 3 = 18×24px/char → 108px)
- Un solo pulsante: ogni azione UI è short/long press, niente tasti contestuali
- **MAX TEMP SISTEMA = 1200°C** → `FIXED_Y_MAX` (ora 1400) va portato a **1200**: grafico, editor e soglia di sicurezza (07) usano questo valore
- Batteria 3000 mAh: da mostrare % carica (`getBatteryPercent` esiste in SimpleTempReading, VMIN 3.2 / VMAX 4.2) — la posizione UI la decide 05

## QUESTIONI APERTE
- Soglia di sicurezza assoluta: 1200 + margine (es. 1250) → da decidere nel campo 07
- Encoder: testare rotazione lenta vs veloce per capire se il problema è il polling o i contatti meccanici