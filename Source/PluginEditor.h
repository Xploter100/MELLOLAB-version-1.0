#pragma once
#include "PluginProcessor.h"
#include "Settings.h"

namespace mc
{
inline const juce::Colour cream { 0xffe9dfc9 }, creamDk { 0xffc9bb98 }, dark { 0xff2b2b2d }, dark2 { 0xff151516 }, metal { 0xffd9d9d5 },
                          orange { 0xffd98a2e }, orangeDk { 0xffa5561a }, red { 0xffe0241f }, label { 0xffd2d2ce }, ink { 0xff2b2418 };
}

// Perillas metalicas con aguja negra, botones planos de "panel de instrumento"
struct MelloLAF : public juce::LookAndFeel_V4
{
    MelloLAF()
    {
        setColour (juce::PopupMenu::backgroundColourId, mc::dark);
        setColour (juce::PopupMenu::textColourId, mc::cream);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, mc::orangeDk);
        setColour (juce::PopupMenu::highlightedTextColourId, mc::cream);
    }
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float startAngle, float endAngle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
};

// Ventana flotante para el editor de un plugin alojado (xQUILER / FRAGATIME / XPACEVERB)
class FxWindow : public juce::DocumentWindow
{
public:
    FxWindow (const juce::String& name, juce::AudioProcessorEditor* editor, std::function<void()> onClose)
        : juce::DocumentWindow (name, mc::dark, juce::DocumentWindow::closeButton), closeCb (std::move (onClose))
    {
        setUsingNativeTitleBar (true);
        setContentOwned (editor, true);
        setResizable (false, false);
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }
    void closeButtonPressed() override { if (closeCb) closeCb(); }
private:
    std::function<void()> closeCb;
};

// Panel "ADVANCED": longitud / bucle de la cinta + rack de plugins TapeLab
class AdvancedPanel : public juce::Component, private juce::Timer
{
public:
    AdvancedPanel (MelloAudioProcessor& p, Settings& s,
                   std::function<void (int)> onEdit, std::function<void (int)> beforeUnload, std::function<void()> onClose);
    ~AdvancedPanel() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    void timerCallback() override;

    MelloAudioProcessor& proc;
    Settings& settings;
    std::function<void (int)> editCb, unloadCb;
    juce::Slider tapeSlider;
    juce::TextButton loopBtn, rescanBtn, folderBtn, closeBtn, siteBtn;
    std::array<juce::TextButton, 3> loadBtn, editBtn, bypassBtn, getBtn;
    std::unique_ptr<SliderAtt> tapeAtt;
    std::unique_ptr<ButtonAtt> loopAtt;
    std::array<std::unique_ptr<ButtonAtt>, 3> bypAtt;
    std::unique_ptr<juce::FileChooser> chooser;
};

// Contenido (tamano base 1100x780). El editor lo escala entre 50% y 400% (clic en el logo MELLOLAB).
class MelloContent : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    MelloContent (MelloAudioProcessor&, Settings&, std::function<void (float)> onScaleChange);
    ~MelloContent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    struct KeyInfo { int note; juce::Rectangle<float> r; bool black; };

    void timerCallback() override;
    void setParam (const juce::String& id, float v);
    float getParam (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    int  keyAt (juce::Point<float> p) const;
    void showSizeMenu();
    void showBankMenu (int slot);
    void chooseFile (int slot);
    void openFxEditor (int slot);
    void closeFxEditor (int slot);
    juce::String knobText (int idx) const;

    void paintCase (juce::Graphics&);
    void paintPanels (juce::Graphics&);
    void paintTapes (juce::Graphics&);
    void paintKeys (juce::Graphics&);
    void paintPlate (juce::Graphics&);
    void paintSlots (juce::Graphics&);

    MelloAudioProcessor& proc;
    Settings& settings;
    std::function<void (float)> onScale;
    MelloLAF laf;

    std::array<juce::Slider, 13> knobs;
    std::array<std::unique_ptr<SliderAtt>, 13> atts;
    juce::TextButton advBtn;
    std::unique_ptr<AdvancedPanel> advanced;
    std::array<std::unique_ptr<FxWindow>, 3> fxWindows;
    std::unique_ptr<juce::FileChooser> chooser;

    std::vector<KeyInfo> keys;
    juce::Rectangle<int> titleRect, keysArea, powerRect, selRect;
    std::array<juce::Rectangle<int>, 3> slotRects, nameRects, minusRects, plusRects;
    int heldNote = -1, hoverKnob = -1;
    float reelAngle = 0.f, timeSec = 0.f;
    std::array<float, ml::kNumKeys> keyLvl {}, tapeScroll {};
    juce::String message;
    juce::uint32 messageUntil = 0;
};

// Ventana del plugin: contiene el contenido y lo escala; el tamano elegido queda guardado
class MelloAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MelloAudioProcessorEditor (MelloAudioProcessor&);
    void resized() override {}

private:
    static constexpr int kBaseW = 1100, kBaseH = 780;
    void applyScale (float s);

    Settings settings;
    MelloContent content;
    float scale = 1.f;
};
