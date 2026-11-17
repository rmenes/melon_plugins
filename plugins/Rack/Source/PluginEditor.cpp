#include "PluginEditor.h"

using namespace melon;

namespace
{
constexpr int kEarWidth = 112; // left "rack ear" with the unit's name and power switch

juce::String formatLatency (int samples, double rate)
{
    if (samples <= 0)
        return "Latency: 0 samples";
    return "Latency: " + juce::String (samples) + " samples (" + juce::String (1000.0 * samples / rate, 1) + " ms)";
}
} // namespace

//==============================================================================
FileSlot::FileSlot (const juce::String& captionText, const juce::String& wc, const juce::String& empty)
    : wildcard (wc), emptyText (empty)
{
    caption.setText (captionText, juce::dontSendNotification);
    caption.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    caption.setColour (juce::Label::textColourId, colours::dimText);
    addAndMakeVisible (caption);

    info.setFont (juce::FontOptions (12.0f));
    info.setColour (juce::Label::textColourId, colours::dimText);
    addAndMakeVisible (info);

    nameButton.setTooltip ("Choose a file");
    nameButton.onClick = [this] { choose(); };
    prev.setTooltip ("Previous file in the same folder");
    next.setTooltip ("Next file in the same folder");
    clear.setTooltip ("Unload");
    prev.onClick  = [this] { step (-1); };
    next.onClick  = [this] { step (+1); };
    clear.onClick = [this] { if (onClear) onClear(); };
    for (auto* b : { &nameButton, &prev, &next, &clear })
        addAndMakeVisible (b);
}

void FileSlot::setStatus (const RackProcessor::SlotStatus& st, const juce::String& extra)
{
    current = st.file;
    const bool has = st.file != juce::File();
    const auto name = has ? st.file.getFileNameWithoutExtension() : emptyText;
    if (nameButton.getButtonText() != name)
        nameButton.setButtonText (name);
    nameButton.setColour (juce::TextButton::textColourOffId, has ? colours::text : colours::dimText);

    const auto text = st.info + (extra.isNotEmpty() && ! st.error && ! st.loading ? "  -  " + extra : juce::String());
    if (info.getText() != text)
        info.setText (text, juce::dontSendNotification);
    info.setColour (juce::Label::textColourId, st.error ? colours::second : colours::dimText);

    prev.setEnabled (has);
    next.setEnabled (has);
    clear.setEnabled (has);
}

void FileSlot::paint (juce::Graphics& g)
{
    if (dragOver)
    {
        g.setColour (colours::accent.withAlpha (0.15f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
        g.setColour (colours::accent);
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 5.0f, 1.5f);
    }
}

void FileSlot::resized()
{
    auto r = getLocalBounds().reduced (4, 2);
    caption.setBounds (r.removeFromTop (15));
    auto row = r.removeFromTop (26);
    prev.setBounds (row.removeFromLeft (28));
    row.removeFromLeft (4);
    clear.setBounds (row.removeFromRight (28));
    row.removeFromRight (4);
    next.setBounds (row.removeFromRight (28));
    row.removeFromRight (4);
    nameButton.setBounds (row);
    info.setBounds (r.removeFromTop (18));
}

bool FileSlot::matches (const juce::File& f) const
{
    for (auto pattern : juce::StringArray::fromTokens (wildcard, ";", {}))
        if (f.getFileName().matchesWildcard (pattern.trim(), true))
            return true;
    return false;
}

bool FileSlot::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && matches (juce::File (files[0]));
}

void FileSlot::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    repaint();
    if (files.size() == 1 && onLoad)
        onLoad (juce::File (files[0]));
}

void FileSlot::choose()
{
    const auto folder = current.existsAsFile() ? current.getParentDirectory()
                                               : (startFolder ? startFolder() : juce::File());
    chooser = std::make_unique<juce::FileChooser> ("Load " + caption.getText().toLowerCase(), folder, wildcard);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<FileSlot> (this)] (const juce::FileChooser& fc) {
                              if (safe != nullptr && fc.getResult().existsAsFile() && safe->onLoad)
                                  safe->onLoad (fc.getResult());
                          });
}

