# WACK'D (Studio Wiga) — VST3

Reproduction en plugin VST3 (Windows 11 et macOS) de la pédale **Ibanez CF7 Chorus/Flanger** (série Tone-Lok, 1999).
Usage personnel. Visuel « Wack'd flangerus » (Studio Wiga).

## Installation via GitHub

1. Déposer à la racine du dépôt : `background.jpg`, `bars_lit.png`, `bars_lit_amber.png`, `mode_sw.png`, `mode_sw_r.png`, `krazy_sw.png`, `krazy_sw_r.png`, `knob_strip.png`, `Montserrat-Bold.ttf`, `CMakeLists.txt`, `PluginProcessor.h/.cpp`, `PluginEditor.h/.cpp`, `README.md` et le dossier `.github/workflows/build.yml`.
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

> Sortie réglable (clic droit > Output) : **Mono compatible** par défaut, ou **CF7 stereo** d'origine (OUT2 = direct − effet). Voir « Placement dans Reaper » plus bas.

Sources : effectsdatabase.com (fiche et manuel CF7), mirosol.kapsi.fi (puce M50198, démontage), freestompboxes.org (analyse du mixage direct/effet), avis utilisateurs (gearbug, equipboard, lonephantom).

## Interface

- Fenêtre ouverte par défaut à **493 px de haut** ; impossible de la réduire en dessous. La pédale occupe presque toute la hauteur, comme Textural et Freeze Shimmer.
- **Clic droit** : taille (Default / 75 % / 100 %), presets d'usine, presets utilisateur 1 à 9 (charger / sauvegarder), table MIDI.
- **Potards** : noirs sobres, rendu haute définition pré-calculé (128 positions) : calotte noire usinée, jupe moletée noire, repère blanc ; la lumière reste fixe en haut à gauche, seuls le repère et le moletage tournent.
- **LED et jauge** : bleu glace en Chorus, jaune-orangé (même intensité) en Flanger ; la jauge sous la LED s'allume de gauche (0 %) à droite (100 %) selon le dernier potard tourné.
- **Switchs MODE / KRAZY** : le levier se déplace réellement (gauche = Chorus / Normal, droite = Flanger / Wack'd).
- Pas d'indication d'alimentation (DC9V) en haut de la pédale.
- **Ligne d'état** agrandie sous la LED (texte 20 px / 15,5 px à 100 %) : elle n'apparaît que lorsqu'un potard bouge, reste 2,5 s puis s'efface en fondu.
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

**My Road** recrée l'intro de « My Way » (Limp Bizkit, 2000), jouée sur cette pédale : MODE Chorus, KRAZY Wack'd, Speed 95,6 % · Depth 1,6 % · Delay Time 73,5 % · Regen 12,3 %. Sur le disque, le son passe ensuite dans un Echoplex (écho à bande) et un ampli clair.

## Placement dans Reaper (avant / après un simulateur d'ampli)

La vraie CF7 sort sur deux jacks : OUT1 = direct + effet, OUT2 = direct − effet (effet inversé). Les versions ≤ 1.4 reproduisaient ça tel quel. Or un simulateur d'ampli comme Genome (Two Notes) traite la guitare en **mono** : il additionne gauche + droite, l'effet inversé s'annule et il ne reste que le son direct (en Wack'd, où il n'y a pas de son direct, le son disparaît complètement). Placé **après** Genome, le plugin recevait déjà le son d'ampli et sortait en stéréo, donc ça marchait. Arc, Freeze Shimmer, Textural et Ring Think n'ont pas de sortie inversée, d'où la différence.

Depuis la v1.5, la sortie par défaut est **Mono compatible** : la droite utilise une seconde lecture du retard décalée de 90° au lieu de l'inversion, ce qui garde la largeur stéréo sans annulation. Le plugin se place donc **avant** Genome comme les autres. L'option **CF7 stereo** reste disponible (clic droit > Output) pour un usage après l'ampli.

## Moteur audio (v1.5, modélisé d'après le schéma)

- **LFO** (IC5 : intégrateur + trigger de Schmitt) = triangle. SPEED = potard log 500 kΩ + 5 kΩ → plage 1:100 : Chorus 0,1 → 10 Hz, Flanger 0,05 → 5 Hz (le switch MODE commute les condensateurs de temporisation) ; Wack'd : Chorus 0,6 → 40 Hz, Flanger 0,3 → 20 Hz.
- **KRAZY Wack'd** : modulation fixe ajoutée par R44 (47 kΩ), indépendante de DEPTH (d'où un effet marqué même DEPTH au minimum), signal direct coupé, aigus filtrés (~6 kHz), niveau égal au bypass.
- **Chorus** : retard 5 → 25 ms, LFO triangle arrondi, excursion jusqu'à 60 % du retard (désaccord nettement plus marqué que la v1.4), effet un peu plus fort que le direct.
- **Flanger** : balayage exponentiel 0,2 → 16 ms, Regen jusqu'à 95 % (saturation douce, anti-DC, amortissement des aigus dans la boucle).
- Gain contrôlé, aucun écrêtage en usage normal.

Référence DSP : J. O. Smith, *Physical Audio Signal Processing* (CCRMA), chapitre Flanger.
