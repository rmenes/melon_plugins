// MelonVersion.h - the small version text under each plugin's title (e.g. "v1.1.0").
// The version itself is set once for the whole series: MELON_VERSION in cmake/MelonCommon.cmake.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace melon
{
inline juce::String versionText() { return juce::String ("v") + JucePlugin_VersionString; }

inline void setUpVersionLabel (juce::Label& label)
{
    label.setText (versionText(), juce::dontSendNotification);
    label.setFont (juce::FontOptions (11.0f));
    label.setColour (juce::Label::textColourId, juce::Colour (0xff7d8590));
    label.setJustificationType (juce::Justification::topLeft);
    label.setTooltip (juce::String (JucePlugin_Name) + " " + versionText());
}

// Title on top, version just under it, in the header's title column
inline void layoutTitle (juce::Rectangle<int> column, juce::Label& title, juce::Label& version)
{
    title.setBounds (column.removeFromTop (25));
    version.setBounds (column);
}
} // namespace melon
