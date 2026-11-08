# MELLOLAB v1.0

Teclado de cinta tipo Mellotron, VST3 para FL Studio (Windows 64 bits). Ligero: sin samples pesados (los sonidos incluidos se
sintetizan), unas decenas de MB de RAM.

## Que hace
- **Tres cintas A / B / C** y un selector **A - B - C - ALL** (ALL las mezcla, A a la izquierda y C a la derecha).
- **Sonidos incluidos** (sintetizados, aproximaciones del caracter, NO grabaciones de cintas reales): Flute (Strawberry style), 3 Violins,
  8 Violins (Strings), Choir Aah, Choir Ooh, Cello. Clic en el nombre de una cinta para elegir.
- **Tu propio sonido**: arrastra un .wav / .mp3 sobre una ranura (o "Load your own sound"). MELLOLAB **detecta la nota** del audio
  (YIN) y cada tecla lo reproduce cambiando la velocidad de la cinta: si tu audio esta en LA, la tecla LA lo suena tal cual y
  las demas lo transportan. Si no hay tono claro (marca "?"), ajusta **ROOT** con - / +.
- **Controles del panel**: Attack / Decay / Sustain / Release, Flutter, Tape Saturation, Mechanics, Noise Floor, Vel > Volume,
  AT > Flutter (aftertouch), Volume, Tone, Pitch, interruptor Power (al apagar la cinta frena y el tono cae).
- Teclado de 36 teclas (F2 - E5) con el raton o MIDI (acepta aftertouch, rueda de modulacion -> flutter, pitch bend, pedal).
- **Tamano 50% - 400%**: clic en el logo MELLOLAB. Se guarda en `%APPDATA%\TapeLab\MELLOLAB\settings.xml`.

## ADVANCED
- **Tape length** (2-30 s; un Mellotron real dura ~8 s y la nota se corta) y **Loop tape end**.
- **Plugin rack**: detecta **xQUILER, FRAGATIME y XPACEVERB** (VST3) en tu PC, los carga dentro de MELLOLAB como inserts
  (xQUILER > FRAGATIME > XPACEVERB, despues del Mellotron), con EDIT (abre su ventana), BYPASS y UNLOAD. Su estado se guarda con tu proyecto.
  Si no estan instalados, el boton abre https://sites.google.com/view/tapelab-free-vst3-audio-plugin/full-page-embed
- RESCAN y ADD FOLDER... si tus VST3 estan en otra carpeta.

## Compilar con GitHub (sin Visual Studio)
1. Repositorio nuevo -> sube el CONTENIDO de esta carpeta (`CMakeLists.txt`, `Source`, `docs` y la carpeta oculta `.github`).
2. Pestana **Actions** -> "Build MELLOLAB" corre solo (10-15 min).
3. Descarga **MELLOLAB-VST3-Windows** (Artifacts), descomprime y copia `MELLOLAB.vst3` a `C:\Program Files\Common Files\VST3`.
4. FL Studio: Options > Manage plugins > Find installed plugins (aparece como generador).