void FileSlot::step (int delta)
{
    if (! current.existsAsFile() || ! onLoad)
        return;
    auto files = current.getParentDirectory().findChildFiles (juce::File::findFiles, false, wildcard);
    if (files.isEmpty())
        return;
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b) {
        return a.getFileName().compareNatural (b.getFileName()) < 0;
    });
    int idx = files.indexOf (current);
    idx     = idx < 0 ? 0 : (idx + delta + files.size()) % files.size();
    onLoad (files[idx]);
}

//==============================================================================
RackView::RackView (RackProcessor& p) : processor (p)
{
    units[0].title = "INPUT";
    units[1].title = "PEDAL";
    units[2].title = "EQ";
    units[3].title = "AMP";
    units[4].title = "CABINET";

    // input
    inputChannel  = &addCombo ("in_ch", "Which input channel feeds the rack (a guitar is usually on one side)");
    inputGain     = &addKnob ("in_gain", "Input", "Level into the rack");
    gateOn        = &addToggle ("gate_on", "Gate", "Noise gate on/off");
    gateThreshold = &addKnob ("gate_thr", "Threshold", "Noise gate threshold: signal below it is muted");

    // pedal and amp
    const char* ids[2][4] = { { "a_on", "a_in", "a_out", "a_norm" }, { "b_on", "b_in", "b_out", "b_norm" } };
    for (int s = 0; s < RackProcessor::kNumModels; ++s)
    {
        units[s == 0 ? 1 : 3].power = &addPower (ids[s][0], s == 0 ? "Pedal model on/off" : "Amp model on/off");
        modelIn[s]   = &addKnob (ids[s][1], "Input", "Level into the model (drives it harder or softer)");
        modelOut[s]  = &addKnob (ids[s][2], "Output", "Level out of the model");
        modelNorm[s] = &addToggle (ids[s][3], "Normalize", "Bring the model to a standard loudness (uses the model's loudness data)");

        auto& slot = modelSlot[s];
        slot.onLoad      = [this, s] (const juce::File& f) { processor.loadModel (s, f); };
        slot.onClear     = [this, s] { processor.clearModel (s); };
        slot.startFolder = [this] { return processor.lastModelFolder; };
        addAndMakeVisible (slot);
    }

    // eq
    units[2].power = &addPower ("eq_on", "EQ on/off");
    eqPosition     = &addCombo ("eq_pos", "Where the EQ sits in the chain");
    const char* eqIds[] = { "eq_1", "eq_2", "eq_3", "eq_4", "eq_5", "eq_6" };
    for (int b = 0; b < rack::Eq6::kBands; ++b)
    {
        const double f = rack::Eq6::kFreqs[(size_t) b];
        const auto name = f >= 1000.0 ? juce::String (f / 1000.0, 1).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + "k"
                                       : juce::String (juce::roundToInt (f));
        eqBand[b] = &addKnob (eqIds[b], name, "Boost or cut around " + name + "Hz");
    }

    // cabinet
    units[4].power = &addPower ("cab_on", "Cabinet (impulse responses) on/off");
    cabMode  = &addCombo ("cab_mode", "Stereo: IR 1 on the left, IR 2 on the right. Blend: both IRs mixed on both sides");
    cabBlend = &addKnob ("cab_blend", "Blend", "Blend mode: balance between IR 1 and IR 2");
    cabLevel = &addKnob ("cab_level", "Level", "Cabinet output level");
    for (int s = 0; s < RackProcessor::kNumIrs; ++s)
    {
        auto& slot = irSlot[s];
        slot.onLoad      = [this, s] (const juce::File& f) { processor.loadIr (s, f); };
        slot.onClear     = [this, s] { processor.clearIr (s); };
        slot.startFolder = [this] { return processor.lastIrFolder; };
        addAndMakeVisible (slot);
    }

    refresh();
}

RackKnob& RackView::addKnob (const juce::String& paramId, const juce::String& name, const juce::String& tip)
{
    auto& k = *knobs.emplace_back (std::make_unique<RackKnob>());
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, colours::grid);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, colours::background.withAlpha (0.6f));
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
    k.slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    k.slider.setTooltip (tip);
    addAndMakeVisible (k.slider);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setTooltip (tip);
    addAndMakeVisible (k.label);

    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramId, k.slider);
    if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (processor.apvts.getParameter (paramId)))
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    return k;
}

