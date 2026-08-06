# 03 — PID
Ultimo aggiornamento: 2026-08-06
Stato: BOZZA — struttura PID confermata; autocorrezione disegnata e confermata

## STATO ATTUALE (da SOSTITUIRE)
Bang-bang con isteresi ±10°C in `updateSSR()`:
```
T < target − 10 → SSR ON
T > target + 10 → SSR OFF
```
Problema: controllore a due stati, oscillazione permanente (overshoot/undershoot determinati dall'inerzia termica), niente controllo della potenza media, isteresi bassa = commutazioni continue (usura SSR).

## Modello PID scelto (port dal peltier_controller, adattato)
PID **posizionale** discretizzato, **dt = 1 s fisso** (allineato al tick di lettura termocoppia: il forno ha inerzia di decine di secondi, calcolare più spesso è matematica finta; il PIDKiln usa finestra 5s).

```
e(t)    = setpoint − currentTemp
integrale += e * dt                  (solo se condizioni anti-windup soddisfatte)
derivata  = (currentTemp − prevTemp) / dt      ← derivata della MISURA, non dell'errore
duty      = Kp·e + Ki·integrale + Kd·derivata
duty      = clamp(duty, 0, 100)      (%)
```

### Regole anti-kick / anti-windup (portate dal peltier)
1. **Derivata sulla misura**: al cambio step/setpoint l'errore fa un gradino; derivando la misura niente impulso → niente kick
2. **Integra solo se**: output non saturato (duty non a 0 o 100) **e** |e| > deadband
3. **Deadband** = 0.2°C: sotto questa errore l'integrale si ferma → niente "creeping" del duty a regime
4. **Clamp integrale** = ±300 °C·s (con Ki=0.2 → contributo max ±60% duty)
5. **Clamp output** = 0..100 (SSR non raffredda: solo riscaldamento)

### Default di partenza (per fascia, poi affinati in NVS dall'autocorrettore)
```
Kp = 10.0   Ki = 0.2   Kd = 0.1    (per tutte e 3 le fasce, stile PIDKiln)
```
Razionale: stessi ordini di grandezza del PIDKiln (forno + SSR), non del peltier (Kp=30/Ki=2/Kd=0 su PWM 0..255).

## Attuazione SSR — time-proportional (il kiln NON ha il PWM del peltier)
Un SSR a zero-cross non modula: si usa una **finestra temporale fissa**, SSR on per `duty × finestra`:
```
SSR_WINDOW_MS = 10000   (10 s)
if (now − windowStart >= SSR_WINDOW_MS) windowStart = now;
ssrState = (now − windowStart) < (duty * SSR_WINDOW_MS / 100);
```
- Risoluzione: tick 1s / finestra 10s = **10% di passo**
- Commutazione max ~1/s → usura SSR trascurabile
- Finestra 2s darebbe passi da 50% (troppo grossolano); 5s = passo 20% (da provare se serve reattività)

## REQUISITO UTENTE: autocorrezione dei parametri PID (CONFERMATO)
**Obiettivo**: valori iniziali sbagliati → il sistema si corregge da solo, durante le cotture vere, senza test a vuoto (costi del forno).

**Scelta**: nessuna libreria di autotune (br3ttb/QuickPID/PetalPID ecc. usano tutte il metodo a relè/Ziegler-Nichols = oscillazione forzata = test a vuoto, vietato). Si implementano le **regole diagnostiche industriali** (Bentrup, Bloor) come osservatore passivo su tenute E rampe delle cotture reali. Riferimento: "continuous parameter adaption" dei controller professionali.

### Osservatore in TENUTA (dwell)
Su finestra scorrevole di 120 s, solo in tenuta:

| Osservazione | Misura concreta | Sintomo | Correzione |
|---|---|---|---|
| **Oscillazione** | ≥ 4 attraversamenti dello zero dell'errore nella finestra | Kp troppo forte (banda proporzionale troppo stretta) | `Kp × 0.9` |
| **Overshoot** | picco di T − target > 3°C dopo l'ingresso in tenuta | Ki troppo forte (windup residuo in salita) | `Ki × 0.9` |
| **Offset residuo** | media di |errore| su 120 s > 2°C con T piatta | Ki troppo debole | `Ki × 1.1` |
| **Uscita nervosa** | duty balla ±20% tra tick successivi (SSR switch nervosi) | Kd troppo alto (amplifica rumore) | `Kd × 0.9` |

### Osservatore in RAMPA (nuovo, richiesto dall'utente)
Motivazione fisica: sotto ~500°C le dispersioni sono minime → quasi tutta la potenza va nel forno → la T sale molto in fretta; una rampa lenta richiede potenza bassa e precisa, altrimenti la T supera la linea → zigzag "a dente di sega" invece di crescita lineare. Il bersaglio del PID in rampa è la **linea** (setpoint mobile): si misura la distanza `T_att − T_linea` e gli attraversamenti della linea.

| Osservazione (ultimi 120 s, in rampa) | Sintomo | Correzione |
|---|---|---|
| **Zigzag sulla linea**: ≥ 4 attraversamenti della linea | PID "tira e molla", crescita non lineare | `Kp × 0.9` **e** `Kd × 1.1` (manca il freno) |
| **Sopra la linea** persistente: `T_att − T_linea > 8°C` per 60 s | Troppo calore accumulato (windup in rampa) | `Ki × 0.9` |
| **Sotto la linea** persistente: `T_linea − T_att > 8°C` per 120 s, ESCLUSI i primi 10 min di rampa | Potenza insufficiente: Kp troppo molle | `Kp × 1.1` |

Nota: tutte le rampe sono > 10 minuti (confermato dall'utente), quindi l'esclusione iniziale non blocca mai l'osservazione. Se il Kd arriva al clamp senza risolvere lo zigzag → la rampa richiesta è fisicamente più lenta di quanto il forno sappia fare: NON è un problema di PID ma di programma (da segnalare, non correggere).

### Gate (condizioni di validità delle osservazioni)
Tutte le correzioni richiedono:
- sensore OK (niente sensorError), programma in corso
- temperatura stazionaria in tenuta (max−min su 120 s < 2°C) oppure rampa in corso da > 10 min
- in tenuta: |errore| < 5°C e tenuta iniziata da ≥ 3 min
- **massimo 1 correzione ogni 120 s** (tenuta e rampa condividono lo stesso ticket)

### Range di sicurezza (clamp) — i limiti che la correzione NON può superare
```
Kp ∈ [0.5, 50]     (default 10 — metà scala)
Ki ∈ [0.01, 2]     (default 0.2)
Kd ∈ [0, 5]        (default 0.1; può arrivare a 0 — i tecnici dei forni spesso lo lasciano a 0)
```
Difese a 3 livelli: gate severo (mai correggere su dati sporchi) + passo ±10% per volta (per raddoppiare un parametro servono ~7 correzioni valide = ~15 minuti) + clamp assoluti.

### Gain scheduling a 3 fasce (dall'industria dei forni)
Il guadagno del processo cambia con la temperatura (sotto ~400-600°C domina convezione/conduzione, sopra ~800°C domina la radiazione con legge T⁴ → variazione reale fino a 7:1): gli stessi parametri che tengono bene a 300°C fanno oscillare a 1000°C. Si usa il set della fascia del **target attuale** (il setpoint, stabile):
```
Fascia 1: target < 400°C    → Kp₁ Ki₁ Kd₁
Fascia 2: target 400–900°C  → Kp₂ Ki₂ Kd₂
Fascia 3: target > 900°C    → Kp₃ Ki₃ Kd₃
```
L'autocorrettore modifica SOLO la fascia della tenuta/rampa in corso. Ogni fascia converge da sé (le cotture hanno tenute in ogni fascia: essiccazione ~200, bisquit ~950, vetrina ~1100).

### Persistenza (NVS, namespace `kilnprogs`)
```
pid_kp_1, pid_ki_1, pid_kd_1   // fascia 1
pid_kp_2, pid_ki_2, pid_kd_2   // fascia 2
pid_kp_3, pid_ki_3, pid_kd_3   // fascia 3
```
Al boot: chiave assente → default 10/0.2/0.1 per quella fascia. Ogni correzione = scrittura immediata della fascia toccata (al più 1 scrittura/120 s in tenuta/rampa → usura NVS trascurabile).

### Interruttore on/off
Costante `AUTOTUNE_ENABLED = true` nel codice (futuro switch di menu). Se disattivo: PID fisso sui valori correnti, nessuna osservazione.

## Fail-safe (interazione con 07)
- `sensorError` → duty = 0, SSR OFF (mai scaldare su temp stantia)
- Programma non in corso → duty = 0
- Reset integrale a ogni avvio programma
- L'autocorrezione non è un salvavita: forno degradato → parametri al clamp e stop; la vera rete di sicurezza resta la soglia assoluta di temperatura (07)

## QUESTIONI APERTE
- Duty massimo durante la rampa (cap in % per non stressare elementi/SSR)? Il PIDKiln non lo ha, ma utile per forni delicati — decide 07
- Azzeramento integrale al cambio punto curva (06): in rampa l'errore è costante e l'integrale cresce; valutare reset a ogni nuovo segmento
