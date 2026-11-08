# MELLOLAB v1.0 - Arquitectura

```
Source/
  Common.h        Zone (trozo de cinta grabado a una nota) y Tape (1..3 zonas)
  TapeBank.*      3 cintas seguras para el hilo de audio (punteros atomicos + basura diferida) · deteccion de tono YIN · carga de .wav/.mp3
  TapeSynth.*     sonidos incluidos: flauta (aditiva + soplido), cuerdas (sierras polyBLEP desafinadas + cuerpo), coros (fuente glotal + formantes)
  MelloEngine.*   32 voces, ADSR, flutter/wow/deriva, motor (power), saturacion, mecanica, ruido, tono; render por bloques de 256
  FxRack.*        busqueda de .vst3, carga/descarga asincrona, procesado en serie (try-lock en el hilo de audio), estado guardado
  Settings.*      tamano de ventana y carpetas extra (global, XML en %APPDATA%)
  PluginProcessor.* parametros (APVTS), MIDI, bancos A/B/C, orquestacion
  PluginEditor.*  caja Mellotron dibujada en vectores, perillas metalicas, cintas animadas, panel ADVANCED, ventana escalable
```
## Motor de cinta
- Velocidad de reproduccion = (fs_cinta / fs) · 2^((nota - raiz)/12) · pitch · (1 + flutter) · motor. Los sonidos incluidos traen 3 zonas
  (DO3, DO4, DO5) y se usa la mas cercana (max. +-6 semitonos de cambio de velocidad). Un sonido del usuario es 1 zona: tono (YIN) -> raiz.
- Flutter = 0.6 Hz (wow) + 6.5 Hz (flutter) + deriva aleatoria; el aftertouch y la rueda de modulacion lo aumentan.
- Mechanics: caida de velocidad al arrancar la cinta (spin-up) + golpe grave del mecanismo + zumbido. Power OFF: el motor frena (tono baja y se apaga).
- La cinta termina a los `Tape length` s (o al final del audio): fundido de 20 ms. Con LOOP se repite con cruce suave.
## Tiempo real
Voces, eventos y buffers se crean antes; el hilo de audio solo lee punteros atomicos y usa `try-lock` con el rack. La carga de audio, la sintesis
de sonidos y la busqueda de plugins corren en hilos aparte. Memoria: cada sonido incluido ~4 MB (3 zonas mono de 10 s); sonidos propios hasta 30 s.
## Rack de plugins
`AudioPluginFormatManager` + VST3 (JUCE_PLUGINHOST_VST3=1). Se buscan xQUILER / FRAGATIME / XPACEVERB en las carpetas VST3 por defecto
(y las que agregues). Al cargar: buses estereo, prepareToPlay, estado restaurado. Si falla o no existe, el panel lo indica y ofrece el enlace de descarga.
