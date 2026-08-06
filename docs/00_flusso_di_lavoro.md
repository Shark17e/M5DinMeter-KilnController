# 00 — FLUSSO DI LAVORO
Ultimo aggiornamento: 2026-08-06
Stato: FINALE — tutte le specifiche confermate

## Scopo del progetto (refactoring strutturale)
1. **PID funzionante** al posto del bang-bang con isteresi ±10°C, con **autocorrezione adattiva dei parametri** (03_pid.md)
2. **Lettura temperatura affidabile** (regole I2C in 02_lettura_temperatura.md)
3. **Interfaccia grafica** con temperatura **scritta in grande** (05_interfaccia_grafica.md)
4. **Inserimento dati a curve** — rampe implicite, niente step intermedi (06_inserimento_dati.md)
5. **Sicurezza**: sovratemperatura, watchdog, salita lenta (07_sicurezza.md)
6. Split del file unico (1489 righe) in tab .ino organizzati per campo

## Come lavoriamo
- Un file di specifica per campo. Si ragiona campo per campo, si decidono le regole, si aggiorna il file, poi si implementa.
- Convenzione file: sezioni `REGOLE / DECISIONI / QUESTIONI APERTE`, data in testa.
- Niente implementazione prima che le regole del campo siano condivise.
- Le decisioni prese vanno segnate come tali (non restano solo nella conversazione).

## Dipendenze tra i campi
```
06 Inserimento dati (curva)  ──definisce──▶  04 Modello dati / persistenza
04 Modello dati              ──consumato da─▶ 03 PID (setpoint interpolato) e 05 UI
02 Lettura temperatura       ──indipendente──▶ usata da 03, 05, 07
03 PID                      ──comanda──▶     SSR (time-proportional)
07 Sicurezza                 ──si appoggia a─▶ 02 (fail-safe), 03 (duty=0, clamp), 04 (offset tempo)
```
- **02** (lettura temp) e **07** (sicurezza) sono i fondamenti: senza telemetria valida niente PID
- **06** è il campo che cambia di più: la sua decisione condiziona 04, 03 e 05
- **01** (hardware) è fisso, non cambia

## Riepilogo delle decisioni per campo
- **01 Hardware** (CONFERMATO): M5 DinMeter, TFT 240×135, encoder ±2, LiPo 3000mAh, SSR **Fotek SSR-40 DA** zero-cross 40A su GPIO1 (attivo alto), resistenze max **1200°C**, termocoppia K, KMeterISO I2C 0x66 (SDA=13, SCL=15, 400kHz), `Wire.begin(13,15,400000L)` dopo `M5.begin()`, probe boot 5 tentativi poi `while(true)`, `FIXED_Y_MAX=1200`. To-do: invertire segno encoder (orario = aumenta), ISR per conteggi persi; pull-down 10kΩ su GPIO1 (07)
- **02 Lettura** (CHIUSO): raw I2C manuale, STOP esplicita, delay 50µs, check 0x20==0, sentinella `INT32_MIN`, scala /100.0f, 5 fallimenti → `sensorError`, temp **grezza** al PID (niente EMA)
- **03 PID** (CONFERMATO): posizionale, dt=1s, derivata sulla misura, anti-windup (integra solo se non saturo e |e|>0.2), integrale ±300, output 0..100; default Kp=10 Ki=0.2 Kd=0.1; SSR time-proportional **10s** (`SSR_WINDOW_MS=10000`); niente librerie autotune (relè/Z-N = test a vuoto vietato); **autocorrezione** con osservatori in tenuta e in rampa (gate: sensore ok, max 1 correzione/120s, clamp Kp[0.5,50] Ki[0.01,2] Kd[0,5]); **gain scheduling 3 fasce** <400/400-900/>900°C, NVS `pid_kp_1..3` ecc., `AUTOTUNE_ENABLED=true`; integrale azzerato ad avvio e cambio segmento
- **04 Persistenza** (CONFERMATO): `Point{cumSec, temp}`, P0 virtuale = temp misurata all'avvio (non salvata), 2..20 punti, 10 programmi, nome ≤20 char, segmento ≤24h, NVS `progX_points`/`progX_pYc`/`progX_pYt`; migrazione vecchi step → 2 punti per passo, fallback `getUInt("progX_points", 0xFFFFFFFF)`, se >20 punti conserva i primi 20; preprogramma "Default" in forma nuova a 4 punti
- **05 UI** (CONFERMATO): menu senza titolo (batteria a sinistra, **TEMP GRANDE a destra** size 4, righe 24px ~4 visibili); RUNNING = testata 2 righe + grafico ~210×95 a tutta larghezza (curva gialla, T verde, pallino posizione, etichette X a **passo auto** min 45px → 1m/5m/10m/15m/30m/1h/2h/4h/6h/12h/24h, formato `2h30m`, taglio bordi); **zoom/pan rimossi**; stati speciali: errore sensore / fine; nome, conferma delete, avvio invariati
- **06 Editor** (CONFERMATO): ibrido grafico + barra comandi `[◀][T][t][▶][+][−]` / `[Salva][Annulla]`; P0 = temp misurata (elimina il gap stagionale); rampe implicite; tenuta = 2 punti stessa T; passi 5°C/5min per detent; tempo sempre crescente (+5min min); 0..1200°C; l'editor è la review
- **07 Sicurezza** (CONFERMATO): SSR off fuori RUNNING/errore/avvio/stop; **sovratemperatura 1250°C** → SSR off + schermata rossa + stop a mano; **watchdog** `esp_task_wdt` + **pull-down 10kΩ** consigliata su GPIO1; **salita lenta** (T >15°C sotto linea per 60 min) → schermata a 3 scelte [Attendi arrivo] (tetto 1h poi ri-chiede) / [Allunga 30 min] (linea ricalibrata dalla posizione attuale, premibile) / [Annulla]; tutte slittano i punti successivi via `programTimeOffset`

