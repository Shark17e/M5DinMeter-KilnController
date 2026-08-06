# 04 — MODELLO DATI E PERSISTENZA (programmi)
Ultimo aggiornamento: 2026-08-06
Stato: CONFERMATO — modello a punti di curva + migrazione NVS

## STATO ATTUALE (da SOSTITUIRE)
```
struct Step    { float temp; uint32_t duration; }        // temp °C, durata s
struct Program { String name; Step steps[20]; uint8_t stepCount; }
Program programs[10]; uint8_t programCount;
```
Chiavi NVS attuali (namespace `kilnprogs`): `count`, `progX_name`, `progX_steps`, `progX_sYt` (temp), `progX_sYd` (durata). Limiti: 10 programmi × 20 step, 24h/step, nome ≤ 20 char. Default pre-caricato "Default" (200°C/1h → 500°C/2h) se NVS vuoto.

## NUOVO MODELLO (confermato da 06)
```
struct Point   { uint32_t cumSec; float temp; }          // tempo cumulato dall'inizio, °C
struct Program { String name; Point points[MAX_CURVE_POINTS]; uint8_t pointCount; }
Program programs[10]; uint8_t programCount;
```
- `MAX_CURVE_POINTS = 20` (include il punto di partenza → 19 di svolta max)
- La durata è **implicita** = differenza tra `cumSec` di punti consecutivi → eliminata
- **P0 NON è salvato**: all'avvio della cottura il setpoint parte da `(0, temperatura_misurata)`; la prima rampa va dal misurato a points[0]
- Limiti: `MAX_PROGRAMS=10`, nome ≤ 20 char, segmento ≤ 24h (come oggi), `FIXED_Y_MAX = 1200` (era 1400)

## Persistenza NVS — nuove chiavi
```
count
progX_name     (string)
progX_points   (uint)     → pointCount (numero di punti salvati; P0 non esiste in NVS)
progX_pYc      (ulong   cumSec del punto Y, Y = 0..pointCount-1)
progX_pYt      (float   temp °C del punto Y)
```
Salvataggio scrive SOLO le chiavi nuove.

## Migrazione dal vecchio formato (fallback a runtime, come concordato)
Lo step vecchio (temp_i, durata_i) era "salto immediato a temp_i + tenuta di durata_i". Si converte in 2 punti:
```
cum = 0
per ogni vecchio passo i:
  point A = (cum,               temp_i)
  point B = (cum + durata_i,    temp_i)
  cum = cum + durata_i
```
Risultato: A-B = tenuta orizzontale; B_i e A_{i+1} stesso tempo = salto istantaneo (identico al vecchio comportamento). Se il totale supera 20 punti (vecchi 20 passi → 40 punti), si conservano i primi 20 (i passi a inizio programma): atteso raro (il Default fa 4 punti).
```
in loadPrograms():
  c = getUInt("progX_points", 0xFFFFFFFF)          // 0xFFFFFFFF = assente
  if (c == 0xFFFFFFFF):                           // vecchio formato → migra
      leggi vecchio progX_steps → per ogni passo crea A e B come sopra
  else: leggi nuovo formato pYc/pYt
```
- NVS è key-value non distruttivo → al primo boot post-upgrade il fallback legge il vecchio formato e converte in RAM; al primo Salvataggio successivo le chiavi nuove esistono e il fallback non scatta più
- Nessuna migrazione bulk, nessuna versione esplicita

## Regole esecuzione (nuove, coerenza con 03 e 06)
- L'avanzamento è **per tempo** lungo la curva: `elapsed = now − programStart`; si trova il segmento corrente (punto i..i+1) per cumSec; setpoint = interpolazione lineare sulla linea del punto
- P0 virtuale a `(0, temp_misurata)` → primo segmento non è in NVS, si calcola a runtime
- Fine programma = oltre l'ultimo `cumSec`
- Rampa a tempo senza catch-up (dichiarato): se il forno è in ritardo si passa comunque oltre — catch-up eventuale = soglia tipo PIDKiln, campo di lavoro (03)
- Campionamento grafico: campioni distribuiti per tempo reale lungo tutta la curva (coerente con 05)

## QUESTIONI APERTE
- Mantenere il preprogramma "Default" pre-caricato? (Sì, nella forma nuova a 4 punti: (0,200)→(1h,200)→(3h,500)→(4h,500))
- Nome al massimo 20 char va bene (è anche il campo di testo dell'editor)