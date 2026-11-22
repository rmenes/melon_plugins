#include "PluginEditor.h"

NamEditor::NamEditor (NamProcessor& p) : EditorBase (p, "NAM")
{
    titleWidth = 110;

    loader.onLoad      = [this] (const juce::File& f) { processor.loadModel (f); };
    loader.onClear     = [this] { processor.clearModel(); };
    loader.startFolder = [this] { return processor.lastModelFolder; };
    setDisplay (loader);

    gateOn = &addToggle ("gate_on", "Noise Gate", 120, "Noise gate on/off (mutes the hiss and hum between notes)");
    eqOn   = &addToggle ("eq_on", "EQ", 70, "Bass / Middle / Treble on/off");
    addToggle ("normalize", "Normalize", 120,
               "Bring every model to the same loudness, using the loudness data stored in the .nam file");
    addChoiceBox ("size", 90, false,
                  "Model size, for NAM A2 (slimmable) models: Light runs the small network stored in the same file "
                  "and needs much less CPU. Other models ignore it");

    knobsPerRow = 5;
    addKnob ("input", "Input", "Level into the model: turn it up to drive the amp harder, down for a cleaner sound");
    gateKnob = &addKnob ("gate_thr", "Gate", "Noise gate threshold: signal below it is muted. Raise it until the noise disappears");
    toneKnobs[0] = &addKnob ("bass", "Bass", "Low shelf at 150 Hz (5 = flat)");
    toneKnobs[1] = &addKnob ("middle", "Middle", "Mid band at 425 Hz (5 = flat)");
    toneKnobs[2] = &addKnob ("treble", "Treble", "High shelf at 1.8 kHz (5 = flat)");

    finishSetup (720, 400, 640, 380);
}

void NamEditor::timerCallback()
{
    loader.setStatus (processor.modelStatus());

    gateKnob->slider.setEnabled (gateOn->getToggleState());
    for (auto* k : toneKnobs)
        k->slider.setEnabled (eqOn->getToggleState());
}
