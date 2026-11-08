#include "FxRack.h"

namespace
{
void matchFile (const juce::File& f, juce::File* found)
{
    const auto n = f.getFileNameWithoutExtension().toLowerCase();
    for (int i = 0; i < FxRack::kSlots; ++i)
        if (found[i] == juce::File() && n.startsWith (juce::String (FxRack::slotName (i)).toLowerCase())) found[i] = f;
}

void scanDir (const juce::File& dir, juce::File* found)
{
    juce::Array<juce::File> items;
    dir.findChildFiles (items, juce::File::findFilesAndDirectories, false, "*.vst3");
    for (auto& f : items) matchFile (f, found);

    juce::Array<juce::File> subs;                 // carpetas de fabricante (p. ej. VST3\TapeLab\)
    dir.findChildFiles (subs, juce::File::findDirectories, false, "*");
    for (auto& s : subs)
    {
        if (s.hasFileExtension ("vst3")) continue;
        juce::Array<juce::File> in;
        s.findChildFiles (in, juce::File::findFilesAndDirectories, false, "*.vst3");
        for (auto& f : in) matchFile (f, found);
    }
}
}

FxRack::FxRack()
{
    alive = std::make_shared<std::atomic<bool>> (true);
    fm.addDefaultFormats();
    for (auto& s : st) s.store (NotFound);
    midiScratch.ensureSize (1024);
}

FxRack::~FxRack()
{
    alive->store (false);
    pool.removeAllJobs (true, 8000);
    for (auto& f : fx)
    {
        std::unique_ptr<juce::AudioPluginInstance> old;
        { const juce::SpinLock::ScopedLockType sl (lock); old = std::move (f); }
        old.reset();
    }
}

void FxRack::prepare (double sampleRate, int blockSize)
{
    sr = sampleRate; block = blockSize;
    for (auto& f : fx)
        if (f != nullptr) { f->setPlayConfigDetails (2, 2, sr, block); f->prepareToPlay (sr, block); }
}

void FxRack::setExtraFolders (const juce::StringArray& f)
{
    const juce::ScopedLock sl (info);
    extraFolders = f;
}

juce::File FxRack::getPath (int i) const { const juce::ScopedLock sl (info); return paths[i]; }
juce::String FxRack::getMessage (int i) const { const juce::ScopedLock sl (info); return msgs[i]; }
juce::StringArray FxRack::getSearchedFolders() const { const juce::ScopedLock sl (info); return searched; }

void FxRack::rescan()
{
    if (scanning.exchange (true)) return;
    auto flag = alive;
    pool.addJob ([this, flag]
    {
        juce::File found[kSlots];
        juce::FileSearchPath sp;
        { juce::VST3PluginFormat f; sp = f.getDefaultLocationsToSearch(); }
        juce::StringArray extra;
        { const juce::ScopedLock sl (info); extra = extraFolders; }
        for (auto& e : extra) sp.add (juce::File (e));

        juce::StringArray list;
        for (int d = 0; d < sp.getNumPaths(); ++d)
        {
            const juce::File dir = sp[d];
            if (! dir.isDirectory()) continue;
            list.add (dir.getFullPathName());
            scanDir (dir, found);
        }
        {
            const juce::ScopedLock sl (info);
            searched = list;
            for (int i = 0; i < kSlots; ++i) paths[i] = found[i];
        }
        for (int i = 0; i < kSlots; ++i)
        {
            const int cur = st[(size_t) i].load();
            if (cur == Loaded || cur == Loading) continue;
            st[(size_t) i].store (found[i] != juce::File() ? Found : NotFound);
        }
        scanning = false;
        juce::MessageManager::callAsync ([this, flag]
        {
            if (! flag->load()) return;
            for (int i = 0; i < kSlots; ++i) if (wantLoad[i] && getStatus (i) == Found) load (i);   // proyectos guardados con el plugin cargado
        });
    });
}

void FxRack::fail (int i, const juce::String& why)
{
    { const juce::ScopedLock sl (info); msgs[i] = why; }
    st[(size_t) i].store (Failed);
}

