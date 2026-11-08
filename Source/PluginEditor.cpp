#include "PluginEditor.h"
#include "TapeSynth.h"

using namespace juce;
using namespace ml;

static const char* kTapelabUrl = "https://sites.google.com/view/tapelab-free-vst3-audio-plugin/full-page-embed";

//==============================================================================
// Look and feel
void MelloLAF::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float s, float e, Slider&)
{
    const auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (1.f);
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float ang = s + pos * (e - s);

    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillEllipse (c.x - r, c.y - r + 3.f, 2.f * r, 2.f * r);
    g.setGradientFill (ColourGradient (Colour (0xff3b3b3b), c.x, c.y - r, Colour (0xff0b0b0b), c.x, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, 2.f * r, 2.f * r);

    g.setColour (Colours::white.withAlpha (0.13f));                    // moleteado del borde
    for (int i = 0; i < 40; ++i)
    {
        const float a = (float) i * MathConstants<float>::twoPi / 40.f;
        g.drawLine (c.x + std::cos (a) * r * 0.9f, c.y + std::sin (a) * r * 0.9f, c.x + std::cos (a) * r, c.y + std::sin (a) * r, 1.f);
    }
    const float rc = r * 0.84f;
    g.setGradientFill (ColourGradient (Colour (0xfff7f7f4), c.x - rc * 0.5f, c.y - rc * 0.6f, Colour (0xff6a6a68), c.x + rc * 0.7f, c.y + rc * 0.8f, true));
    g.fillEllipse (c.x - rc, c.y - rc, 2.f * rc, 2.f * rc);
    g.setColour (Colours::black.withAlpha (0.16f));
    g.drawEllipse (c.x - rc * 0.7f, c.y - rc * 0.7f, rc * 1.4f, rc * 1.4f, 1.f);
    g.drawEllipse (c.x - rc * 0.4f, c.y - rc * 0.4f, rc * 0.8f, rc * 0.8f, 1.f);

    Path ptr;
    ptr.addRoundedRectangle (-rc * 0.11f, -rc * 0.98f, rc * 0.22f, rc * 1.0f, 1.5f);
    g.setColour (Colour (0xff111111));
    g.fillPath (ptr, AffineTransform::rotation (ang).translated (c.x, c.y));
}

void MelloLAF::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    g.setGradientFill (ColourGradient ((on ? mc::orangeDk : mc::dark).brighter (down ? 0.3f : (over ? 0.15f : 0.f)), r.getX(), r.getY(),
                                       (on ? mc::orangeDk : mc::dark2), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 5.f);
    g.setColour (mc::label.withAlpha (b.isEnabled() ? 0.55f : 0.2f));
    g.drawRoundedRectangle (r, 5.f, 1.f);
}

void MelloLAF::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    g.setColour (mc::cream.withAlpha (b.isEnabled() ? 1.f : 0.4f));
    g.setFont (Font (FontOptions (11.f)).boldened());
    g.drawText (b.getButtonText(), b.getLocalBounds(), Justification::centred);
}

void MelloLAF::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle, Slider&)
{
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle ((float) x, cy - 3.f, (float) w, 6.f, 3.f);
    g.setGradientFill (ColourGradient (mc::orange, (float) x, cy, mc::orangeDk, (float) x + (float) w, cy, false));
    g.fillRoundedRectangle ((float) x, cy - 3.f, pos - (float) x, 6.f, 3.f);
    g.setGradientFill (ColourGradient (Colour (0xfff4f4f1), pos - 6.f, cy - 8.f, Colour (0xff77776f), pos + 6.f, cy + 8.f, false));
    g.fillEllipse (pos - 8.f, cy - 8.f, 16.f, 16.f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.drawEllipse (pos - 8.f, cy - 8.f, 16.f, 16.f, 1.f);
}

//==============================================================================
// Panel ADVANCED
AdvancedPanel::AdvancedPanel (MelloAudioProcessor& p, Settings& s, std::function<void (int)> onEdit,
                              std::function<void (int)> beforeUnload, std::function<void()> onClose)
    : proc (p), settings (s), editCb (std::move (onEdit)), unloadCb (std::move (beforeUnload))
{
    closeBtn.setButtonText ("X");
    closeBtn.onClick = std::move (onClose);
    addAndMakeVisible (closeBtn);

    tapeSlider.setSliderStyle (Slider::LinearHorizontal);
    tapeSlider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (tapeSlider);
    tapeAtt = std::make_unique<SliderAtt> (proc.apvts, "tapelen", tapeSlider);

    loopBtn.setButtonText ("LOOP TAPE END");
    loopBtn.setClickingTogglesState (true);
    addAndMakeVisible (loopBtn);
    loopAtt = std::make_unique<ButtonAtt> (proc.apvts, "loop", loopBtn);

    for (int i = 0; i < 3; ++i)
    {
        const size_t k = (size_t) i;
        loadBtn[k].onClick = [this, i]
        {
            if (proc.rack.getStatus (i) == FxRack::Loaded) { unloadCb (i); proc.rack.unload (i); }
            else proc.rack.load (i);
        };
        editBtn[k].setButtonText ("EDIT");
        editBtn[k].onClick = [this, i] { editCb (i); };
        bypassBtn[k].setButtonText ("BYPASS");
        bypassBtn[k].setClickingTogglesState (true);
        bypAtt[k] = std::make_unique<ButtonAtt> (proc.apvts, "fxbyp" + String (i), bypassBtn[k]);
        getBtn[k].setButtonText ("NOT INSTALLED  -  GET IT FREE");
        getBtn[k].onClick = [] { URL (kTapelabUrl).launchInDefaultBrowser(); };
        for (auto* b : { &loadBtn[k], &editBtn[k], &bypassBtn[k], &getBtn[k] }) addAndMakeVisible (*b);
    }

    rescanBtn.setButtonText ("RESCAN");
    rescanBtn.onClick = [this] { proc.rack.rescan(); };
    folderBtn.setButtonText ("ADD FOLDER...");
    folderBtn.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Folder that contains your .vst3 plugins", File());
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories, [this] (const FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == File()) return;
            settings.extraFolders.addIfNotAlreadyThere (f.getFullPathName());
            settings.save();
            proc.rack.setExtraFolders (settings.extraFolders);
            proc.rack.rescan();
        });
    };
    siteBtn.setButtonText ("TAPELAB FREE VST3 PLUGINS  (website)");
    siteBtn.onClick = [] { URL (kTapelabUrl).launchInDefaultBrowser(); };
    for (auto* b : { &rescanBtn, &folderBtn, &siteBtn }) addAndMakeVisible (*b);

    startTimerHz (6);
    timerCallback();
}

