#include "PluginEditor.h"

CabEditor::CabEditor (CabProcessor& p) : EditorBase (p, "Cab")
{
    titleWidth = 110;

    loader.onLoad      = [this] (const juce::File& f) { processor.loadIr (f); };
    loader.onClear     = [this] { processor.clearIr(); };
    loader.startFolder = [this] { return processor.lastIrFolder; };
    setDisplay (loader);

    knobsPerRow = 3;
    addKnob ("lowcut", "Low Cut", "Removes the low end below this frequency (20 Hz = off). Tightens boomy cabinets");
    addKnob ("highcut", "High Cut", "Removes the highs above this frequency (20 kHz = off). Tames fizz and harshness");
    addKnob ("mix", "Mix", "Balance between the dry signal and the cabinet (100 % = cabinet only)");

    finishSetup (600, 360, 520, 340);
}
