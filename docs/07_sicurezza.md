# 07 — SICUREZZA E FAIL-SAFE
Ultimo aggiornamento: 2026-08-06
Stato: CONFERMATO

## REGOLE (già nel codice, NON rimuovere mai)
1. **SSR OFF fuori dal RUNNING**: in `loop()`, fuori dal RUNNING l'SSR è forzato LOW ad ogni tick (mai "caldo residuo" nel menu)
2. **SSR OFF su errore sensore**: `sensorError` (5 letture consecutive fallite, ≈5s) → SSR forzato OFF. *"never heat on stale/frozen temp"* — mai scaldare su temperatura stantia
3. **SSR OFF all'avvio**: `digitalWrite(SSR_PIN, LOW)` in `setup()` e all'avvio di ogni programma
4. **SSR OFF allo stop**: `stopProgram()` spegne subito
5. **Probe KMeter al boot**: se il modulo non risponde dopo 5 tentativi → blocco `while(true)` (non si parte senza telemetria)
6. **Nessuna lettura corrotta usata come temperatura**: sentinella `INT32_MIN`, check stato 0x20 (vedi 02)

## SOVRATEMPERATURA (nuovo)
- Soglia di intervento: **1250°C** (1200 di esercizio + 50 di margine)
- Superata → **SSR off immediato + programma terminato + schermata rossa "SOVRATEMP!"**; si esce solo a mano (il forno deve raffreddarsi)
- È la rete di salvataggio finale: indipendente da taratura PID, autocorrezione e stato del programma

## WATCHDOG (nuovo)
- **Software**: `esp_task_wdt` attivato, feed ad ogni giro di loop. Se il loop si blocca (es. I2C hang), l'ESP32 riavvia da solo in pochi secondi; a ogni avvio l'SSR riparte LOW (regola 3)
- **Hardware (necessario)**: resistenza **pull-down 10kΩ tra base del transistor-driver e GND** (o, senza driver, tra GPIO1 e GND). Durante il reset il GPIO non è garantito LOW: la pull-down rende l'SSR spento per costruzione, indipendente dal software. Senza questa, un boot anomalo può accendere l'SSR

## SALITA LENTA (nuovo)
- **Condizione**: in rampa, T più di 15°C sotto la linea per 60 min
- **Schermata di scelta** (con T attuale e scostamento):
```
SALITA LENTA
T è 18°C sotto la curva

[ Attendi arrivo ]
[ Allunga 30 min ]
[ Annulla cottura ]
```
- **[Attendi arrivo]**: setpoint = temperatura del punto finale del segmento; quando T ci arriva (±2°C) riparte il segmento successivo col suo ritmo pianificato. La pendenza del segmento attuale viene abbandonata (il forno non la sosteneva comunque), le successive restano pianificate. **Tetto: 1 ora di attesa** — se non arriva, la schermata ricompare e si può decidere di nuovo
- **[Allunga 30 min]**: il segmento attuale si estende di 30 min con **linea ricalibrata dalla posizione attuale** (non dal punto di partenza: pendenza più dolce). Premibile più volte finché non basta
- **[Annulla cottura]**: stop + SSR off (fail-safe)
- Tutte e tre le scelte **slittano i punti successivi** di un offset di tempo (`programTimeOffset`) — nessun cambio di modello dati; il tempo trascorso mostrato resta quello reale
- Il PID continua a funzionare durante l'attesa (fail-safe regola 2 incluso)

## FAIL-SAFE DEL PID (da 03)
- `sensorError` → duty = 0 → SSR off (regola 2 applicata al PID)
- Integrale azzerato a ogni avvio e a ogni cambio segmento
- Clamp del correttore: Kp[0.5,50] Ki[0.01,2] Kd[0,5] — l'autocorrezione non può mai spingere il PID fuori dai limiti