void AdvancedPanel::resized()
{
    closeBtn.setBounds (640 - 24 - 30, 12, 30, 28);
    tapeSlider.setBounds (24, 104, 300, 24);
    loopBtn.setBounds (400, 100, 210, 30);
    for (int i = 0; i < 3; ++i)
    {
        const int y = 190 + i * 58;
        loadBtn[(size_t) i].setBounds (346, y + 10, 90, 30);
        editBtn[(size_t) i].setBounds (442, y + 10, 64, 30);
        bypassBtn[(size_t) i].setBounds (512, y + 10, 84, 30);
        getBtn[(size_t) i].setBounds (346, y + 10, 250, 30);
    }
    rescanBtn.setBounds (24, 376, 110, 28);
    folderBtn.setBounds (142, 376, 150, 28);
    siteBtn.setBounds (300, 376, 316, 28);
}

void AdvancedPanel::timerCallback()
{
    for (int i = 0; i < 3; ++i)
    {
        const size_t k = (size_t) i;
        const auto st = proc.rack.getStatus (i);
        const bool missing = (st == FxRack::NotFound);
        const bool loaded = (st == FxRack::Loaded);
        getBtn[k].setVisible (missing);
        loadBtn[k].setVisible (! missing);
        editBtn[k].setVisible (loaded);
        bypassBtn[k].setVisible (loaded);
        loadBtn[k].setEnabled (st != FxRack::Loading);
        loadBtn[k].setButtonText (loaded ? "UNLOAD" : (st == FxRack::Loading ? "LOADING..." : (st == FxRack::Failed ? "RETRY" : "LOAD")));
    }
    repaint();
}

