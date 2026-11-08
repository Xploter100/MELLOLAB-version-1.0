#include "Settings.h"

juce::File Settings::file()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("TapeLab").getChildFile ("MELLOLAB").getChildFile ("settings.xml");
}

void Settings::load()
{
    const auto f = file();
    if (! f.existsAsFile()) return;
    if (auto xml = juce::XmlDocument::parse (f))
        if (xml->hasTagName ("MELLOLAB_SETTINGS"))
        {
            scale = juce::jlimit (0.5f, 4.f, (float) xml->getDoubleAttribute ("scale", 1.0));
            extraFolders.clear();
            for (auto* e : xml->getChildWithTagNameIterator ("Folder")) extraFolders.add (e->getAllSubText());
        }
}

void Settings::save() const
{
    juce::XmlElement xml ("MELLOLAB_SETTINGS");
    xml.setAttribute ("scale", (double) scale);
    for (auto& s : extraFolders) xml.createNewChildElement ("Folder")->addTextElement (s);
    const auto f = file();
    f.getParentDirectory().createDirectory();
    xml.writeTo (f);
}
