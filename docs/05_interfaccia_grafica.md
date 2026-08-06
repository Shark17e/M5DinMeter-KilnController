# 05 — INTERFACCIA GRAFICA
Ultimo aggiornamento: 2026-08-06
Stato: CONFERMATO — tutte le schermate definite dall'utente

## Requisiti principali (utente)
- **Temperatura GRANDE a destra** nella schermata principale (menu)
- In RUNNING la temperatura ha la **dimensione normale** (testata + grafico in basso, niente colonne laterali: lo schermo è piccolo)
- Zoom/pan del grafico: **rimossi per ora** (riattivabili in futuro)

## Vincoli fisici (da 01)
- Schermo 240×135, font M5GFX: size 1 = 6×8px/char, size 2 = 12×16, size 3 = 18×24, size 4 = 24×32
- Un solo pulsante (short/long), encoder rotativo

---

## Schermata 1 — MENU PRINCIPALE
```
┌──────────────────────────────────┐
│ 78%                     │ 1200C  │  ← batteria a sinistra (size 2)
│                         │        │     TEMP GRANDE a destra (size 4, 120×32px)
│  > Cottura Bisquit              │
│    Raku                         │  ← righe 24px (4 visibili), selezione blu
│    Essiccazione                 │
│  + Aggiungi Programma           │
│    Spegni                       │
└──────────────────────────────────┘
```
- **Niente titolo**: la lista è auto-evidente
- Temp grande: intero, 5 char max ("1200"), aggiornata ogni 400ms con ridisegno parziale
- Lista centrata sul selezionato (righe 24px, ~4 visibili, scorrimento con encoder); ultime voci "Aggiungi Programma" e "Spegni"
- Long press su un programma = cancella (conferma in schermata dedicata)
- Batteria: `getBatteryPercent` (VMIN 3.2 / VMAX 4.2)

## Schermata 2 — NOME PROGRAMMA (invariata)
Alfabeto A-Z 0-9 `-_` + spazio, encoder scorre il carattere, short press aggiunge al nome, long press conferma. Max 20 char.
Nota: tutte le schermate di gestione programmi (nome, numero punti, editor) usano **textSize 1 esplicito** all'avvio del draw — la size "leakava" dal menu (4) e rendeva i testi enormi.

## Schermata 3 — EDITOR CURVA (nuovo, da 06)
```
┌────────────────────────────────────┐
│  1200┤        •                    │
│      │      •                      │  grafico (~85px): curva + P0 grigio
│    0 └───────► (0h,1h,2h)          │
│  Punto 2:  950°C  alle 2h 30m      │  valore selezionato (~15px, grande)
├────────────────────────────────────┤
│ [◀][T][t][▶][+][−]                 │  riga comandi 1 (6 voci)
│ [        Salva        ][ Annulla ] │  riga comandi 2
└────────────────────────────────────┘
```
- Barra comandi a **2 righe** (Salva/Annulla da soli nella seconda, non fit nella barra da 8 voci)
- Encoder scorre la barra; short press esegue; T/t entrano in modifica valore (±5°C / ±5min per detent), short conferma
- Long press = Annulla
- P0 grigio fisso a t=0 = temperatura misurata all'avvio (non editabile)
- Editor = review: nessuna schermata separata

## Schermata 4 — RUNNING (nuova, testata + grafico)
```
┌────────────────────────────────────────────────┐
│ Bisqua · Seg 3/5             SSR: ON    78/   │ ← R1 (size 1): nome+seg | SSR+batteria
│ 523.4C   Tgt 950.0C        2h30m / 4h00m      │ ← R2: temp size3, target size2 | tempo
├────────────────────────────────────────────────┤
│                                                │
│              grafico 210×95                    │  ← curva gialla + T verde
│   pallino bianco = posizione sulla curva      │     + pallino posizione
│                                                │
│    1h       2h        3h                      │  ← etichette X passo auto (sotto)
└────────────────────────────────────────────────┘
```
- Testata 2 righe (~32px): R1 nome + segmento / SSR (verde=ON, grigio=OFF, rosso=errore) + batteria; R2 temp attuale (size 3) + target (size 2) / tempo trascorso/totale
- Grafico a tutta larghezza (~2 00×95): curva programmata completa (gialla, rampe inclinate + tenute orizz), temperatura reale (verde), pallino posizione
- Etichette X con **passo automatico**: distanza minima 45px tra etichette, passo arrotondato al valore carino superiore (1m, 5m, 10m, 15m, 30m, 1h, 2h, 4h, 6h, 12h, 24h); formato `2h30m`; taglio ai bordi del grafico (niente sovrapposizioni, come oggi sulle X)
- Niente etichette Y (i numeri sono in testata)
- **Stati speciali**: errore sensore → R2 rossa "SENSORE ERRORE!" (SSR off, fail-safe 07); fine cottura → "FINE · 4h00m · SSR 2h10m"

## Schermata 5 — CONFERMA CANCELLAZIONE (invariata)
Testo rosso su nero: "Cancellare il Programma '...'?" — short = Conferma, long = Annulla.

## Schermata 6 — AVVIO (invariata)
"Inizializzazione..." al boot (probe KMeter, 5 tentativi; blocca se assente).

## Regole aggiornamento (invariate)
- Ridisegno parziale: solo aree "sporche" (temp menu 400ms; testata+grafico RUNNING al tick 1s; editor alla modifica)
- `needsUpdate` full-screen solo al cambio stato
- Colori: verde = temp attuale, giallo = curva programmata, rosso = SSR/errori, bianco = selezione, grigio = punti non selezionati/P0

## Nota futura
- Zoom/pan: implementazione rimossa, riinseribile come modalità opzionale (tasti brevi ciclano) se servirà in futuro