void AdvancedPanel::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (0.f, 6.f), 16.f);
    g.setGradientFill (ColourGradient (Colour (0xff343436), 0.f, 0.f, Colour (0xff18181a), 0.f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 16.f);
    g.setColour (mc::label.withAlpha (0.5f));
    g.drawRoundedRectangle (r.reduced (1.f), 16.f, 1.5f);

    g.setColour (mc::cream);
    g.setFont (Font (FontOptions (22.f)).boldened());
    g.drawText ("ADVANCED", 24, 10, 300, 32, Justification::centredLeft);

    g.setColour (mc::orange);
    g.setFont (Font (FontOptions (11.f)).boldened());
    g.drawText ("TAPE", 24, 62, 200, 16, Justification::centredLeft);
    g.setColour (mc::label);
    g.setFont (11.f);
    g.drawText ("TAPE LENGTH   " + String (proc.apvts.getRawParameterValue ("tapelen")->load(), 1) + " s", 24, 82, 300, 16, Justification::centredLeft);
    g.setColour (mc::label.withAlpha (0.6f));
    g.drawText ("A real Mellotron tape lasts about 8 seconds, then the note stops. LOOP keeps it going.", 24, 132, 590, 14, Justification::centredLeft);

    g.setColour (mc::orange);
    g.setFont (Font (FontOptions (11.f)).boldened());
    g.drawText ("TAPELAB PLUGIN RACK   -   inserts after the Mellotron sound:  xQUILER > FRAGATIME > XPACEVERB", 24, 160, 600, 16, Justification::centredLeft);

    for (int i = 0; i < 3; ++i)
    {
        const float y = (float) (190 + i * 58);
        g.setColour (Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (16.f, y, 608.f, 52.f, 8.f);

        const auto st = proc.rack.getStatus (i);
        Colour dot = Colour (0xffc43a34);
        String text = "Not found on this PC  -  press the button to download it";
        if (st == FxRack::Found)   { dot = mc::orange; text = "Found: " + proc.rack.getPath (i).getFileName(); }
        if (st == FxRack::Loading) { dot = mc::orange; text = "Loading..."; }
        if (st == FxRack::Loaded)  { dot = Colour (0xff6bd16b); text = "Loaded  -  runs after the Mellotron sound"; }
        if (st == FxRack::Failed)  { dot = Colour (0xffc43a34); text = "Could not load: " + proc.rack.getMessage (i); }
        g.setColour (dot);
        g.fillEllipse (26.f, y + 20.f, 12.f, 12.f);
        g.setColour (mc::cream);
        g.setFont (Font (FontOptions (14.f)).boldened());
        g.drawText (FxRack::slotName (i), 48, (int) y + 6, 160, 18, Justification::centredLeft);
        g.setColour (mc::label.withAlpha (0.65f));
        g.setFont (10.f);
        g.drawText (String (" ") + FxRack::slotRole (i), 160, (int) y + 8, 120, 14, Justification::centredLeft);
        g.setColour (mc::label);
        g.setFont (11.f);
        g.drawFittedText (text, 48, (int) y + 26, 290, 22, Justification::centredLeft, 2);
    }

    g.setColour (mc::label.withAlpha (0.55f));
    g.setFont (10.f);
    const auto folders = proc.rack.getSearchedFolders();
    g.drawFittedText ((proc.rack.isScanning() ? String ("Scanning...  ") : String ("Searched:  ")) + (folders.isEmpty() ? String ("-") : folders.joinIntoString ("  |  ")),
                      24, 410, 592, 24, Justification::topLeft, 2);
}

//==============================================================================
// Contenido principal
namespace
{
const char* kIds[13]    { "attack", "decay", "sustain", "release", "flutter", "satur", "mech", "noise", "velvol", "atflutter", "volume", "tone", "pitch" };
const char* kLabels[13] { "ATTACK", "DECAY", "SUSTAIN", "RELEASE", "FLUTTER", "TAPE SATURATION", "MECHANICS", "NOISE FLOOR", "VEL > VOLUME", "AT > FLUTTER", "VOLUME", "TONE", "PITCH" };
const int kCx[13] { 139, 249, 133, 247, 405, 513, 620, 726, 832, 939, 76, 144, 213 };
const int kCy[13] { 185, 185, 269, 269, 125, 125, 125, 125, 125, 125, 513, 513, 513 };

void drawSpaced (Graphics& g, const String& t, float x, float y, float spacing, const Font& f)
{
    g.setFont (f);
    for (int i = 0; i < t.length(); ++i)
    {
        const auto s = t.substring (i, i + 1);
        g.drawText (s, (int) x, (int) y, 40, (int) f.getHeight() + 4, Justification::centredLeft, false);
        x += f.getStringWidthFloat (s) + spacing;
    }
}
}

MelloContent::MelloContent (MelloAudioProcessor& p, Settings& s, std::function<void (float)> sc)
    : proc (p), settings (s), onScale (std::move (sc))
{
    setLookAndFeel (&laf);
    proc.rack.setExtraFolders (settings.extraFolders);
    if (! settings.extraFolders.isEmpty()) proc.rack.rescan();

    for (size_t i = 0; i < 13; ++i)
    {
        auto& k = knobs[i];
        k.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        k.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
        addAndMakeVisible (k);
        atts[i] = std::make_unique<SliderAtt> (proc.apvts, kIds[i], k);
    }

    advBtn.setButtonText ("ADVANCED");
    advBtn.onClick = [this]
    {
        if (advanced != nullptr) { advanced.reset(); return; }
        Component::SafePointer<MelloContent> sp (this);
        advanced = std::make_unique<AdvancedPanel> (proc, settings,
            [this] (int slot) { openFxEditor (slot); },
            [this] (int slot) { fxWindows[(size_t) slot].reset(); },
            [sp] { MessageManager::callAsync ([sp] { if (sp != nullptr) sp->advanced.reset(); }); });
        addAndMakeVisible (*advanced);
        advanced->setBounds (230, 170, 640, 440);
        advanced->toFront (true);
    };
    addAndMakeVisible (advBtn);

    setSize (1100, 780);
    startTimerHz (30);
}

MelloContent::~MelloContent()
{
    stopTimer();
    for (auto& w : fxWindows) w.reset();
    advanced.reset();
    setLookAndFeel (nullptr);
}

void MelloContent::setParam (const String& id, float v)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
}

String MelloContent::knobText (int i) const
{
    const float v = getParam (kIds[i]);
    switch (i)
    {
        case 0: case 1: case 3: return v < 1.f ? String ((int) std::round (v * 1000.f)) + " ms" : String (v, 2) + " s";
        case 12:                return String (v >= 0.f ? "+" : "") + String ((int) std::round (v)) + " ct";
        default:                return String ((int) std::round (v)) + "%";
    }
}

