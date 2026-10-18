# WACK'D (Studio Wiga) — VST3

Reproduction en plugin VST3 (Windows 11 et macOS) de la pédale **Ibanez CF7 Chorus/Flanger** (série Tone-Lok, 1999).
Usage personnel. Visuel « Wack'd flangerus » (Studio Wiga).

## Installation via GitHub

1. Déposer à la racine du dépôt : `background.jpg`, `bars_lit.png`, `mode_sw.png`, `krazy_sw.png`, `knob_strip.png`, `Montserrat-Bold.ttf`, `CMakeLists.txt`, `PluginProcessor.h/.cpp`, `PluginEditor.h/.cpp`, `README.md` et le dossier `.github/workflows/build.yml`.
2. Le push lance la compilation (onglet **Actions**) : Windows (`windows-2022`) et macOS (binaire universel).
3. Télécharger l'artefact :
   - **Windows** : copier `Wackd.vst3` dans `C:\Program Files\Common Files\VST3\`
   - **macOS** : dézipper puis copier `Wackd.vst3` dans `~/Library/Audio/Plug-Ins/VST3/`
4. Relancer l'analyse des plugins dans le DAW.

## Ce que fait la vraie CF7 (documentation)

| Élément | Comportement d'origine | Reproduction dans le plugin |
|---|---|---|
| Cœur | Puce de retard **numérique** Mitsubishi **M50198** (la même que la DSC10), horloge pilotée par un oscillateur 74HC04 | Ligne à retard à interpolation d'Hermite, filtres anti-repliement / reconstruction 4 pôles autour, léger grain numérique |
| SPEED | Vitesse du LFO (triangle, IC5) | 0,06 → 10 Hz (Normal) |
| DEPTH | Amplitude du balayage | Chorus jusqu'à ±6 ms, Flanger jusqu'à 6,5 ms |
| DELAY TIME | Vers la droite : retard plus long, chorus plus profond. Vers la gauche en flanger : résonance plus aiguë | Chorus 3,5 → 20 ms, Flanger 0,25 → 6,25 ms |
| REGEN. | Quantité de réinjection | Jusqu'à 0,75 (chorus) / 0,92 (flanger), saturation douce de l'ampli op |
| MODE | Chorus / Flanger | identique |
| KRAZY Normal | Chorus / flanger classiques, signal direct + effet à niveau égal (la pédale **booste** légèrement le volume, comme l'original) | identique |
| KRAZY Wack'd | Signal direct coupé → vibrato ; LFO beaucoup plus rapide (sons « Korny », chirps, type ring-mod) ; aigus atténués ; volume égal au bypass | LFO 0,3 → 300 Hz, direct coupé, filtre ~4,5 kHz |
| Sorties | OUT1 = direct + effet ; **INV OUT2** = direct − effet (effet en opposition de phase → stéréo large) | Canal gauche = OUT1, canal droit = INV OUT2 |
| Footswitch / LED | Commutation électronique, LED rouge | Clic sur la plaque inférieure, LED rouge animée |

> En Wack'd, OUT2 est l'inverse exact de OUT1 (comme sur la pédale) : si la piste est repliée en mono, le signal s'annule. Rester en stéréo.

Sources : effectsdatabase.com (fiche et manuel CF7), mirosol.kapsi.fi (puce M50198, démontage), freestompboxes.org (analyse du mixage direct/effet), avis utilisateurs (gearbug, equipboard, lonephantom).

## Interface

- Fenêtre ouverte par défaut à **493 px de haut** ; impossible de la réduire en dessous. La pédale occupe presque toute la hauteur, comme Textural et Freeze Shimmer.
- **Clic droit** : taille (Default / 75 % / 100 %), presets d'usine, presets utilisateur 1 à 9 (charger / sauvegarder), table MIDI.
- **Potards** : noirs sobres, rendu haute définition pré-calculé (128 positions) : calotte noire usinée, jupe moletée noire, repère blanc ; la lumière reste fixe en haut à gauche, seuls le repère et le moletage tournent.
- **LED et barres** du visuel : LED bleu glace qui scintille légèrement, barres qui s'allument selon les potards (Speed = vitesse de la vague, Depth = amplitude, Delay Time = niveau de base, Regen = scintillement).
- **Ligne d'état** sous la LED : elle n'apparaît que lorsqu'un potard bouge (souris, automation ou MIDI), reste 2,5 s puis s'efface en fondu.
- **STUDIO WIGA** sous le footswitch, dans la police du logo Studio Wiga (Montserrat Bold, licence libre OFL), imprimé dans la même encre que « Wack'd » et « flangerus », avec un léger voile couleur papier dessous pour rester lisible sur le dessin.
- **Prises jack** : mêmes écrous chromés que la pédale Textural (OUT1 et INV. OUT2 à gauche, IN à droite).
- Fond noir avec les motifs de la pédale en transparence autour.

## MIDI

| Message | Fonction |
|---|---|
| CC 20 / 21 / 22 / 23 | Speed / Depth / Delay Time / Regen. |
| CC 24 | Mode (0-63 Chorus, 64-127 Flanger) |
| CC 25 | Krazy (0-63 Normal, 64-127 Wack'd) |
| CC 26 | Footswitch (≥ 64 ON, < 64 OFF) |
| CC 27 | Footswitch bascule (chaque appui) |
| Program Change 1-9 | Presets d'usine |

Tous les paramètres sont aussi automatisables et assignables en MIDI-learn dans le DAW.

## Presets d'usine

1. Classic Chorus · 2. Lush Stereo Chorus · 3. Jet Flanger · 4. Metallic Flange · 5. Slow Deep Sweep · 6. Wack'd Vibrato · 7. Korny Chirps · 8. Ring Mod Madness · **9. My Road**

**My Road** recrée l'intro de « My Way » (Limp Bizkit, 2000). Wes Borland y utilisait un flanger Ibanez réglé sur « whacked » (interview MusicRadar, citée par Equipboard) : MODE Flanger, KRAZY Wack'd, Speed 3,2 · Depth 5,5 · Delay Time 3 · Regen 4,5. Sur le disque, le son passe ensuite dans un Echoplex (écho à bande) et un ampli clair : ajoute un écho ~350 ms avec quelques répétitions après le plugin pour s'en approcher.

## Moteur audio (v1.4)

- **Speed** : 0,08 → 8 Hz en Normal ; 0,5 → 120 Hz en Wack'd (au-delà de ~25 Hz, la modulation devient audible comme des bandes latérales type ring-mod).
- **Chorus** : retard 4 → 22 ms (Delay Time), LFO triangle aux sommets arrondis, excursion limitée selon la vitesse pour garder un désaccord musical.
- **Flanger** : balayage exponentiel du retard (0,25 ms → 14 ms selon Delay Time et Depth), le peigne monte et descend régulièrement ; Regen jusqu'à 92 % avec saturation douce et amortissement des aigus dans la boucle.
- **Wack'd** : signal direct coupé (vibrato), aigus plus filtrés, niveau égal au bypass, deux sorties de même polarité (compatible mono).
- **Normal** : OUT1 = direct + effet, OUT2 = direct − effet, comme la pédale (stéréo large).
- Gain contrôlé (sortie ≈ niveau d'entrée), plus de grain numérique ni d'écrêtage.

Référence DSP : J. O. Smith, *Physical Audio Signal Processing* (CCRMA), chapitre Flanger.
