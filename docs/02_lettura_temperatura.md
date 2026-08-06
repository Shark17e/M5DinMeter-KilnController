# 02 — LETTURA TEMPERATURA (KMeterISO)
Ultimo aggiornamento: 2026-08-06
Stato: REGOLE CONFERMATE dall'esperienza sul campo

## STATO ATTUALE
Lettura **raw I2C** a mano, **non** la libreria M5Unit-KMeterISO. Stessa tecnica in `SimpleTempReading.ino`.

Registri del modulo:
- **0x00**: temperatura, int32 big-endian, in **centesimi di °C** (raw 52345 → 523.45°C)
- **0x20**: registro di stato; **0 = OK**, diverso da 0 = non leggere

## Errore riscontrato (storia del fix)
- **Sintomo**: temperatura errata/sballata — saliva anche con SSR off e LED spento → sembrava un guasto SSR, era telemetria inaffidabile.
- **Causa**: `Wire.endTransmission()` senza argomento usa il **repeated start**; il KMeterISO richiede una **STOP esplicita** tra la scrittura del registro e la lettura. Senza STOP il modulo non risponde → dati non validi. In più: nessun check dello stato, nessuna gestione delle letture fallite → dati corrotti pilotavano l'SSR.
- Nota terminologica: "modifica alla libreria" = l'insieme di regole sotto, applicate a mano in un helper I2C.

## REGOLE (obbligatorie, NON rimuovere mai)
1. **STOP esplicita**: `Wire.endTransmission(true)` prima di ogni `Wire.requestFrom` — mai repeated start
2. **delay 50µs** dopo la write del registro (assestamento del modulo)
3. **Check stato prima**: `kmReadReg(0x20) == 0` → solo allora leggere la temperatura
4. **Sentinella**: se `Wire.available() < 4` → restituire `INT32_MIN`, la lettura è da scartare
5. **Scala**: `currentTemp = raw / 100.0f`
6. **Conteggio fallimenti**: ogni lettura fallita → `sensorFailCount++`; a `SENSOR_FAIL_THRESHOLD = 5` (≈5s) → `sensorError = true`
7. **MAI pilotare l'SSR su temperatura stantia/corrotta**: `sensorError` → SSR forzato OFF (vedi 07_sicurezza.md)

## Ciclo di lettura (com'è oggi, ogni 1000ms)
```
if (kmReadReg(0x20) == 0) {
  rawT = kmReadTemp();
  if (rawT != INT32_MIN) { currentTemp = rawT/100.0f; sensorFailCount=0; sensorError=false; }
  else { sensorFailCount++; }
} else { sensorFailCount++; }
if (sensorFailCount >= 5) sensorError = true;
```
`TEMP_READ_INTERVAL = 1000` ms: 1 lettura al secondo, allineata al tick del PID (03).

## DECISIONI
- Campo 02 CHIUSO: il resto (frequenza lettura, scala, gestione errori, consumo nel loop) **funziona correttamente** così com'è — nessuna modifica oltre alle regole sopra
- Temperatura **grezza** al PID (niente filtro EMA): la termocoppia digitale è già pulita; si rivaluta solo se il Kd disturba