void MelloContent::resized()
{
    titleRect = { 68, 90, 230, 48 };
    keysArea = { 351, 378, 628, 190 };
    powerRect = { 54, 426, 76, 52 };
    selRect = { 186, 420, 64, 64 };
    for (int i = 0; i < 13; ++i) knobs[(size_t) i].setBounds (kCx[i] - 28, kCy[i] - 28, 56, 56);
    advBtn.setBounds (872, 594, 150, 46);
    for (int i = 0; i < 3; ++i)
    {
        const int x = 60 + i * 268, y = 582;
        slotRects[(size_t) i] = { x, y, 252, 66 };
        nameRects[(size_t) i] = { x + 46, y + 8, 196, 26 };
        minusRects[(size_t) i] = { x + 150, y + 38, 24, 22 };
        plusRects[(size_t) i] = { x + 214, y + 38, 24, 22 };
    }

    keys.clear();                                           // 36 teclas: F2 .. E5
    int whites = 0;
    for (int n = kFirstKey; n < kFirstKey + kNumKeys; ++n) { const int m = n % 12; if (! (m == 1 || m == 3 || m == 6 || m == 8 || m == 10)) ++whites; }
    const float ww = (float) keysArea.getWidth() / (float) whites, x0 = (float) keysArea.getX(), y0 = (float) keysArea.getY();
    int wi = 0;
    std::vector<KeyInfo> blacks;
    for (int n = kFirstKey; n < kFirstKey + kNumKeys; ++n)
    {
        const int m = n % 12;
        if (! (m == 1 || m == 3 || m == 6 || m == 8 || m == 10)) { keys.push_back ({ n, { x0 + (float) wi * ww, y0, ww, (float) keysArea.getHeight() }, false }); ++wi; }
        else { const float bw = ww * 0.58f; blacks.push_back ({ n, { x0 + (float) wi * ww - bw * 0.5f, y0, bw, (float) keysArea.getHeight() * 0.62f }, true }); }
    }
    for (auto& b : blacks) keys.push_back (b);
    if (advanced != nullptr) advanced->setBounds (230, 170, 640, 440);
}

int MelloContent::keyAt (Point<float> p) const
{
    for (auto it = keys.rbegin(); it != keys.rend(); ++it) if (it->r.contains (p)) return it->note;
    return -1;
}

//==============================================================================
// Menus y archivos
void MelloContent::showSizeMenu()
{
    PopupMenu m, sizes;
    static const int pct[] { 50, 60, 70, 80, 90, 100, 110, 125, 150, 175, 200, 250, 300, 350, 400 };
    const int cur = (int) std::round (settings.scale * 100.f);
    for (int p : pct) sizes.addItem (1000 + p, String (p) + "%", true, p == cur);
    m.addSubMenu ("Window size", sizes);
    m.addItem (1, "Advanced...");
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (titleRect)), [this] (int r)
    {
        if (r >= 1050 && r <= 1400) { if (onScale) onScale ((float) (r - 1000) / 100.f); }
        else if (r == 1 && advanced == nullptr) advBtn.onClick();
    });
}

void MelloContent::showBankMenu (int slot)
{
    PopupMenu m;
    const bool user = proc.bankIsUser (slot);
    const String cur = proc.bankName (slot);
    for (int i = 0; i < kSynthCount; ++i) m.addItem (10 + i, synthName (i), true, ! user && cur == synthName (i));
    m.addSeparator();
    m.addItem (100, "Load your own sound (wav / mp3)...");
    m.showMenuAsync (PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (slotRects[(size_t) slot])), [this, slot] (int r)
    {
        if (r >= 10 && r < 10 + kSynthCount) proc.setBankSynth (slot, r - 10);
        else if (r == 100) chooseFile (slot);
    });
}

void MelloContent::chooseFile (int slot)
{
    chooser = std::make_unique<FileChooser> (String ("Pick a sound for tape ") + String ("ABC").substring (slot, slot + 1), File(), "*.wav;*.mp3;*.aif;*.aiff;*.flac;*.ogg");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this, slot] (const FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile()) proc.setBankFile (slot, f);
    });
}

void MelloContent::filesDropped (const StringArray& files, int x, int y)
{
    int slot = -1;
    for (int i = 0; i < 3; ++i) if (slotRects[(size_t) i].contains (x, y)) slot = i;
    if (slot < 0) { const int b = (int) std::round (getParam ("bank")); slot = b >= 3 ? 0 : b; }   // si no cae en una ranura: la cinta elegida
    for (auto& path : files)
    {
        const File f (path);
        if (f.existsAsFile()) { proc.setBankFile (slot, f); break; }
    }
}

void MelloContent::openFxEditor (int slot)
{
    if (fxWindows[(size_t) slot] != nullptr) { fxWindows[(size_t) slot]->toFront (true); return; }
    auto* inst = proc.rack.getInstance (slot);
    if (inst == nullptr) return;
    AudioProcessorEditor* ed = inst->hasEditor() ? inst->createEditorIfNeeded() : nullptr;
    if (ed == nullptr) ed = new GenericAudioProcessorEditor (*inst);
    Component::SafePointer<MelloContent> sp (this);
    fxWindows[(size_t) slot] = std::make_unique<FxWindow> (FxRack::slotName (slot), ed,
                                                           [sp, slot] { MessageManager::callAsync ([sp, slot] { if (sp != nullptr) sp->fxWindows[(size_t) slot].reset(); }); });
}

void MelloContent::closeFxEditor (int slot) { fxWindows[(size_t) slot].reset(); }

