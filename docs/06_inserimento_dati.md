# 06 — LOGICA DI INSERIMENTO DATI (la curva)
Ultimo aggiornamento: 2026-08-06
Stato: CONFERMATO dall'utente (editor ibrido, punto 0 = temperatura misurata, passi 5°C/5min)

## Problema attuale (risolto)
Wizard con 2 campi per step (`temp` + `durata`): per una rampa bisognava spezzarla in tanti step intermedi → noioso, fragile, inclinazione non esplicita.

## REQUISITO (confermato)
1. Inserire una **curva**: punti di svolta `(tempo cumulato, temperatura)`
2. **L'inclinazione tra i punti è rispettata automaticamente** dal PID (rampa implicita = pendenza del segmento) → zero step intermedi
3. **Tenuta** = due punti alla stessa temperatura (segmento orizzontale)

## Modalità di input (CONFERMATA: ibrida)
**Editor ibrido**: grafico come anteprima sempre visibile + barra comandi in basso.

```
┌──────────────────────────────────┐
│  1200 ┤                          │
│       │      • P2                │  ← curva sempre visibile, si aggiorna live
│       │    •                    │
│   T   │  • P1                   │
│       │• P0 (grigio, "Ambiente")│
│     0 └──────────────────────►  │
│  Punto 2:  950°C   alle 2h 30m  │  ← valore del punto selezionato, grande
├──────────────────────────────────┤
│ [◀] [T] [t] [▶] [+] [−] [Salva] [Annulla]  │  ← barra comandi (ASCII: < > +P -P)
└──────────────────────────────────┘
```

### Barra comandi e interazione
| Comando | Azione |
|---|---|
| **◀ ▶** (schermo: `<` `>`) | seleziona il punto precedente/successivo |
| **T / t** | entra in modifica valore: l'encoder regola T (±5°C) o t (±5 min), short press conferma e torna alla barra |
| **+P** | aggiunge un punto dopo il selezionato (stessa T, tempo a metà verso il successivo) e lo seleziona |
| **−P** | elimina il punto selezionato (disattivo se restano solo 2 punti) |
| **Salva** | salva e torna al menu |
| **Annulla** | esce senza salvare (anche long press ovunque) |

> Da schermo: il numero di punti NON si sceglie a priori: dopo il nome si entra
> subito nell'editor con 2 punti e si aggiungono/eliminano con `+P`/`-P`.
> Frecce e `−` sono solo ASCII (`<` `>` `-`): i glifi Unicode ◀▶≠ non esistono nel font 6x8.

## Regole di vincolo (CONFERMATE)
- **Punto 0 fisso**: `t=0`, temperatura = **quella misurata all'avvio della cottura** (grigio, non edibile). Elimina il gap stagionale: il forno è all'esterno, parte da ~30°C d'estate e ~5°C d'inverno, la prima rampa parte sempre dalla T reale
- **Tempo sempre crescente** (mai curve all'indietro; minimo +5 min dopo il punto precedente)
- **Temperatura 0..1200 °C** (il nuovo `FIXED_Y_MAX` — non più 1400)
- **Punti: 2..20** (P0 incluso)
- **Durata segmento max: 24h**
- **L'editor È la review**: niente schermata "rivedi step" separata; quello che vedi è quello che viene eseguito

## Esempio (rampa lenta 0→100 in 1h, tenuta 30 min, 100→950 in 1h30, tenuta 1h)
```
P0(0, ambiente) ── P1(1h, 100) ── P2(1h30, 100) ── P3(3h, 950) ── P4(4h, 950)
                    rampa 1h       tenuta 30min     rampa 1h30     tenuta 1h
```
Costruzione: `+` P1 → T=100, t=1h → `+` P2 → t=1h30 (stessa T = tenuta) → `+` P3 → T=950, t=3h → `+` P4 → t=4h → Salva. 5 punti, zero step intermedi.

## Conseguenze (cascata)
- **04**: Point `{cumSec, temp}` al posto di Step `{temp, durata}`; durata implicita; migrazione NVS dal vecchio formato
- **03**: il setpoint segue la curva (linea interpolata tra punti); rampa = segmento inclinato, tenuta = orizzontale; osservatore rampa/tenuta (già in 03)
- **05**: editor con grafico + barra comandi; linea target inclinata nel grafico run
- **01**: `FIXED_Y_MAX` 1400 → 1200

## QUESTIONI APERTE
- Nessuna per l'ingresso dati. Rimanda (da dove il PID si interfaccia): azzeramento integrale al cambio di segmento curva (03)