juce::ToggleButton& RackView::addToggle (const juce::String& paramId, const juce::String& text, const juce::String& tip)
{
    auto& t = *toggles.emplace_back (std::make_unique<juce::ToggleButton> (text));
    t.setTooltip (tip);
    addAndMakeVisible (t);
    buttonAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (processor.apvts, paramId, t));
    return t;
}

juce::ToggleButton& RackView::addPower (const juce::String& paramId, const juce::String& tip)
{
    return addToggle (paramId, "On", tip);
}

juce::ComboBox& RackView::addCombo (const juce::String& paramId, const juce::String& tip)
{
    auto* param = dynamic_cast<juce::AudioParameterChoice*> (processor.apvts.getParameter (paramId));
    jassert (param != nullptr);
    auto& box = *combos.emplace_back (std::make_unique<juce::ComboBox>());
    box.addItemList (param->choices, 1);
    box.setTooltip (tip);
    addAndMakeVisible (box);
    comboAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.apvts, paramId, box));
    return box;
}

void RackView::placeKnob (RackKnob& k, juce::Rectangle<int> r)
{
    k.label.setBounds (r.removeFromTop (18));
    k.slider.setBounds (r);
}

//==============================================================================
void RackView::refresh()
{
    for (int s = 0; s < RackProcessor::kNumModels; ++s)
        modelSlot[s].setStatus (processor.modelStatus (s), processor.isResampling (s) ? "resampled" : juce::String());
    for (int s = 0; s < RackProcessor::kNumIrs; ++s)
        irSlot[s].setStatus (processor.irStatus (s), {});

    const bool blend = cabMode->getSelectedItemIndex() == 1;
    cabBlend->slider.setEnabled (blend);
    gateThreshold->slider.setEnabled (gateOn->getToggleState());

    const bool lit     = gateOn->getToggleState() && processor.isGateOpen();
    const auto latency = formatLatency (processor.latencyForDisplay(), processor.hostSampleRate());
    if (lit != gateLit || latency != latencyText)
    {
        gateLit     = lit;
        latencyText = latency;
        repaint();
    }
}

void RackView::paint (juce::Graphics& g)
{
    for (auto& u : units)
    {
        const auto b = u.bounds.toFloat();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (b, 6.0f);

        // rack ear: darker strip, two screws, the unit name
        auto ear = b.withWidth ((float) kEarWidth);
        g.setColour (colours::background.withAlpha (0.45f));
        g.fillRoundedRectangle (ear, 6.0f);
        g.setColour (colours::grid);
        g.fillRect (ear.getRight() - 1.0f, b.getY() + 8.0f, 1.0f, b.getHeight() - 16.0f);

        for (float y : { b.getY() + 12.0f, b.getBottom() - 12.0f })
        {
            const auto screw = juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ b.getX() + 14.0f, y });
            g.setColour (colours::grid.brighter (0.2f));
            g.fillEllipse (screw);
            g.setColour (colours::background);
            g.drawLine (screw.getX() + 2.0f, screw.getCentreY(), screw.getRight() - 2.0f, screw.getCentreY(), 1.2f);
        }

        g.setColour (colours::accent);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (u.title, juce::Rectangle<float> (b.getX() + 28.0f, b.getY() + 6.0f, (float) kEarWidth - 32.0f, 22.0f).toNearestInt(),
                    juce::Justification::centredLeft);
    }

    // gate LED
    if (! gateLed.isEmpty())
    {
        const auto led = gateLed.toFloat();
        g.setColour (gateLit ? juce::Colour (0xff3fb950) : colours::grid);
        g.fillEllipse (led);
        if (gateLit)
        {
            g.setColour (juce::Colour (0xff3fb950).withAlpha (0.3f));
            g.fillEllipse (led.expanded (3.0f));
        }
    }

    // footer
    g.setFont (juce::FontOptions (12.0f));
    g.setColour (colours::dimText);
    g.drawText (latencyText, footer, juce::Justification::centredLeft);
    g.drawText ("After NeuralRack by brummer10  |  NeuralAudio engine by Mike Oliphant", footer, juce::Justification::centredRight);
}