//==============================================================================
// Raton
void MelloContent::mouseMove (const MouseEvent& e)
{
    hoverKnob = -1;
    for (int i = 0; i < 13; ++i)
        if (e.position.getDistanceFrom (Point<float> ((float) kCx[i], (float) kCy[i])) <= 30.f) hoverKnob = i;
}

void MelloContent::mouseDown (const MouseEvent& e)
{
    const auto pt = e.getPosition();
    if (titleRect.contains (pt)) { showSizeMenu(); return; }
    if (powerRect.contains (pt)) { setParam ("power", getParam ("power") > 0.5f ? 0.f : 1.f); return; }
    if (selRect.contains (pt)) { setParam ("bank", (float) (((int) std::round (getParam ("bank")) + 1) % 4)); return; }

    for (int i = 0; i < 3; ++i)
    {
        const size_t k = (size_t) i;
        if (nameRects[k].contains (pt)) { showBankMenu (i); return; }
        if (proc.bankIsUser (i))
        {
            const String id = "root" + String (i);
            if (minusRects[k].contains (pt)) { setParam (id, jlimit (24.f, 96.f, getParam (id.toRawUTF8()) - 1.f)); return; }
            if (plusRects[k].contains (pt))  { setParam (id, jlimit (24.f, 96.f, getParam (id.toRawUTF8()) + 1.f)); return; }
        }
    }

    const int note = keyAt (e.position);
    if (note >= 0)
    {
        const float vel = jlimit (0.3f, 1.f, 0.3f + 0.7f * (e.position.y - (float) keysArea.getY()) / (float) keysArea.getHeight());
        proc.uiNote (note, vel, true);
        heldNote = note;
    }
}

void MelloContent::mouseDrag (const MouseEvent& e)
{
    if (heldNote < 0) return;
    const int note = keysArea.contains (e.getPosition()) ? keyAt (e.position) : -1;
    if (note == heldNote) return;
    proc.uiNote (heldNote, 0.f, false);
    heldNote = -1;
    if (note >= 0)
    {
        proc.uiNote (note, jlimit (0.3f, 1.f, 0.3f + 0.7f * (e.position.y - (float) keysArea.getY()) / (float) keysArea.getHeight()), true);
        heldNote = note;
    }
}

void MelloContent::mouseUp (const MouseEvent&)
{
    if (heldNote >= 0) { proc.uiNote (heldNote, 0.f, false); heldNote = -1; }
}

//==============================================================================
void MelloContent::timerCallback()
{
    timeSec += 1.f / 30.f;
    bool any = false;
    for (int k = 0; k < kNumKeys; ++k)
    {
        const float l = proc.engine.keyLevel[(size_t) k].load();
        keyLvl[(size_t) k] = l;
        tapeScroll[(size_t) k] = std::fmod (tapeScroll[(size_t) k] + l * 2.4f, 36.f);
        any = any || l > 0.02f;
    }
    const float motor = proc.engine.motorOut.load();
    reelAngle += motor * 0.03f + (any ? motor * 0.05f : 0.f);

    static String lastName[3];
    for (int i = 0; i < 3; ++i)
    {
        const auto nm = proc.bankName (i);
        if (nm != lastName[i])
        {
            if (lastName[i].isNotEmpty() && proc.bankIsUser (i))
            {
                const float c = proc.bankConfidence (i);
                message = "Tape " + String ("ABC").substring (i, i + 1) + ": \"" + nm + "\"  -  "
                          + (c >= 0.5f ? "detected note " + noteName ((int) std::round (getParam (("root" + String (i)).toRawUTF8()))) + " (" + String ((int) (c * 100.f)) + "% sure)"
                                       : String ("no clear pitch found, set the ROOT note by hand"));
                messageUntil = Time::getMillisecondCounter() + 7000;
            }
            lastName[i] = nm;
        }
    }
    if (messageUntil != 0 && Time::getMillisecondCounter() > messageUntil) { message = {}; messageUntil = 0; }
    repaint();
}