void FxRack::load (int i)
{
    const int cur = st[(size_t) i].load();
    if (cur == Loading || cur == Loaded) return;
    const juce::File p = getPath (i);
    if (p == juce::File()) return;

    wantLoad[i] = false;
    st[(size_t) i].store (Loading);
    auto flag = alive;
    pool.addJob ([this, i, p, flag]
    {
        juce::OwnedArray<juce::PluginDescription> descs;
        { juce::VST3PluginFormat f; f.findAllTypesForFile (descs, p.getFullPathName()); }
        if (descs.isEmpty()) { if (flag->load()) fail (i, "Could not read the plugin"); return; }
        const juce::PluginDescription d = *descs[0];

        juce::MessageManager::callAsync ([this, i, d, flag]
        {
            if (! flag->load()) return;
            fm.createPluginInstanceAsync (d, sr, block, [this, i, flag] (std::unique_ptr<juce::AudioPluginInstance> inst, const juce::String& err)
            {
                if (! flag->load()) return;
                if (inst == nullptr) { fail (i, err.isNotEmpty() ? err : juce::String ("Could not create the plugin")); return; }
                finishLoad (i, std::move (inst));
            });
        });
    });
}

void FxRack::finishLoad (int i, std::unique_ptr<juce::AudioPluginInstance> inst)
{
    juce::AudioProcessor::BusesLayout lay;
    lay.inputBuses.add (juce::AudioChannelSet::stereo());
    lay.outputBuses.add (juce::AudioChannelSet::stereo());
    if (inst->checkBusesLayoutSupported (lay)) inst->setBusesLayout (lay);
    inst->setPlayConfigDetails (2, 2, sr, block);
    inst->prepareToPlay (sr, block);

    if (pendingState[i].getSize() > 0)
    {
        inst->setStateInformation (pendingState[i].getData(), (int) pendingState[i].getSize());
        pendingState[i].setSize (0);
    }
    std::unique_ptr<juce::AudioPluginInstance> old;
    { const juce::SpinLock::ScopedLockType sl (lock); old = std::move (fx[(size_t) i]); fx[(size_t) i] = std::move (inst); }
    old.reset();
    { const juce::ScopedLock sl (info); msgs[i] = {}; }
    st[(size_t) i].store (Loaded);
}

void FxRack::unload (int i)
{
    std::unique_ptr<juce::AudioPluginInstance> old;
    { const juce::SpinLock::ScopedLockType sl (lock); old = std::move (fx[(size_t) i]); }
    if (old != nullptr) old->releaseResources();
    old.reset();
    wantLoad[i] = false;
    st[(size_t) i].store (getPath (i) != juce::File() ? Found : NotFound);
}

void FxRack::process (juce::AudioBuffer<float>& buffer, const bool* bypass)
{
    const juce::SpinLock::ScopedTryLockType tl (lock);       // si se esta cargando / descargando un plugin, ese bloque pasa sin efectos
    if (! tl.isLocked()) return;
    for (int i = 0; i < kSlots; ++i)
        if (fx[(size_t) i] != nullptr && ! bypass[i])
        {
            midiScratch.clear();
            fx[(size_t) i]->processBlock (buffer, midiScratch);
        }
}

void FxRack::saveStates (juce::ValueTree& t)
{
    for (int i = 0; i < kSlots; ++i)
    {
        const juce::String k = juce::String (i);
        if (fx[(size_t) i] != nullptr)
        {
            juce::MemoryBlock mb;
            fx[(size_t) i]->getStateInformation (mb);
            t.setProperty ("fxload" + k, 1, nullptr);
            t.setProperty ("fxstate" + k, mb.toBase64Encoding(), nullptr);
        }
        else if (! wantLoad[i]) t.setProperty ("fxload" + k, 0, nullptr);
    }
}

void FxRack::restoreStates (const juce::ValueTree& t)
{
    for (int i = 0; i < kSlots; ++i)
    {
        const juce::String k = juce::String (i);
        wantLoad[i] = (int) t.getProperty ("fxload" + k, 0) != 0;
        if (wantLoad[i]) pendingState[i].fromBase64Encoding (t.getProperty ("fxstate" + k).toString());
        if (wantLoad[i] && getStatus (i) == Found) load (i);
    }
}