## Struttura file di codice (target)
```
M5StackDin_KilnController.ino   setup, loop, FSM, checkButton, handleEncoder, helper I2C
kiln_pid.ino                    costanti PID + runPID + updateSSR time-proportional + fail-safe
kiln_programs.ino               modello dati, save/load NVS (con migrazione), start/stop/update
kiln_run.ino                    schermata RUNNING: grafico, campionamento, stati speciali
kiln_ui.ino                     menu, editor, conferma delete, utility stringhe
```

## Flusso del programma (com'è oggi, invariato nella struttura)
```
setup():  I2C → probe KMeter (5 tentativi) → Preferences → loadPrograms → encoder=0 → SSR off
loop():   tick 1000ms:  kmReadReg(0x20)==0 → kmReadTemp() → currentTemp
                        │  (conteggio fallimenti → sensorError dopo 5)
                        ├─ programRunning → updateSSR() + campionamento adattivo + partialUpdate
                        └─ else → SSR forzato OFF
          DinMeter.update(); checkButton(); handleEncoder();
          switch (currentState): MAIN_MENU / RUNNING / CONFIRM_DELETE /
                                 ADD_PROG_NAME / ADD_PROG_NUMSTEPS / ADD_PROG_EDITSTEP /
                                 ADD_PROG_SAVE_OR_EXIT
          delay(10)
```

## Ordine di implementazione
1. Split in tab .ino **senza cambi di comportamento** (verifica: compile ok)
2. Campo 06/04: nuovo modello a curve + migrazione NVS + editor
3. Campo 03: PID + autocorrezione + osservatori al posto di updateSSR()
4. Campo 07: sovratemperatura, watchdog, salita lenta (schermata 3 scelte)
5. Campo 05: layout con temperatura grande e passo etichette auto
6. Verifica finale: `arduino-cli compile --fqbn m5stack:esp32:m5stack_dinmeter`

## Verifica build
```
arduino-cli compile --fqbn m5stack:esp32:m5stack_dinmeter M5StackDin_KilnController
```
Core `m5stack:esp32` e librerie `M5DinMeter`/`M5Unit-KMeterISO` già installate.

## Bug noti da sistemare strada facendo
- ~~`ssrOnTotalTime` incrementato due volte~~ RISOLTO: conteggio per tick (1s) nel PID (campo 03)
- ~~`shortTimeLabel()` morto~~ RISOLTO: usato da editor e etichette X a passo auto (campo 05)
- Encoder: `lastEncoderValue` segue la posizione raw + resto accumulato (PJRC conta ±1 e ±2), segno invertito in output. La version e con `lastEncoderValue += detents*2` driftava (tremolio) → sostituita. Da verificare sul banco

## Stato implementazione
Completata la sequenza 1-5 (split → curva+NVS+editor → PID+autocorrezione → sicurezza 07 → UI 05), compile ok ad ogni step:
```
arduino-cli compile --fqbn m5stack:esp32:m5stack_dinmeter M5StackDin_KilnController
```
Resta: verifica sul banco (prima cottura di prova) e pull-down 10kΩ su GPIO1 (consigliata, campo 07).