//==============================================================================
// Pintura: caja crema, paneles oscuros, cintas naranjas, teclado, placa metalica
void MelloContent::paintCase (Graphics& g)
{
    g.setColour (Colour (0xff121212));
    g.fillRoundedRectangle (150.f, 650.f, 52.f, 126.f, 6.f);
    g.fillRoundedRectangle (900.f, 650.f, 52.f, 126.f, 6.f);

    const Rectangle<float> body (20.f, 24.f, 1060.f, 636.f);
    g.setGradientFill (ColourGradient (mc::cream.brighter (0.08f), 0.f, 24.f, mc::creamDk, 0.f, 660.f, false));
    g.fillRoundedRectangle (body, 18.f);
    g.setColour (Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (body, 18.f, 2.f);

    for (int s = 0; s < 2; ++s)       // laterales altos
    {
        const Rectangle<float> w (s == 0 ? 20.f : 1030.f, 8.f, 50.f, 480.f);
        g.setGradientFill (ColourGradient (mc::cream.brighter (0.12f), w.getX(), 0.f, mc::creamDk, w.getRight(), 0.f, false));
        g.fillRoundedRectangle (w, 10.f);
        g.setColour (Colours::black.withAlpha (0.25f));
        g.drawRoundedRectangle (w, 10.f, 1.5f);
    }
    g.setColour (mc::creamDk.brighter (0.1f));
    g.fillRect (70.f, 58.f, 960.f, 24.f);                       // riel superior
    g.setColour (Colour (0xffb5a782));
    g.fillRect (70.f, 84.f, 960.f, 488.f);                      // hueco central
    g.setColour (Colours::black.withAlpha (0.22f));
    g.drawRect (70.f, 84.f, 960.f, 488.f, 1.5f);

    g.setColour (mc::cream);                                    // bloques laterales del teclado
    g.fillRoundedRectangle (266.f, 372.f, 84.f, 190.f, 6.f);
    g.fillRoundedRectangle (981.f, 372.f, 72.f, 190.f, 6.f);

    g.setGradientFill (ColourGradient (mc::cream.brighter (0.05f), 0.f, 572.f, mc::creamDk.darker (0.05f), 0.f, 660.f, false));
    g.fillRoundedRectangle (40.f, 572.f, 1020.f, 88.f, 8.f);    // panel frontal
    g.setColour (Colours::black.withAlpha (0.25f));
    g.drawRoundedRectangle (40.f, 572.f, 1020.f, 88.f, 8.f, 1.f);
}

void MelloContent::paintPanels (Graphics& g)
{
    for (const auto& r : { Rectangle<float> (55.f, 84.f, 268.f, 253.f), Rectangle<float> (330.f, 84.f, 693.f, 85.f) })
    {
        g.setGradientFill (ColourGradient (Colour (0xff3a3a3c), r.getX(), r.getY(), mc::dark2, r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 6.f);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 6.f, 1.5f);
        g.setColour (Colours::white.withAlpha (0.12f));
        for (int i = 0; i < 4; ++i) g.fillEllipse ((i & 1) ? r.getRight() - 12.f : r.getX() + 6.f, (i & 2) ? r.getBottom() - 12.f : r.getY() + 6.f, 6.f, 6.f);
    }

    g.setColour (mc::label);                                    // logo (clic: tamano de la ventana)
    drawSpaced (g, "MELLOLAB", 80.f, 92.f, 3.f, Font (FontOptions (30.f)).boldened());
    g.setColour (mc::label.withAlpha (0.55f));
    g.setFont (9.f);
    g.drawText ("TAPE REPLAY KEYBOARD   v1.0", 82, 124, 220, 12, Justification::centredLeft);
    Path chev;
    chev.addTriangle (282.f, 106.f, 294.f, 106.f, 288.f, 114.f);
    g.setColour (mc::orange);
    g.fillPath (chev);

    g.setColour (mc::label);
    g.setFont (Font (FontOptions (10.f)).boldened());
    g.drawText ("AMPLITUDE ENVELOPE", 60, 141, 258, 12, Justification::centred);

    for (int i = 0; i < 13; ++i)
    {
        g.setColour (i >= 10 ? mc::ink : mc::label);
        g.setFont (Font (FontOptions (9.5f)).boldened());
        g.drawText (kLabels[i], kCx[i] - 60, kCy[i] + (i >= 4 && i <= 9 ? 31 : 34), 120, 12, Justification::centred);
    }
}

void MelloContent::paintTapes (Graphics& g)
{
    const Rectangle<float> frame (330.f, 183.f, 649.f, 191.f);
    g.setGradientFill (ColourGradient (Colour (0xff2a2a2c), frame.getX(), frame.getY(), Colour (0xff111112), frame.getX(), frame.getBottom(), false));
    g.fillRect (frame);

    for (int k = 0; k < kNumKeys; ++k)
    {
        const float x = 343.f + (float) k * 17.4f, lvl = keyLvl[(size_t) k];
        const Rectangle<float> strip (x, 192.f, 14.f, 174.f);
        g.setGradientFill (ColourGradient (mc::orange.brighter (0.05f + lvl * 0.2f), x, 192.f, mc::orangeDk, x + 14.f, 366.f, false));
        g.fillRect (strip);
        g.setColour (Colours::black.withAlpha (0.35f));
        g.drawRect (strip, 0.8f);
        g.setColour (Colour (0xff3a2210));                       // perforaciones que avanzan cuando suena la nota
        for (int j = -1; j < 6; ++j)
        {
            const float y = 204.f + (float) j * 36.f + tapeScroll[(size_t) k];
            if (y > 196.f && y < 362.f) g.fillEllipse (x + 4.5f, y, 5.f, 5.f);
        }
    }
    for (float y : { 183.f, 366.f })                             // rieles metalicos
    {
        g.setGradientFill (ColourGradient (Colour (0xffdedede), 0.f, y, Colour (0xff6d6d6d), 0.f, y + 8.f, false));
        g.fillRect (330.f, y, 649.f, 8.f);
    }

    // carrete
    const Point<float> c (1012.f, 286.f);
    const float R = 54.f;
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillEllipse (c.x - R, c.y - R + 4.f, 2.f * R, 2.f * R);
    g.setGradientFill (ColourGradient (Colour (0xff3b3b3d), c.x - R, c.y - R, Colour (0xff0d0d0e), c.x + R, c.y + R, false));
    g.fillEllipse (c.x - R, c.y - R, 2.f * R, 2.f * R);
    g.setColour (mc::red);
    g.drawEllipse (c.x - R * 0.82f, c.y - R * 0.82f, R * 1.64f, R * 1.64f, 6.f);
    g.setColour (Colour (0xff9a9a98));
    for (int i = 0; i < 5; ++i)
    {
        const float a = reelAngle + (float) i * MathConstants<float>::twoPi / 5.f;
        g.drawLine (c.x + std::cos (a) * 12.f, c.y + std::sin (a) * 12.f, c.x + std::cos (a) * R * 0.7f, c.y + std::sin (a) * R * 0.7f, 5.f);
    }
    g.setGradientFill (ColourGradient (Colour (0xffeeeeea), c.x - 8.f, c.y - 8.f, Colour (0xff77776f), c.x + 10.f, c.y + 10.f, false));
    g.fillEllipse (c.x - 11.f, c.y - 11.f, 22.f, 22.f);
}

void MelloContent::paintKeys (Graphics& g)
{
    g.setColour (Colour (0xff1c1c1c));
    g.fillRect (keysArea.expanded (2, 0));
    for (const auto& k : keys)
    {
        const float lvl = keyLvl[(size_t) (k.note - kFirstKey)];
        const bool down = lvl > 0.02f;
        auto r = k.r.reduced (k.black ? 0.f : 0.8f, 0.f);
        if (down) r = r.withTrimmedTop (3.f);
        if (! k.black)
        {
            g.setGradientFill (ColourGradient (down ? Colour (0xffd8cfb8) : Colour (0xfffbf8ee), r.getX(), r.getY(), down ? Colour (0xffbfb59a) : Colour (0xffe8e0c8), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 2.f);
            g.setColour (Colours::black.withAlpha (0.35f));
            g.drawRoundedRectangle (r, 2.f, 0.8f);
        }
        else
        {
            g.setGradientFill (ColourGradient (down ? Colour (0xff4a4a4a) : Colour (0xff222222), r.getX(), r.getY(), Colour (0xff050505), r.getX(), r.getBottom(), false));
            g.fillRoundedRectangle (r, 2.f);
            g.setColour (Colours::white.withAlpha (0.18f));
            g.drawLine (r.getX() + 3.f, r.getY() + 3.f, r.getRight() - 3.f, r.getY() + 3.f, 1.f);
        }
    }
}

void MelloContent::paintPlate (Graphics& g)
{
    const Rectangle<float> p (37.f, 359.f, 227.f, 198.f);
    g.setGradientFill (ColourGradient (Colour (0xffe2e2de), p.getX(), p.getY(), Colour (0xff9d9d98), p.getRight(), p.getBottom(), false));
    g.fillRoundedRectangle (p, 5.f);
    g.setColour (Colours::black.withAlpha (0.4f));
    g.drawRoundedRectangle (p, 5.f, 1.2f);
    g.setColour (Colours::black.withAlpha (0.35f));
    for (int i = 0; i < 4; ++i) g.fillEllipse ((i & 1) ? p.getRight() - 12.f : p.getX() + 6.f, (i & 2) ? p.getBottom() - 12.f : p.getY() + 6.f, 6.f, 6.f);
    g.setColour (mc::ink.withAlpha (0.7f));
    drawSpaced (g, "TAPE ENGINE", 78.f, 372.f, 2.f, Font (FontOptions (13.f)).boldened());

    // interruptor de encendido + LED
    const bool on = getParam ("power") > 0.5f;
    const Point<float> led (80.f, 446.f);
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillEllipse (led.x - 12.f, led.y - 12.f, 24.f, 24.f);
    g.setGradientFill (ColourGradient (on ? Colour (0xffff7a6e) : Colour (0xff5a1612), led.x - 4.f, led.y - 4.f, on ? mc::red : Colour (0xff2b0a08), led.x + 8.f, led.y + 8.f, true));
    g.fillEllipse (led.x - 9.f, led.y - 9.f, 18.f, 18.f);
    if (on)
    {
        g.setGradientFill (ColourGradient (mc::red.withAlpha (0.35f), led.x, led.y, mc::red.withAlpha (0.f), led.x + 26.f, led.y, true));
        g.fillEllipse (led.x - 26.f, led.y - 26.f, 52.f, 52.f);
    }
    g.setColour (mc::ink);
    g.setFont (Font (FontOptions (9.f)).boldened());
    g.drawText ("OFF", 98, 434, 30, 12, Justification::centredLeft);
    g.drawText ("ON", 98, 456, 30, 12, Justification::centredLeft);

    // selector de cinta: ALL / A / B / C
    const Point<float> sc (218.f, 452.f);
    const int bank = (int) std::round (getParam ("bank"));
    static const float angle[4] { 2.2f, 1.4f, 0.65f, -1.57f };      // A, B, C, ALL
    static const char* names[4] { "A", "B", "C", "ALL" };
    g.setGradientFill (ColourGradient (Colour (0xff444444), sc.x, sc.y - 22.f, Colour (0xff0b0b0b), sc.x, sc.y + 22.f, false));
    g.fillEllipse (sc.x - 22.f, sc.y - 22.f, 44.f, 44.f);
    Path bar;
    bar.addRoundedRectangle (-6.f, -24.f, 12.f, 32.f, 3.f);
    g.setColour (Colour (0xff111111));
    g.fillPath (bar, AffineTransform::rotation (angle[bank]).translated (sc.x, sc.y));
    g.setColour (Colours::white.withAlpha (0.3f));
    g.drawLine (sc.x, sc.y, sc.x + std::sin (angle[bank]) * 20.f, sc.y - std::cos (angle[bank]) * 20.f, 1.5f);
    g.setFont (Font (FontOptions (9.5f)).boldened());
    for (int i = 0; i < 4; ++i)
    {
        g.setColour (i == bank ? mc::red : mc::ink);
        g.drawText (names[i], (int) (sc.x + std::sin (angle[i]) * 38.f) - 14, (int) (sc.y - std::cos (angle[i]) * 38.f) - 6, 28, 12, Justification::centred);
    }
}

void MelloContent::paintSlots (Graphics& g)
{
    const int bank = (int) std::round (getParam ("bank"));
    for (int i = 0; i < 3; ++i)
    {
        const auto r = slotRects[(size_t) i].toFloat();
        const bool sel = (bank == 3 || bank == i);
        g.setGradientFill (ColourGradient (Colour (0xff3a2f22), r.getX(), r.getY(), Colour (0xff1b150d), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 8.f);
        g.setColour (sel ? mc::orange : Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 8.f, sel ? 1.8f : 1.f);

        g.setColour (sel ? mc::orange : Colour (0xff6d6558));
        g.fillEllipse (r.getX() + 8.f, r.getY() + 10.f, 30.f, 30.f);
        g.setColour (mc::ink);
        g.setFont (Font (FontOptions (18.f)).boldened());
        g.drawText (String ("ABC").substring (i, i + 1), Rectangle<float> (r.getX() + 8.f, r.getY() + 10.f, 30.f, 30.f), Justification::centred);

        g.setColour (mc::cream);
        g.setFont (Font (FontOptions (13.f)).boldened());
        g.drawFittedText (proc.bankName (i).isEmpty() ? String ("loading...") : proc.bankName (i), nameRects[(size_t) i], Justification::centredLeft, 1);
        g.setColour (mc::label.withAlpha (0.55f));
        g.setFont (9.f);
        g.drawText (proc.bankIsUser (i) ? "YOUR SOUND  -  click to change" : "BUILT-IN TAPE  -  click to change", r.getX() + 46.f, r.getY() + 32.f, 200.f, 11.f, Justification::centredLeft);

        if (proc.bankIsUser (i))       // nota raiz: la nota en que esta grabado tu audio
        {
            const int root = (int) std::round (getParam (("root" + String (i)).toRawUTF8()));
            const bool unsure = proc.bankConfidence (i) < 0.5f;
            g.setColour (unsure ? mc::orange : mc::cream);
            g.setFont (Font (FontOptions (11.f)).boldened());
            g.drawText ("ROOT " + noteName (root) + (unsure ? " ?" : ""), (int) r.getX() + 46, (int) r.getY() + 42, 100, 14, Justification::centredLeft);
            for (int s = 0; s < 2; ++s)
            {
                const auto b = (s == 0 ? minusRects : plusRects)[(size_t) i].toFloat();
                g.setColour (Colours::black.withAlpha (0.5f));
                g.fillRoundedRectangle (b, 4.f);
                g.setColour (mc::cream);
                g.drawText (s == 0 ? "-" : "+", b, Justification::centred);
            }
        }
    }
}

void MelloContent::paint (Graphics& g)
{
    g.fillAll (Colour (0xff0e0e0f));
    paintCase (g);
    paintPanels (g);
    paintTapes (g);
    paintKeys (g);
    paintPlate (g);
    paintSlots (g);

    g.setColour (mc::ink.withAlpha (0.75f));
    g.setFont (Font (FontOptions (11.f)));
    g.drawText (message.isNotEmpty() ? message : String ("Drop a .wav / .mp3 on a tape slot (A, B, C) to make your own Mellotron - each key replays it at its own pitch"),
                80, 60, 940, 20, Justification::centred);

    if (hoverKnob >= 0)            // valor al pasar el raton por una perilla
    {
        const auto r = Rectangle<float> ((float) kCx[hoverKnob] - 34.f, (float) kCy[hoverKnob] - 50.f, 68.f, 18.f);
        g.setColour (Colours::black.withAlpha (0.85f));
        g.fillRoundedRectangle (r, 5.f);
        g.setColour (mc::orange);
        g.setFont (Font (FontOptions (11.f)).boldened());
        g.drawText (knobText (hoverKnob), r, Justification::centred);
    }
}

//==============================================================================
MelloAudioProcessorEditor::MelloAudioProcessorEditor (MelloAudioProcessor& p)
    : AudioProcessorEditor (&p), content (p, settings, [this] (float s) { applyScale (s); })
{
    addAndMakeVisible (content);
    content.setBounds (0, 0, kBaseW, kBaseH);
    applyScale (settings.scale);
}

void MelloAudioProcessorEditor::applyScale (float s)
{
    scale = jlimit (0.5f, 4.f, s);
    content.setTransform (AffineTransform::scale (scale));
    setSize (roundToInt ((float) kBaseW * scale), roundToInt ((float) kBaseH * scale));
    settings.scale = scale;
    settings.save();
}
