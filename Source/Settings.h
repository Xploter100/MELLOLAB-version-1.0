#pragma once
#include <JuceHeader.h>

// Ajustes globales de MELLOLAB (se comparten entre instancias): %APPDATA%\TapeLab\MELLOLAB\settings.xml
struct Settings
{
    float scale = 1.f;                 // tamano de la ventana 0.5 .. 4.0 (50% .. 400%)
    juce::StringArray extraFolders;    // carpetas VST3 extra donde buscar xQUILER / FRAGATIME / XPACEVERB

    Settings() { load(); }
    static juce::File file();
    void load();
    void save() const;
};
