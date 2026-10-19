# DISSONA 2 — Collateral Effects

Emulazione software (VST3, AU e Standalone) del **Crumar DS-2** (1978): monofonico a due DCO con sezione POLY paraphonica a 44 note, due LFO, LFO mixer, VCF a 4 stadi CA3080, due ADSR.
Il motore è ricostruito dagli schemi originali (DWG1–18), dal manuale d'uso Crumar e dal reverse engineering di Enrico Bassi.

## Installazione (macOS)
Scarica `DISSONA-2-macOS.zip` dalla pagina Actions (o dalle Release), scompatta e fai doppio clic su **Installa DISSONA 2.command**. Riapri la DAW: lo strumento è tra quelli di *Collateral Effects*.

## Come è emulato

| Sezione | Circuito originale | Nel plugin |
|---|---|---|
| DCO | master clock 74LS221 (~1 MHz) → rate multiplier a 12 bit (P-434) → divisore di registro → contatore della forma (P-436) | rate multiplier simulato impulso per impulso (con il suo leggero jitter); sega a 15 gradini, triangolo a 30 passi, quadra, impulso quantizzato a 1/32 (50 % → 6 %). Gradini resi con polyBLEP: restano le immagini spettrali dei gradini (il timbro "ringy" del DS-2), non l'aliasing |
| Intonazione | codice N ∝ frequenza, DO acuto = 4095 | stessa tabella: errori di accordatura < 1 cent come l'originale |
| Vibrato, PITCH, leva | agiscono sul master clock (modulazione lineare in Hz) | idem: la modulazione forte è asimmetrica (sale meno di quanto scenda) |
| Glide | contatore avanti/indietro sul codice N | portamento lineare in Hz, a piccoli gradini; continua anche a tasto rilasciato |
| Keyboard control | DAC del codice N (−0,62 … −7,8 V) come corrente di riferimento del convertitore esponenziale | il taglio è moltiplicato per la frequenza della nota (inseguimento esatto) |
| VCF | 4 × CA3080 + BF244, 150 pF ×3 e 22 pF, guadagno 1,5 per stadio | modello nonlineare a 4 stadi (tanh degli OTA), quarto polo 6,8× più alto; autooscillazione a ≈ 8½ come da manuale |
| ADSR | 4,7 µF, attacco verso 5 V fermato a 4,3 V, reostati 1M antilog | stesse costanti di tempo (attacco da ~9 ms a ~9 s, decay/release fino a decine di secondi) |
| LFO 1 | integratore + Schmitt, 0,3–30 Hz (reostato a filo) | stessa legge della manopola; la sega è ottenuta invertendo il triangolo e quindi va a **frequenza doppia**, come nel circuito |
| LFO 2 | HBF4727 sul fronte della quadra | staircase discendente a 16 gradini e S&H "random ripetitivo" (stesso contatore con pesi invertiti), accoppiati in alternata |
| LFO delay | rampa dal gate che apre un CA3080 | dissolvenza in entrata degli LFO con DELAY IN, azzerata al rilascio |
| POLY | MK50240 (divisori 478…253) + HBF4727, scala a 8 gradini da 3 ottave di quadra (47K/100K/220K) | stesse frequenze e stessa scala, ottave in fase; i tasti chiudono direttamente le note (nessun rilascio, come l'originale); equalizzatore ±15 dB |
| NOISE | bianco / rosa con la rete 270K–47K+10n–1n | stessa rete |

## Comandi MIDI (Arturia KeyLab e simili)
Clic destro su qualsiasi comando → **MIDI Learn**. Assegnazioni predefinite:

| CC | Comando |
|---|---|
| 74 | CUTOFF FILTER |
| 71 | RESONANCE |
| 1 (mod wheel) | LFO mixer → OSC 1 (vibrato) |
| 7 | VOLUME |
| 5 / 65 | SPEED GLIDE / GLIDE IN |
| 73 / 72 | VCA attack / release |
| 75 | VCF decay |
| 64 | sustain (tiene le note, anche nella POLY) |
| Pitch bend | muove la leva PITCH BENDING (±terza minore come l'originale, regolabile) |

## Impostazioni (clic sulla targa DISSONA 2)
- **Pitch bend MIDI**: ampiezza della rotella.
- **Estensione**: tutte le note MIDI, oppure solo i 44 tasti FA–DO (MIDI 41–84) dell'originale.
- **Trigger**: singolo (originale: il legato non riattacca gli inviluppi) o multiplo.
- **Deviatore VCF/VCA**: il manuale Crumar dice che in IN l'ADSR del VCF comanda anche il VCA; sulla macchina di Bassi lo stesso deviatore sceglie tra ADSR del VCA e semplice gate. Sono disponibili entrambe.
- **Uscite**: tutto sull'uscita principale, oppure POLY separata (sinistra = principale, destra = POLY non filtrata), come i due jack MAIN e POLY.
- **Qualità**: sovracampionamento 2× (normale) o 4×.

## Confronto con registrazioni dell'originale
Da una registrazione dimostrativa di 14 minuti di un DS-2 (mono, senza effetti):
- il dente di sega ha **15 gradini** per periodo: lo spettro ha zeri esatti alle armoniche 15, 30, 45 e 60 (il plugin ora fa lo stesso);
- l'impulso al 25 % ha zeri esatti alle armoniche 4, 8, 12, 16, riprodotti dal plugin;
- l'intonazione dei DCO resta entro ±0,7 cent su note tenute: nessuna deriva da simulare;
- il filtro in autooscillazione ha 3ª armonica a −28…−39 dB e 5ª a −41…−59 dB: il limite d'escursione degli stadi è stato reso morbido per ottenere lo stesso spettro (prima la 5ª era a −35 dB).

## Scelte non documentate negli schemi
Alcuni valori non si leggono negli schemi e sono stimati: frequenza esatta del master clock e prescaler dopo il rate multiplier, profondità massime dell'LFO mixer, frequenze di svolta dell'equalizzatore POLY, tempi del glide e del delay degli LFO. Se hai registrazioni dell'originale (note singole per forma d'onda, sweep del filtro, glide, LFO), si possono tarare.

## Compilare
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target Dissona2_VST3 Dissona2_AU Dissona2_Standalone
```
Test del motore: `c++ -O2 -std=c++17 -Isrc tests/engine_test.cpp -o engine_test && ./engine_test`.

Font: Michroma (SIL Open Font License) e Ultra (Apache 2.0), in `resources/fonts`. JUCE è AGPLv3/commerciale.
