#pragma once
#include "PluginProcessor.h"

namespace ds::ui
{
struct Fonts
{
    static Fonts& get();
    juce::Font label (float h) const;   // Michroma: caratteri larghi e squadrati come la serigrafia
    juce::Font logo (float h) const;    // Ultra: slab pesante come le lettere della targa CRUMAR
    juce::Typeface::Ptr wide, slab;
private:
    Fonts();
};

class LearnHost
{
public:
    virtual ~LearnHost() = default;
    virtual void showControlMenu (juce::Component& c, const juce::String& paramId) = 0;
    virtual bool isLearning (const juce::String& paramId) const = 0;
    // fumetto con il valore del comando (passaggio del mouse, trascinamento, rotella)
    virtual void showValue (juce::Component& c, const juce::String& paramId, bool show) = 0;
};

// Regolazione comune di manopole e cursori: trascinamento (fine con ⇧ o ⌘), rotella, doppio clic
struct Drag
{
    static bool fine (const juce::ModifierKeys& m) { return m.isShiftDown() || m.isCommandDown(); }
};

//==============================================================================
// Manopola (scala stampata sul pannello) o selettore a puntatore
class Knob : public juce::Slider
{
public:
    Knob (LearnHost& h, juce::String id, bool selector, int numPositions = 0);
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent&) override { host.showValue (*this, paramId, true); }
    void mouseExit (const juce::MouseEvent&) override { if (! dragging) host.showValue (*this, paramId, false); }
    void paint (juce::Graphics&) override;

    LearnHost& host;
    juce::String paramId;
    bool isSelector;
    int positions;
    float startDeg = -135.0f, endDeg = 135.0f;
    float radius = 20.0f;
    double defaultValue = 0.0;
    bool showTicks = false;          // manopola senza scala serigrafata (EXT)
private:
    bool dragged = false, dragging = false;
    juce::Point<float> last;
};

//==============================================================================
// Deviatore a levetta (orizzontale o verticale)
class Toggle : public juce::Button
{
public:
    Toggle (LearnHost& h, juce::String id, bool horizontal, bool onIsFirst);
    void mouseDown (const juce::MouseEvent& e) override;
    void paintButton (juce::Graphics&, bool over, bool down) override;
    LearnHost& host;
    juce::String paramId;
    bool horiz, onFirst;   // onFirst: posizione "IN" a sinistra / in alto
};

//==============================================================================
// Cursore a slitta (LFO mixer orizzontali, ADSR e glide verticali)
class Fader : public juce::Slider
{
public:
    Fader (LearnHost& h, juce::String id, bool horizontal);
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent&) override { host.showValue (*this, paramId, true); }
    void mouseExit (const juce::MouseEvent&) override { if (! dragging) host.showValue (*this, paramId, false); }
    void paint (juce::Graphics&) override;
    LearnHost& host;
    juce::String paramId;
    bool horiz;
    double defaultValue = 0.0;
private:
    bool dragging = false;
    juce::Point<float> last;
};

//==============================================================================
// Fumetto con nome e valore del comando
class ValueBubble : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    juce::String title, value;
};

//==============================================================================
// Barra dei preset: ◀ nome ▶, menu, A/B, salva
class PresetBar : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PresetBar (DsProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void refresh();
private:
    void showMenu();
    void save();
    juce::Rectangle<float> prevR() const, nextR() const, nameR() const, abR() const, saveR() const;
    DsProcessor& proc;
    juce::String shownName;
    bool shownMod = false, shownB = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

//==============================================================================
// Leva PITCH BENDING: torna al centro con una molla; segue anche la rotella MIDI
class BendLever : public juce::Component, public juce::SettableTooltipClient
{
public:
    BendLever (DsProcessor& p, LearnHost& h);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void refresh();
    float shown = 0.0f;
private:
    void setFromX (float x);
    DsProcessor& proc;
    LearnHost& host;
    juce::RangedAudioParameter* param;
};

//==============================================================================
class Keys : public juce::MidiKeyboardComponent
{
public:
    explicit Keys (juce::MidiKeyboardState& s);
    void drawWhiteNote (int, juce::Graphics&, juce::Rectangle<float>, bool, bool, juce::Colour, juce::Colour) override;
    void drawBlackNote (int, juce::Graphics&, juce::Rectangle<float>, bool, bool, juce::Colour) override;
};

//==============================================================================
// Il pannello è disegnato in unità fisse (1880 x 980) e scalato con una trasformazione.
class Panel : public juce::Component, public LearnHost, private juce::Timer
{
public:
    static constexpr int W = 1880, H = 980;
    explicit Panel (DsProcessor&);
    ~Panel() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    void showControlMenu (juce::Component&, const juce::String& paramId) override;
    bool isLearning (const juce::String& paramId) const override;
    void showValue (juce::Component& c, const juce::String& paramId, bool show) override;

    enum class Scale { Ten, Bipolar5, Bipolar4, Db15, None };
    struct KnobSpec { const char* id; float x, y; Scale scale; const char* label; const char* tip; };
    // icone: 0 testo; 1 forme DCO; 2 forme LFO1; 3 forme LFO2; 4 sorgenti mixer
    struct SelSpec  { const char* id; float x, y; float a0, a1; std::vector<juce::String> labels; int icons; float labelR; const char* tip; };
    struct TogSpec  { const char* id; float x, y; bool horiz, onFirst; const char* tip; };
    struct FadSpec  { const char* id; float x, y, len; bool horiz; const char* tip; };

private:
    void timerCallback() override;
    void addKnob (const KnobSpec&);
    void addSelector (const SelSpec&);
    void addToggle (const TogSpec&);
    void addFader (const FadSpec&);
    void showSettingsMenu();
    void repaintControls();

    DsProcessor& proc;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<Toggle> toggles;
    juce::OwnedArray<Fader> faders;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAtt;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAtt;
    std::vector<KnobSpec> knobSpecs;
    std::vector<SelSpec> selSpecs;
    std::vector<TogSpec> togSpecs;
    std::vector<FadSpec> fadSpecs;
    Keys keys;
    BendLever lever;
    PresetBar presetBar;
    ValueBubble bubble;
    juce::Component* bubbleFor = nullptr;
    juce::String bubbleParam;
    void placeBubble();
    juce::Rectangle<float> logoArea;
    float led1 = 0.0f, led2 = 0.0f;
    juce::String learnId;
};

} // namespace ds::ui

//==============================================================================
class DsEditor : public juce::AudioProcessorEditor
{
public:
    explicit DsEditor (DsProcessor&);
    ~DsEditor() override = default;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff0d0d0d)); }

private:
    DsProcessor& proc;
    ds::ui::Panel panel;
    juce::TooltipWindow tooltips { this, 600 };
};