void RackView::resized()
{
    auto area = getLocalBounds();
    footer    = area.removeFromBottom (18);
    area.removeFromBottom (6);

    // unit heights, scaled to the space available
    const float weights[5] = { 96.0f, 104.0f, 100.0f, 104.0f, 150.0f };
    const int   gap        = 6;
    const float scale      = (float) (area.getHeight() - 4 * gap) / (96.0f + 104.0f + 100.0f + 104.0f + 150.0f);
    for (int i = 0; i < 5; ++i)
    {
        auto& u  = units[i];
        u.bounds = i == 4 ? area : area.removeFromTop (juce::roundToInt (weights[i] * scale));
        area.removeFromTop (gap);
        u.content = u.bounds.withTrimmedLeft (kEarWidth).reduced (10, 6);
        if (u.power != nullptr)
            u.power->setBounds (u.bounds.getX() + 26, u.bounds.getY() + 32, kEarWidth - 34, 24);
    }

    const int knobW = 84;

    // --- input -----------------------------------------------------------------------
    {
        auto c   = units[0].content;
        auto col = c.removeFromLeft (190).withSizeKeepingCentre (190, 26);
        inputChannel->setBounds (col);
        c.removeFromLeft (16);
        placeKnob (*inputGain, c.removeFromLeft (knobW));
        c.removeFromLeft (30);
        auto gateCol = c.removeFromLeft (90);
        gateOn->setBounds (gateCol.withSizeKeepingCentre (90, 24).translated (0, -14));
        gateLed = juce::Rectangle<int> (12, 12).withCentre (gateCol.getCentre().translated (-30, 18));
        placeKnob (*gateThreshold, c.removeFromLeft (knobW));
    }

    // --- pedal and amp ------------------------------------------------------------------
    for (int s = 0; s < RackProcessor::kNumModels; ++s)
    {
        auto c     = units[s == 0 ? 1 : 3].content;
        auto right = c.removeFromRight (2 * knobW + 110);
        modelSlot[s].setBounds (c.withSizeKeepingCentre (c.getWidth(), juce::jmin (c.getHeight(), 66)));
        right.removeFromLeft (12);
        modelNorm[s]->setBounds (right.removeFromLeft (98).withSizeKeepingCentre (98, 24));
        placeKnob (*modelIn[s], right.removeFromLeft (knobW));
        placeKnob (*modelOut[s], right.removeFromLeft (knobW));
    }

    // --- eq ----------------------------------------------------------------------------------
    {
        auto c = units[2].content;
        eqPosition->setBounds (c.removeFromLeft (210).withSizeKeepingCentre (210, 26));
        c.removeFromLeft (10);
        const int w = juce::jmin (knobW, c.getWidth() / rack::Eq6::kBands);
        for (auto* k : eqBand)
            placeKnob (*k, c.removeFromLeft (w));
    }

    // --- cabinet ------------------------------------------------------------------------------
    {
        auto c     = units[4].content;
        auto right = c.removeFromRight (2 * knobW + 110);
        const int slotH = juce::jmin (c.getHeight() / 2, 66);
        auto slots = c.withSizeKeepingCentre (c.getWidth(), 2 * slotH);
        irSlot[0].setBounds (slots.removeFromTop (slotH));
        irSlot[1].setBounds (slots);

        right.removeFromLeft (12);
        auto modeCol = right.removeFromLeft (98);
        cabMode->setBounds (modeCol.withSizeKeepingCentre (98, 26));
        placeKnob (*cabBlend, right.removeFromLeft (knobW).withSizeKeepingCentre (knobW, juce::jmin (right.getHeight(), 100)));
        placeKnob (*cabLevel, right.removeFromLeft (knobW).withSizeKeepingCentre (knobW, juce::jmin (c.getHeight(), 100)));
    }
}

//==============================================================================
RackEditor::RackEditor (RackProcessor& p) : EditorBase (p, "Rack"), rackView (p)
{
    setDisplay (rackView);
    finishSetup (960, 720, 880, 660);
}
