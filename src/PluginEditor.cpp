#include "PluginEditor.h"
#include "BinaryData.h"

using namespace juce;
using namespace ds;
using namespace ds::ui;

namespace
{
const Colour kPanel   (0xff26241f);   // nero caldo del pannello
const Colour kPanelHi (0xff2e2c27);
const Colour kLine    (0xff9a4a37);   // filetti rosso mattone delle sezioni
const Colour kPrint   (0xffe6dcc2);   // serigrafia crema
const Colour kWood    (0xff5a2a15);

float degToRad (float d) { return d * MathConstants<float>::pi / 180.0f; }
Point<float> polar (Point<float> c, float r, float deg)
{
    const float a = degToRad (deg);
    return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
}

void text (Graphics& g, const String& s, Point<float> c, float h, Justification j = Justification::centred, float w = 240.0f)
{
    g.setFont (Fonts::get().label (h));
    Rectangle<float> r (c.x - w * 0.5f, c.y - h * 0.75f, w, h * 1.5f);
    if (j == Justification::centredRight) r = Rectangle<float> (c.x - w, c.y - h * 0.75f, w, h * 1.5f);
    if (j == Justification::centredLeft)  r = Rectangle<float> (c.x, c.y - h * 0.75f, w, h * 1.5f);
    g.drawText (s, r, j, false);
}

// titolo di sezione: Michroma ingrossato con un contorno dello stesso colore
void title (Graphics& g, const String& s, Point<float> c, float h)
{
    GlyphArrangement ga;
    ga.addLineOfText (Fonts::get().label (h).withExtraKerningFactor (0.08f), s, 0.0f, 0.0f);
    Path p; ga.createPath (p);
    const auto b = p.getBounds();
    p.applyTransform (AffineTransform::translation (c.x - b.getCentreX(), c.y - b.getCentreY()));
    g.fillPath (p);
    g.strokePath (p, PathStrokeType (h * 0.07f));
}

Path roundedPoly (const std::vector<Point<float>>& pts, float r)
{
    Path p;
    p.startNewSubPath (pts.front());
    for (size_t i = 1; i < pts.size(); ++i) p.lineTo (pts[i]);
    p.closeSubPath();
    return p.createPathWithRoundedCorners (r);
}

// icone serigrafate delle forme d'onda
void icon (Graphics& g, const String& sym, Point<float> c, float s)
{
    Path p;
    const float x0 = c.x - s * 0.5f, x1 = c.x + s * 0.5f, yT = c.y - s * 0.3f, yB = c.y + s * 0.3f;
    if (sym == "saw")      { p.startNewSubPath (x0, yB); p.lineTo (x1, yT); p.lineTo (x1, yB); }
    else if (sym == "rsaw") { p.startNewSubPath (x0, yB); p.lineTo (x0, yT); p.lineTo (x1, yB); }
    else if (sym == "tri") { p.startNewSubPath (x0, c.y); p.lineTo (x0 + s * 0.25f, yT); p.lineTo (x0 + s * 0.75f, yB); p.lineTo (x1, c.y); }
    else if (sym == "sq")  { p.startNewSubPath (x0, yB); p.lineTo (x0, yT); p.lineTo (c.x, yT); p.lineTo (c.x, yB); p.lineTo (x1, yB); }
    else if (sym == "pulse")
    {
        p.startNewSubPath (x0, yB); p.lineTo (x0, yT); p.lineTo (x0 + s * 0.3f, yT); p.lineTo (x0 + s * 0.3f, yB); p.lineTo (x1, yB);
        p.startNewSubPath (x0 + s * 0.15f, yT - s * 0.22f); p.lineTo (x0 + s * 0.45f, yT - s * 0.22f);   // larghezza variabile
    }
    else if (sym == "stair")
    {
        const float h = (yB - yT) / 4.0f, w = s / 5.0f;
        p.startNewSubPath (x0, yT);
        for (int i = 0; i < 4; ++i) { p.lineTo (x0 + w * (float) (i + 1), yT + h * (float) i); p.lineTo (x0 + w * (float) (i + 1), yT + h * (float) (i + 1)); }
        p.lineTo (x1, yB);
    }
    g.strokePath (p, PathStrokeType (s * 0.09f, PathStrokeType::mitered, PathStrokeType::square));
}

void screw (Graphics& g, Point<float> c, float r = 6.5f)
{
    g.setGradientFill (ColourGradient (Colour (0xff3c3a36), c.x - r, c.y - r, Colour (0xff0e0d0c), c.x + r, c.y + r, false));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    g.setColour (Colours::black.withAlpha (0.8f));
    g.drawLine (c.x - r * 0.7f, c.y + r * 0.2f, c.x + r * 0.7f, c.y - r * 0.2f, 1.6f);
    g.drawLine (c.x - r * 0.2f, c.y - r * 0.7f, c.x + r * 0.2f, c.y + r * 0.7f, 1.6f);
}

const std::vector<String> kSrc { "OUT", "1", "2", "1+2" };

// Carattere squadrato a tratti spezzati, come "DIGITAL SYNTHESIZER DS 2" sul pannello del DS-2
// (griglia 4 x 6; ogni glifo è un insieme di spezzate).
struct DigiGlyph { char c; std::vector<std::vector<Point<float>>> lines; };
const std::vector<DigiGlyph>& digiGlyphs()
{
    using P = Point<float>;
    static const std::vector<DigiGlyph> g {
        { 'D', { { P (0,0), P (3,0), P (4,1), P (4,5), P (3,6), P (0,6), P (0,0) } } },
        { 'I', { { P (2,0), P (2,6) } } },
        { 'G', { { P (4,0), P (0,0), P (0,6), P (4,6), P (4,3), P (2,3) } } },
        { 'T', { { P (0,0), P (4,0) }, { P (2,0), P (2,6) } } },
        { 'A', { { P (0,6), P (0,1), P (1,0), P (3,0), P (4,1), P (4,6) }, { P (0,3.4f), P (4,3.4f) } } },
        { 'L', { { P (0,0), P (0,6), P (4,6) } } },
        { 'S', { { P (4,0), P (0,0), P (0,3), P (4,3), P (4,6), P (0,6) } } },
        { 'Y', { { P (0,0), P (0,3), P (4,3) }, { P (4,0), P (4,3) }, { P (2,3), P (2,6) } } },
        { 'N', { { P (0,6), P (0,0), P (4,6), P (4,0) } } },
        { 'H', { { P (0,0), P (0,6) }, { P (4,0), P (4,6) }, { P (0,3), P (4,3) } } },
        { 'E', { { P (4,0), P (0,0), P (0,6), P (4,6) }, { P (0,3), P (3,3) } } },
        { 'Z', { { P (0,0), P (4,0), P (0,6), P (4,6) } } },
        { 'R', { { P (0,6), P (0,0), P (4,0), P (4,3), P (0,3) }, { P (2,3), P (4,6) } } },
        { 'C', { { P (4,0), P (0,0), P (0,6), P (4,6) } } },
        { 'O', { { P (0,0), P (4,0), P (4,6), P (0,6), P (0,0) } } },
        { 'F', { { P (4,0), P (0,0), P (0,6) }, { P (0,3), P (3,3) } } },
        { '2', { { P (0,0), P (4,0), P (4,3), P (0,3), P (0,6), P (4,6) } } },
    };
    return g;
}

// pos = estremo sinistro (o destro se alignRight) della riga, y al centro del testo
void digital (Graphics& g, const String& txt, Point<float> pos, float h, bool alignRight)
{
    const float u = h / 6.0f, adv = u * 5.6f;
    float width = 0.0f;
    for (auto ch : txt) width += ch == ' ' ? u * 4.0f : adv;
    float x = alignRight ? pos.x - width + u * 1.6f : pos.x;
    const float y0 = pos.y - h * 0.5f;
    for (auto ch : txt)
    {
        if (ch == ' ') { x += u * 4.0f; continue; }
        for (auto& gl : digiGlyphs())
        {
            if (gl.c != (char) ch) continue;
            for (auto& line : gl.lines)
                for (size_t i = 1; i < line.size(); ++i)
                {
                    const Point<float> a (x + line[i - 1].x * u, y0 + line[i - 1].y * u), b (x + line[i].x * u, y0 + line[i].y * u);
                    const float L = a.getDistanceFrom (b), gap = jmin (u * 0.35f, L * 0.3f);
                    const auto d = (b - a) / L;
                    g.drawLine (Line<float> (a + d * gap, b - d * gap), u * 0.95f);   // tratti spezzati agli angoli
                }
        }
        x += adv;
    }
}
} // namespace

//==============================================================================
Fonts& Fonts::get() { static Fonts f; return f; }
Fonts::Fonts()
{
    wide = Typeface::createSystemTypefaceFor (BinaryData::Michroma_ttf, BinaryData::Michroma_ttfSize);
    slab = Typeface::createSystemTypefaceFor (BinaryData::Ultra_ttf, BinaryData::Ultra_ttfSize);
}
Font Fonts::label (float h) const { return Font (FontOptions (wide).withHeight (h)); }
Font Fonts::logo (float h) const  { return Font (FontOptions (slab).withHeight (h)); }

//==============================================================================
Knob::Knob (LearnHost& h, String id, bool selector, int numPositions)
    : host (h), paramId (std::move (id)), isSelector (selector), positions (numPositions)
{
    setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    setMouseDragSensitivity (selector ? 120 : 260);
    setVelocityBasedMode (false);
}
void Knob::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { host.showControlMenu (*this, paramId); return; }
    dragged = false;
    if (isSelector) { Slider::mouseDown (e); return; }
    // manopola: trascinamento gestito qui per avere la regolazione fine con ⇧ o ⌘
    dragging = true;
    last = e.position;
    if (onDragStart) onDragStart();
    host.showValue (*this, paramId, true);
}
void Knob::mouseDrag (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (e.getDistanceFromDragStart() > 3) dragged = true;
    if (isSelector) { Slider::mouseDrag (e); return; }
    const float delta = (last.y - e.position.y) + (e.position.x - last.x);
    last = e.position;
    const double sens = Drag::fine (e.mods) ? 2400.0 : 260.0;   // pixel per tutta la corsa
    const double prop = jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + (double) delta / sens);
    setValue (proportionOfLengthToValue (prop), sendNotificationSync);
}
void Knob::mouseUp (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (! isSelector)
    {
        dragging = false;
        if (onDragEnd) onDragEnd();
        if (! isMouseOver()) host.showValue (*this, paramId, false);
        return;
    }
    Slider::mouseUp (e);
    // un clic senza trascinare fa avanzare il selettore di una posizione
    if (! dragged)
    {
        double v = getValue() + 1.0;
        if (v > getMaximum()) v = getMinimum();
        setValue (v, sendNotificationSync);
    }
}
void Knob::mouseDoubleClick (const MouseEvent& e)
{
    // doppio clic: valore predefinito (i selettori avanzano già con il clic singolo)
    if (isSelector || e.mods.isPopupMenu()) return;
    if (onDragStart) onDragStart();
    setValue (defaultValue, sendNotificationSync);
    if (onDragEnd) onDragEnd();
}
void Knob::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const float d = (w.isReversed ? -1.0f : 1.0f) * (std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : w.deltaX);
    if (std::abs (d) < 1.0e-6f) return;
    if (isSelector)
    {
        setValue (jlimit (getMinimum(), getMaximum(), getValue() + (d > 0 ? 1.0 : -1.0)), sendNotificationSync);
        return;
    }
    const double step = Drag::fine (e.mods) ? 0.03 : 0.25;
    setValue (proportionOfLengthToValue (jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + (double) d * step)), sendNotificationSync);
    host.showValue (*this, paramId, true);
}

void Knob::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float prop = (float) valueToProportionOfLength (getValue());
    const float deg = startDeg + prop * (endDeg - startDeg);

    if (! isSelector && showTicks)
    {
        g.setColour (kPrint);
        for (int i = 0; i <= 10; ++i)
        {
            const float ang = -135.0f + 27.0f * (float) i;
            g.drawLine (Line<float> (polar (c, radius + 1.0f, ang), polar (c, radius + 3.5f, ang)), i % 5 == 0 ? 1.6f : 1.0f);
        }
    }
    if (! isSelector)
    {
        // manopola nera zigrinata con indice crema
        const float rs = radius, rc = radius * 0.72f;
        g.setColour (Colours::black.withAlpha (0.55f));
        g.fillEllipse (c.x - rs + 2.0f, c.y - rs + 3.5f, rs * 2.0f, rs * 2.0f);
        g.setGradientFill (ColourGradient (Colour (0xff3a3936), c.x - rs, c.y - rs, Colour (0xff030303), c.x + rs, c.y + rs, false));
        g.fillEllipse (c.x - rs, c.y - rs, rs * 2.0f, rs * 2.0f);
        g.setColour (Colours::black.withAlpha (0.75f));
        for (int i = 0; i < 48; ++i)
            g.drawLine (Line<float> (polar (c, rc + 1.0f, (float) i * 7.5f), polar (c, rs - 0.6f, (float) i * 7.5f)), 1.0f);
        g.setGradientFill (ColourGradient (Colour (0xff4d4b47), c.x - rc * 0.6f, c.y - rc * 0.8f, Colour (0xff080808), c.x + rc * 0.5f, c.y + rc, true));
        g.fillEllipse (c.x - rc, c.y - rc, rc * 2.0f, rc * 2.0f);
        g.setColour (Colours::black);
        g.drawEllipse (c.x - rc, c.y - rc, rc * 2.0f, rc * 2.0f, 1.0f);
        g.setColour (kPrint);
        g.drawLine (Line<float> (polar (c, rc * 0.2f, deg), polar (c, rs - 1.0f, deg)), 2.6f);
    }
    else
    {
        // selettore a puntatore (a goccia) come quelli del DS-2
        const float r = radius * 0.62f, tip = radius * 1.05f;
        Path body;
        body.addCentredArc (0, 0, r, r, 0.0f, degToRad (35.0f), degToRad (325.0f), true);
        body.lineTo (0.0f, -tip);
        body.closeSubPath();
        auto tf = AffineTransform::rotation (degToRad (deg)).translated (c);
        Path shadow (body);
        shadow.applyTransform (AffineTransform::rotation (degToRad (deg)).translated (c.x + 2.5f, c.y + 4.0f));
        g.setColour (Colours::black.withAlpha (0.55f)); g.fillPath (shadow);
        body.applyTransform (tf);
        g.setGradientFill (ColourGradient (Colour (0xff45433f), c.x - r, c.y - r, Colour (0xff020202), c.x + r, c.y + r, false));
        g.fillPath (body);
        g.setColour (Colours::black); g.strokePath (body, PathStrokeType (1.0f));
        // riflesso sulla calotta
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.18f), c.x - r * 0.4f, c.y - r * 0.5f, Colours::transparentWhite, c.x + r * 0.3f, c.y + r * 0.4f, true));
        g.fillEllipse (c.x - r * 0.8f, c.y - r * 0.8f, r * 1.6f, r * 1.6f);
        g.setColour (kPrint);
        g.drawLine (Line<float> (polar (c, r * 0.1f, deg), polar (c, tip - 3.0f, deg)), 2.2f);
    }

    if (host.isLearning (paramId))
    {
        g.setColour (Colour (0xffff3b30));
        g.drawEllipse (b.reduced (1.0f), 2.2f);
    }
}

//==============================================================================
Toggle::Toggle (LearnHost& h, String id, bool horizontal, bool onIsFirst)
    : Button (id), host (h), paramId (std::move (id)), horiz (horizontal), onFirst (onIsFirst)
{
    setClickingTogglesState (true);
}
void Toggle::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { host.showControlMenu (*this, paramId); return; }
    Button::mouseDown (e);
}
void Toggle::paintButton (Graphics& g, bool over, bool)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    // dado esagonale cromato
    Path nut; nut.addPolygon (c, 6, 11.0f, 0.0f);
    g.setGradientFill (ColourGradient (Colour (0xffd9d6cf), c.x - 10, c.y - 10, Colour (0xff4a4844), c.x + 10, c.y + 10, false));
    g.fillPath (nut);
    g.setColour (Colours::black.withAlpha (0.6f)); g.strokePath (nut, PathStrokeType (0.8f));
    g.setColour (Colour (0xff111111)); g.fillEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);
    // collare filettato attorno al foro
    g.setGradientFill (ColourGradient (Colour (0xffc9c5bd), c.x - 6, c.y - 6, Colour (0xff3c3a36), c.x + 6, c.y + 6, false));
    g.fillEllipse (c.x - 6.5f, c.y - 6.5f, 13.0f, 13.0f);
    g.setColour (Colour (0xff0c0c0c)); g.fillEllipse (c.x - 3.6f, c.y - 3.6f, 7.2f, 7.2f);

    // levetta: esce dal foro verso chi guarda ed è inclinata di ~20° verso la posizione scelta,
    // quindi vista di fronte appare corta (in scorcio) con la punta arrotondata più grande
    const bool atFirst = getToggleState() == onFirst;
    const Point<float> dir = horiz ? Point<float> (atFirst ? -1.0f : 1.0f, 0.0f) : Point<float> (0.0f, atFirst ? -1.0f : 1.0f);
    const Point<float> nrm (-dir.y, dir.x);
    const float reach = 9.0f;                         // proiezione della leva sul pannello
    const auto tip = c + dir * reach;

    // ombra portata sul pannello (luce dall'alto a sinistra): più lunga della leva
    {
        Path sh;
        const auto s0 = c.translated (1.5f, 2.0f), s1 = c + dir * (reach + 9.0f) + Point<float> (4.0f, 6.0f);
        sh.startNewSubPath (s0 + nrm * 2.0f); sh.lineTo (s1 + nrm * 3.5f); sh.lineTo (s1 - nrm * 3.5f); sh.lineTo (s0 - nrm * 2.0f); sh.closeSubPath();
        g.setColour (Colours::black.withAlpha (0.38f)); g.fillPath (sh);
        g.setColour (Colours::black.withAlpha (0.30f)); g.fillEllipse (Rectangle<float> (9.0f, 9.0f).withCentre (s1));
    }
    // fusto rastremato (stretto alla base, più largo verso la punta)
    {
        Path stem;
        stem.startNewSubPath (c + nrm * 2.2f); stem.lineTo (tip + nrm * 3.4f); stem.lineTo (tip - nrm * 3.4f); stem.lineTo (c - nrm * 2.2f); stem.closeSubPath();
        g.setGradientFill (ColourGradient (Colour (0xfff4f2ec), (c + nrm * 3.0f).x, (c + nrm * 3.0f).y,
                                           Colour (0xff77746d), (c - nrm * 3.0f).x, (c - nrm * 3.0f).y, false));
        g.fillPath (stem);
        g.setColour (Colours::black.withAlpha (0.35f)); g.strokePath (stem, PathStrokeType (0.6f));
    }
    // punta sferica, la parte più vicina all'osservatore
    {
        const float r = 5.6f;
        auto ball = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (tip);
        g.setGradientFill (ColourGradient (Colour (0xffffffff), tip.x - r * 0.45f, tip.y - r * 0.5f,
                                           Colour (0xff6f6c66), tip.x + r, tip.y + r, true));
        g.fillEllipse (ball);
        g.setColour (Colours::black.withAlpha (0.45f)); g.drawEllipse (ball, 0.7f);
        g.setColour (Colours::white.withAlpha (0.85f)); g.fillEllipse (tip.x - r * 0.55f, tip.y - r * 0.6f, r * 0.55f, r * 0.45f);
    }
    if (over) { g.setColour (Colours::white.withAlpha (0.06f)); g.fillEllipse (b.reduced (2.0f)); }
    if (host.isLearning (paramId)) { g.setColour (Colour (0xffff3b30)); g.drawEllipse (b.reduced (1.0f), 2.0f); }
}

//==============================================================================
Fader::Fader (LearnHost& h, String id, bool horizontal)
    : host (h), paramId (std::move (id)), horiz (horizontal)
{
    setSliderStyle (horizontal ? Slider::LinearHorizontal : Slider::LinearVertical);
    setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    setSliderSnapsToMousePosition (false);
}
void Fader::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { host.showControlMenu (*this, paramId); return; }
    dragging = true;
    last = e.position;
    if (onDragStart) onDragStart();
    host.showValue (*this, paramId, true);
}
void Fader::mouseDrag (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    const float delta = horiz ? e.position.x - last.x : last.y - e.position.y;
    last = e.position;
    const double travel = (double) (horiz ? getWidth() : getHeight()) - 14.0;
    const double sens = Drag::fine (e.mods) ? travel * 8.0 : travel;   // normale: il cappuccio segue il mouse
    const double prop = jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + (double) delta / sens);
    setValue (proportionOfLengthToValue (prop), sendNotificationSync);
}
void Fader::mouseUp (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    dragging = false;
    if (onDragEnd) onDragEnd();
    if (! isMouseOver()) host.showValue (*this, paramId, false);
}
void Fader::mouseDoubleClick (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    if (onDragStart) onDragStart();
    setValue (defaultValue, sendNotificationSync);
    if (onDragEnd) onDragEnd();
}
void Fader::mouseWheelMove (const MouseEvent& e, const MouseWheelDetails& w)
{
    const float d = (w.isReversed ? -1.0f : 1.0f) * (std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : w.deltaX);
    if (std::abs (d) < 1.0e-6f) return;
    const double step = Drag::fine (e.mods) ? 0.03 : 0.25;
    setValue (proportionOfLengthToValue (jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + (double) d * step)), sendNotificationSync);
    host.showValue (*this, paramId, true);
}
void Fader::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float prop = (float) valueToProportionOfLength (getValue());
    // fessura
    auto slot = horiz ? b.withSizeKeepingCentre (b.getWidth() - 14.0f, 7.0f) : b.withSizeKeepingCentre (7.0f, b.getHeight() - 14.0f);
    g.setColour (Colour (0xff050505)); g.fillRoundedRectangle (slot, 3.0f);
    g.setColour (Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (slot.translated (0, 1), 3.0f, 0.8f);
    // cappuccio
    Rectangle<float> cap;
    if (horiz)
    {
        const float x = slot.getX() + prop * slot.getWidth();
        cap = Rectangle<float> (16.0f, 34.0f).withCentre ({ x, b.getCentreY() });
    }
    else
    {
        const float y = slot.getBottom() - prop * slot.getHeight();
        cap = Rectangle<float> (36.0f, 17.0f).withCentre ({ b.getCentreX(), y });
    }
    g.setColour (Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (cap.translated (2.0f, 3.0f), 3.0f);
    g.setGradientFill (ColourGradient (Colour (0xff4a4844), cap.getX(), cap.getY(), Colour (0xff060606), cap.getRight(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (Colours::black); g.drawRoundedRectangle (cap, 3.0f, 1.0f);
    g.setColour (kPrint);
    if (horiz) g.drawLine (cap.getCentreX(), cap.getY() + 3.0f, cap.getCentreX(), cap.getBottom() - 3.0f, 2.0f);
    else       g.drawLine (cap.getX() + 3.0f, cap.getCentreY(), cap.getRight() - 3.0f, cap.getCentreY(), 2.0f);
    if (host.isLearning (paramId)) { g.setColour (Colour (0xffff3b30)); g.drawRoundedRectangle (b.reduced (1.0f), 4.0f, 2.0f); }
}

//==============================================================================
BendLever::BendLever (DsProcessor& p, LearnHost& h) : proc (p), host (h)
{
    param = proc.apvts.getParameter (pid::bend);
    setTooltip (String::fromUTF8 ("PITCH BENDING: trascina a destra/sinistra (±terza minore, torna al centro). Segue anche la rotella pitch bend MIDI."));
}
void BendLever::refresh()
{
    const float v = jlimit (-1.0f, 1.0f, proc.apvts.getRawParameterValue (pid::bend)->load() + proc.wheelBend());
    if (std::abs (v - shown) > 0.003f) { shown = v; repaint(); }
}
void BendLever::setFromX (float x)
{
    const float v = jlimit (-1.0f, 1.0f, (x - (float) getWidth() * 0.5f) / ((float) getWidth() * 0.38f));
    param->setValueNotifyingHost (param->convertTo0to1 (v));
}
void BendLever::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { host.showControlMenu (*this, pid::bend); return; }
    param->beginChangeGesture(); setFromX (e.position.x);
}
void BendLever::mouseDrag (const MouseEvent& e) { if (! e.mods.isPopupMenu()) setFromX (e.position.x); }
void BendLever::mouseUp (const MouseEvent& e)
{
    if (e.mods.isPopupMenu()) return;
    param->setValueNotifyingHost (param->convertTo0to1 (0.0f));   // molla
    param->endChangeGesture();
}
void BendLever::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const Point<float> pivot (b.getCentreX(), b.getBottom() - 12.0f);
    const float len = b.getHeight() - 30.0f;
    const auto top = polar (pivot, len, shown * 22.0f);
    g.setColour (Colours::black.withAlpha (0.5f)); g.drawLine (Line<float> (pivot.translated (3, 3), top.translated (3, 3)), 14.0f);
    g.setGradientFill (ColourGradient (Colour (0xff3d3b38), top.x - 8, top.y, Colour (0xff050505), top.x + 8, top.y, false));
    g.drawLine (Line<float> (pivot, top), 13.0f);
    g.setGradientFill (ColourGradient (Colour (0xff57544f), top.x - 12, top.y - 12, Colour (0xff050505), top.x + 12, top.y + 12, false));
    g.fillEllipse (top.x - 15.0f, top.y - 15.0f, 30.0f, 30.0f);
    g.setColour (Colours::black); g.drawEllipse (top.x - 15.0f, top.y - 15.0f, 30.0f, 30.0f, 1.0f);
    if (host.isLearning (pid::bend)) { g.setColour (Colour (0xffff3b30)); g.drawRect (b, 2.0f); }
}

//==============================================================================
void ValueBubble::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (b.translated (2, 3), 7.0f);
    g.setColour (Colour (0xf2141310)); g.fillRoundedRectangle (b, 7.0f);
    g.setColour (kLine); g.drawRoundedRectangle (b, 7.0f, 1.6f);
    g.setColour (kPrint.withMultipliedBrightness (0.75f));
    g.setFont (Fonts::get().label (10.5f));
    g.drawText (title, b.removeFromTop (b.getHeight() * 0.45f).reduced (8.0f, 0.0f).translated (0, 3), Justification::centred, true);
    g.setColour (kPrint);
    g.setFont (Fonts::get().label (15.0f));
    g.drawText (value, b.reduced (8.0f, 0.0f).translated (0, -2), Justification::centred, true);
}

//==============================================================================
Rectangle<float> PresetBar::prevR() const { return { 0.0f, 6.0f, 40.0f, (float) getHeight() - 12.0f }; }
Rectangle<float> PresetBar::nameR() const { return { 46.0f, 0.0f, 340.0f, (float) getHeight() }; }
Rectangle<float> PresetBar::nextR() const { return { 392.0f, 6.0f, 40.0f, (float) getHeight() - 12.0f }; }
Rectangle<float> PresetBar::abR()   const { return { 448.0f, 6.0f, 64.0f, (float) getHeight() - 12.0f }; }
Rectangle<float> PresetBar::saveR() const { return { 520.0f, 6.0f, 80.0f, (float) getHeight() - 12.0f }; }

void PresetBar::paint (Graphics& g)
{
    auto button = [&] (Rectangle<float> r, bool lit)
    {
        g.setColour (Colours::black.withAlpha (0.45f)); g.fillRoundedRectangle (r.translated (1.5f, 2.5f), 5.0f);
        g.setGradientFill (ColourGradient (Colour (0xff3a3833), r.getX(), r.getY(), Colour (0xff0d0c0b), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (lit ? kLine.brighter (0.2f) : Colours::black);
        g.drawRoundedRectangle (r, 5.0f, lit ? 2.0f : 1.0f);
    };
    // display
    auto d = nameR();
    g.setColour (Colour (0xff0b0a08)); g.fillRoundedRectangle (d, 6.0f);
    g.setColour (kLine); g.drawRoundedRectangle (d.reduced (0.5f), 6.0f, 1.6f);
    g.setColour (kPrint.withMultipliedBrightness (0.6f));
    g.setFont (Fonts::get().label (9.0f));
    g.drawText ("PRESET", d.withTrimmedLeft (12.0f).withHeight (20.0f).translated (0, 4), Justification::centredLeft, false);
    g.setColour (kPrint);
    g.setFont (Fonts::get().label (14.0f));
    g.drawFittedText (shownName + (shownMod ? "  *" : ""), d.reduced (12.0f, 0.0f).withTrimmedTop (14.0f).toNearestInt(),
                      Justification::centredLeft, 1, 0.7f);
    // ▾ menu
    {
        Path tri; const float cx = d.getRight() - 16.0f, cy = d.getCentreY() + 5.0f;
        tri.addTriangle (cx - 5, cy - 3, cx + 5, cy - 3, cx, cy + 3);
        g.setColour (kPrint.withMultipliedBrightness (0.7f)); g.fillPath (tri);
    }
    // ◀ ▶
    for (int i = 0; i < 2; ++i)
    {
        auto r = i == 0 ? prevR() : nextR();
        button (r, false);
        Path t; const auto c = r.getCentre();
        if (i == 0) t.addTriangle (c.x + 5, c.y - 7, c.x + 5, c.y + 7, c.x - 6, c.y);
        else        t.addTriangle (c.x - 5, c.y - 7, c.x - 5, c.y + 7, c.x + 6, c.y);
        g.setColour (kPrint); g.fillPath (t);
    }
    // A/B
    {
        auto r = abR(); button (r, shownB);
        auto half = r.reduced (6.0f, 0.0f);
        auto ra = half.removeFromLeft (half.getWidth() * 0.5f);
        g.setFont (Fonts::get().label (14.0f));
        g.setColour (shownB ? kPrint.withMultipliedBrightness (0.45f) : kPrint);
        g.drawText ("A", ra, Justification::centred, false);
        g.setColour (shownB ? kPrint : kPrint.withMultipliedBrightness (0.45f));
        g.drawText ("B", half, Justification::centred, false);
    }
    // SALVA
    {
        auto r = saveR(); button (r, false);
        g.setColour (kPrint); g.setFont (Fonts::get().label (11.5f));
        g.drawText ("SALVA", r, Justification::centred, false);
    }
}

void PresetBar::refresh()
{
    const auto n = proc.currentPresetName();
    const bool mod = proc.presetModified(), b = proc.isOnB();
    if (n != shownName || mod != shownMod || b != shownB) { shownName = n; shownMod = mod; shownB = b; repaint(); }
}

void PresetBar::mouseDown (const MouseEvent& e)
{
    const auto p = e.position;
    if (abR().contains (p))
    {
        if (e.mods.isPopupMenu())
        {
            PopupMenu m;
            m.addItem (1, proc.isOnB() ? "Copia B in A" : "Copia A in B");
            m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this] (int r) { if (r == 1) proc.copyCurrentToOther(); });
        }
        else proc.toggleAB();
    }
    else if (prevR().contains (p)) proc.stepPreset (-1);
    else if (nextR().contains (p)) proc.stepPreset (1);
    else if (saveR().contains (p)) save();
    else if (nameR().contains (p)) showMenu();
    refresh();
}

void PresetBar::showMenu()
{
    PopupMenu m, um;
    const auto& list = ds::presets::factory();
    const auto users = proc.userPresets();
    for (int i = 0; i < users.size(); ++i)
        um.addItem (1000 + i, users[i].getFileNameWithoutExtension(), true, users[i].getFileNameWithoutExtension() == proc.currentPresetName());
    if (users.isEmpty()) um.addItem (999, "(nessun preset salvato)", false);
    m.addSectionHeader ("Preset di fabbrica");
    for (int i = 0; i < (int) list.size(); ++i)
        m.addItem (1 + i, String::fromUTF8 (list[(size_t) i].name), true, proc.currentPresetName() == String::fromUTF8 (list[(size_t) i].name));
    m.addSeparator();
    m.addSubMenu ("Preset utente", um, true);
    m.addSeparator();
    m.addItem (2000, "Salva come...");
    m.addItem (2001, "Mostra la cartella dei preset");
    m.addItem (2002, proc.isOnB() ? "Copia B in A" : "Copia A in B");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this), [this, users] (int r)
    {
        if (r >= 1 && r < 1000) proc.loadFactory (r - 1);
        else if (r >= 1000 && r < 2000) proc.loadUserPreset (users[r - 1000]);
        else if (r == 2000) save();
        else if (r == 2001) { DsProcessor::userPresetFolder().createDirectory(); DsProcessor::userPresetFolder().revealToUser(); }
        else if (r == 2002) proc.copyCurrentToOther();
        refresh();
    });
}

void PresetBar::save()
{
    auto folder = DsProcessor::userPresetFolder();
    folder.createDirectory();
    chooser = std::make_unique<FileChooser> ("Salva il preset", folder.getChildFile (proc.currentPresetName() + ".dissona2"), "*.dissona2");
    chooser->launchAsync (FileBrowserComponent::saveMode | FileBrowserComponent::canSelectFiles | FileBrowserComponent::warnAboutOverwriting,
                          [this] (const FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f != File()) proc.saveUserPreset (f);
        refresh();
    });
}

//==============================================================================
Keys::Keys (MidiKeyboardState& s) : MidiKeyboardComponent (s, horizontalKeyboard)
{
    setAvailableRange (41, 84);           // 44 tasti, dal FA al DO, come l'originale
    setScrollButtonsVisible (false);
    setBlackNoteLengthProportion (0.62f);
    setBlackNoteWidthProportion (0.55f);
    setOctaveForMiddleC (4);
    setVelocity (1.0f, false);
    setColour (shadowColourId, Colours::black.withAlpha (0.5f));
    setColour (keySeparatorLineColourId, Colour (0xff6c6a64));
    setColour (mouseOverKeyOverlayColourId, Colours::transparentBlack);
    setColour (keyDownOverlayColourId, Colours::transparentBlack);
}
void Keys::drawWhiteNote (int, Graphics& g, Rectangle<float> a, bool down, bool, Colour, Colour)
{
    auto k = a.reduced (1.0f, 0.0f);
    const Colour top = down ? Colour (0xffd2cec4) : Colour (0xfff5f2ea);
    const Colour bot = down ? Colour (0xffbcb8ad) : Colour (0xffe0dcd1);
    g.setGradientFill (ColourGradient (top, k.getX(), k.getY(), bot, k.getX(), k.getBottom(), false));
    g.fillRect (k.withTrimmedBottom (4.0f));
    g.setColour (down ? Colour (0xffa29e94) : Colour (0xffc9c5bb));
    g.fillRect (k.withTop (k.getBottom() - (down ? 2.0f : 5.0f)));
    g.setColour (Colour (0xff3a3834));
    g.drawRect (a.withTrimmedRight (-0.5f), 1.0f);
}
void Keys::drawBlackNote (int, Graphics& g, Rectangle<float> a, bool down, bool, Colour)
{
    g.setColour (Colours::black);
    g.fillRect (a);
    auto top = a.reduced (a.getWidth() * 0.12f, 0.0f).withTrimmedBottom (a.getHeight() * (down ? 0.05f : 0.11f));
    g.setGradientFill (ColourGradient (down ? Colour (0xff2a2a2a) : Colour (0xff3c3c3c), top.getX(), top.getY(),
                                       Colour (0xff0b0b0b), top.getRight(), top.getY(), false));
    g.fillRect (top);
    g.setColour (Colours::white.withAlpha (down ? 0.05f : 0.15f));
    g.fillRect (top.withWidth (top.getWidth() * 0.22f).translated (top.getWidth() * 0.12f, 0.0f));
}

//==============================================================================
Panel::Panel (DsProcessor& p) : proc (p), keys (p.keyboardState), lever (p, *this), presetBar (p)
{
    setSize (W, H);

    knobSpecs = {
        { pid::lfo1Rate, 500, 88,  Scale::Ten, "RATE", "LFO 1: velocità (0,3 … 30 Hz)" },
        { pid::lfo2Rate, 905, 88,  Scale::Ten, "RATE", "LFO 2: velocità (0,3 … 30 Hz)" },
        { pid::lfoDelay, 1338, 86, Scale::Ten, "", "LFO's DELAY: ritardo prima che gli LFO con DELAY IN entrino in azione" },
        { pid::o1Vol,    497, 222, Scale::Ten, "VOLUME", "OSC 1: volume" },
        { pid::o1Pitch,  607, 222, Scale::Bipolar5, "PITCH", "OSC 1: intonazione (±½ ottava). È l'oscillatore di riferimento per accordare" },
        { pid::o2Vol,    725, 222, Scale::Ten, "VOLUME", "OSC 2: volume" },
        { pid::o2Pitch,  835, 222, Scale::Bipolar5, "PITCH", "OSC 2: intonazione (±¾ di ottava): unisono, intervalli o battimenti" },
        { pid::vcfEnv,   958, 216, Scale::Ten, "ADSR\nRANGE", "VCF: quantità dell'inviluppo sul taglio" },
        { pid::vcfCut,   1070, 216, Scale::Ten, "CUTOFF\nFILTER", "VCF: frequenza di taglio" },
        { pid::vcfRes,   1180, 216, Scale::Ten, "RESONANCE", "VCF: risonanza (oltre 8½ il filtro autooscilla)" },
        { pid::vcaEnv,   1338, 216, Scale::Ten, "ADSR\nRANGE", "VCA: quantità dell'inviluppo sull'amplificatore" },
        { pid::noiseVol, 1492, 216, Scale::Ten, "VOLUME", "NOISE: volume del rumore" },
        { pid::polyHigh, 1612, 216, Scale::Db15, "HIGH  PASS", "POLY: acuti ±15 dB" },
        { pid::polyLow,  1730, 216, Scale::Db15, "LOW  PASS", "POLY: bassi ±15 dB" },
        { pid::polyVol,  1612, 336, Scale::Ten, "VOLUME", "POLY: volume della sezione polifonica" },
        { pid::volume,   1730, 432, Scale::Ten, "", "VOLUME generale" },
        { pid::polyPitch, 210, 706, Scale::Bipolar4, "PITCH\nPOLY", "POLY: intonazione (±4 semitoni)" },
    };

    const float L0 = -140.0f, L1 = -40.0f;   // etichette a sinistra del selettore (dal basso all'alto)
    selSpecs = {
        { pid::mxO1Src,  385, 80,  L0, L1, kSrc, 4, 38, "LFO mixer → OSC 1: sorgente (OUT, LFO 1, LFO 2, 1+2)" },
        { pid::mxO2Src,  300, 172, L0, L1, kSrc, 4, 38, "LFO mixer → OSC 2: sorgente" },
        { pid::mxVcfSrc, 390, 264, L0, L1, kSrc, 4, 38, "LFO mixer → VCF (wah-wah): sorgente" },
        { pid::mxVcaSrc, 310, 355, L0, L1, kSrc, 4, 38, "LFO mixer → VCA (tremolo): sorgente" },
        { pid::mxPwSrc,  400, 445, -130.0f, -50.0f, { "MAN.", "1", "2" }, 4, 38, "PULSE WIDTH: manuale o modulata da LFO 1 / LFO 2" },
        { pid::lfo1Wave, 775, 92,  L1, L0, { "rsaw", "tri", "sq", "saw" }, 2, 44, "LFO 1: forma d'onda (sega inversa, triangolo, quadra, sega)" },
        { pid::lfo2Wave, 1180, 90, L1, L0, { "S&H", "tri", "sq", "stair" }, 3, 44, "LFO 2: forma d'onda (S&H random ripetitivo, triangolo, quadra, staircase)" },
        { pid::o1Range,  527, 342, L0, L1, { "32'", "16'", "8'", "4'" }, 0, 38, "OSC 1: ottava (32' … 4')" },
        { pid::o1Wave,   630, 342, L1, L0, { "saw", "tri", "sq", "pulse" }, 1, 40, "OSC 1: forma d'onda (sega, triangolo, quadra, impulso a larghezza variabile)" },
        { pid::o2Range,  748, 342, L0, L1, { "32'", "16'", "8'", "4'" }, 0, 38, "OSC 2: ottava (32' … 4')" },
        { pid::o2Wave,   860, 342, -50.0f, -130.0f, { "saw", "tri", "sq" }, 1, 40, "OSC 2: forma d'onda" },
        { pid::polyLfoSrc, 1742, 86, L0, L1, kSrc, 4, 38, "POLY: LFO sulla sezione polifonica" },
        { pid::polyOn,   1628, 440, -105.0f, -55.0f, { "OUT", "IN" }, 0, 36, "POLY: sezione polifonica inserita (IN) / esclusa (OUT)" },
        { pid::noisePink, 1492, 425, -32.0f, 32.0f, { "W", "P" }, 0, 36, "NOISE: bianco (W) o rosa (P)" },
        { pid::polyOct,  96, 708, -40.0f, 40.0f, { "16'", "8'", "4'" }, 0, 38, "OCTAVE POLY: ottava della sezione polifonica" },
    };

    togSpecs = {
        { pid::lfo1Delay, 632, 108, true, false, "LFO 1: DELAY inserito (IN, a destra)" },
        { pid::lfo2Delay, 1040, 108, true, false, "LFO 2: DELAY inserito (IN, a destra)" },
        { pid::kbdCtrl,   674, 448, true, false, "KEYBOARD CONTROL: IN (a destra) il filtro segue la tastiera" },
        { pid::vcaIn,     1160, 291, true, true, "Deviatore VCF/VCA: IN (a sinistra) l'ADSR del VCF comanda anche il VCA" },
        { pid::headMain,  1732, 322, true, true, "HEAD / MAIN: uscita cuffia (a sinistra, più forte) o principale" },
        { pid::glideOn,   1745, 752, true, true, "GLIDE: IN (a sinistra) portamento attivo" },
        { pid::glideLong, 1745, 862, true, true, "SPEED RANGE: IN (a sinistra) estende i portamenti lunghi" },
    };

    fadSpecs = {
        { pid::mxO1Amt,  162, 78,  150, true, "LFO mixer: ampiezza su OSC 1 (vibrato)" },
        { pid::mxO2Amt,  162, 170, 150, true, "LFO mixer: ampiezza su OSC 2" },
        { pid::mxVcfAmt, 162, 262, 150, true, "LFO mixer: ampiezza sul VCF" },
        { pid::mxVcaAmt, 162, 353, 150, true, "LFO mixer: ampiezza sul VCA" },
        { pid::mxPwAmt,  162, 443, 150, true, "PULSE WIDTH: 50 % (sinistra) … 5 % (destra); con un LFO è la profondità" },
        { pid::polyLfoAmt, 1568, 84, 150, true, "POLY: LFO AMOUNT" },
        { pid::vcfA, 942, 408, 150, false, "VCF: attack" },
        { pid::vcfD, 1000, 408, 150, false, "VCF: decay" },
        { pid::vcfS, 1058, 408, 150, false, "VCF: sustain" },
        { pid::vcfR, 1114, 408, 150, false, "VCF: release" },
        { pid::vcaA, 1214, 408, 150, false, "VCA: attack" },
        { pid::vcaD, 1270, 408, 150, false, "VCA: decay" },
        { pid::vcaS, 1328, 408, 150, false, "VCA: sustain" },
        { pid::vcaR, 1384, 408, 150, false, "VCA: release" },
        { pid::glideSpeed, 1826, 800, 150, false, "SPEED GLIDE: velocità del portamento (in alto più veloce)" },
    };

    for (auto& s : knobSpecs) addKnob (s);
    for (auto& s : selSpecs)  addSelector (s);
    for (auto& s : togSpecs)  addToggle (s);
    for (auto& s : fadSpecs)  addFader (s);

    addAndMakeVisible (keys);
    keys.setBounds (296, 662, 1388, 282);
    keys.setKeyWidth ((float) keys.getWidth() / 26.0f);
    keys.setLowestVisibleKey (41);

    addAndMakeVisible (lever);
    lever.setBounds (Rectangle<int> (150, 120).withCentre ({ 150, 870 }));

    logoArea = { 1150.0f, 532.0f, 490.0f, 56.0f };

    // ingresso esterno (AUX): livello e modo del VCA
    {
        auto* k = knobs.add (new Knob (*this, pid::extVol, false));
        addAndMakeVisible (k);
        k->radius = 16.0f; k->showTicks = true;
        k->setBounds (Rectangle<int> (40, 40).withCentre ({ 744, 580 }));
        k->setTooltip (String::fromUTF8 ("EXT. INPUT: livello dell'ingresso audio esterno verso il filtro (in una DAW: ingresso sidechain)"));
        sliderAtt.add (new AudioProcessorValueTreeState::SliderAttachment (proc.apvts, pid::extVol, *k));
        k->defaultValue = 0.0;
        auto* t = toggles.add (new Toggle (*this, pid::extOpen, true, false));
        addAndMakeVisible (t);
        t->setBounds (Rectangle<int> (46, 46).withCentre ({ 884, 580 }));
        t->setTooltip (String::fromUTF8 ("VCA per l'ingresso esterno: GATE = si apre con la tastiera (come l'originale), OPEN = sempre aperto (DISSONA 2 come filtro)"));
        buttonAtt.add (new AudioProcessorValueTreeState::ButtonAttachment (proc.apvts, pid::extOpen, *t));
    }

    addAndMakeVisible (presetBar);
    presetBar.setBounds (70, 538, 600, 56);
    presetBar.setTooltip (String::fromUTF8 ("Preset: ◀ ▶ per scorrerli, clic sul nome per l'elenco, A/B per confrontare due versioni (clic destro: copia), SALVA per i tuoi suoni"));
    presetBar.refresh();

    addChildComponent (bubble);
    bubble.setInterceptsMouseClicks (false, false);
    bubble.setSize (190, 46);
    startTimerHz (30);
}

Panel::~Panel() { stopTimer(); }

void Panel::addKnob (const KnobSpec& s)
{
    auto* k = knobs.add (new Knob (*this, s.id, false));
    addAndMakeVisible (k);
    k->radius = 23.0f;
    k->setBounds (Rectangle<int> (52, 52).withCentre ({ (int) s.x, (int) s.y }));
    k->setTooltip (String::fromUTF8 (s.tip));
    sliderAtt.add (new AudioProcessorValueTreeState::SliderAttachment (proc.apvts, s.id, *k));
    if (auto* p = proc.apvts.getParameter (s.id)) k->defaultValue = p->convertFrom0to1 (p->getDefaultValue());
}

void Panel::addSelector (const SelSpec& s)
{
    auto* k = knobs.add (new Knob (*this, s.id, true, (int) s.labels.size()));
    k->startDeg = s.a0; k->endDeg = s.a1;
    k->radius = 22.0f;
    addAndMakeVisible (k);
    k->setBounds (Rectangle<int> (54, 54).withCentre ({ (int) s.x, (int) s.y }));
    k->setTooltip (String::fromUTF8 (s.tip));
    sliderAtt.add (new AudioProcessorValueTreeState::SliderAttachment (proc.apvts, s.id, *k));
}

void Panel::addToggle (const TogSpec& s)
{
    auto* t = toggles.add (new Toggle (*this, s.id, s.horiz, s.onFirst));
    addAndMakeVisible (t);
    t->setBounds (Rectangle<int> (46, 46).withCentre ({ (int) s.x, (int) s.y }));
    t->setTooltip (String::fromUTF8 (s.tip));
    buttonAtt.add (new AudioProcessorValueTreeState::ButtonAttachment (proc.apvts, s.id, *t));
}

void Panel::addFader (const FadSpec& s)
{
    auto* f = faders.add (new Fader (*this, s.id, s.horiz));
    addAndMakeVisible (f);
    const int len = (int) s.len;
    f->setBounds (s.horiz ? Rectangle<int> (len, 40).withCentre ({ (int) s.x, (int) s.y })
                          : Rectangle<int> (40, len).withCentre ({ (int) s.x, (int) s.y }));
    f->setTooltip (String::fromUTF8 (s.tip));
    sliderAtt.add (new AudioProcessorValueTreeState::SliderAttachment (proc.apvts, s.id, *f));
    if (auto* p = proc.apvts.getParameter (s.id)) f->defaultValue = p->convertFrom0to1 (p->getDefaultValue());
}

//==============================================================================
void Panel::paint (Graphics& g)
{
    // ------------------------------------------------ mobile: legno ai lati, pannello, cassa
    g.fillAll (Colour (0xff141312));
    {
        // cassa in similpelle grigio scuro sotto la tastiera
        auto casing = Rectangle<float> (0, 640, (float) W, (float) H - 640);
        g.setGradientFill (ColourGradient (Colour (0xff2d2c2a), 0, casing.getY(), Colour (0xff121110), 0, casing.getBottom(), false));
        g.fillRect (casing);
        Random rnd (7);
        g.setColour (Colours::white.withAlpha (0.025f));
        for (int i = 0; i < 2600; ++i)
            g.fillRect ((float) rnd.nextInt (W), 640.0f + (float) rnd.nextInt (H - 640), 2.0f, 2.0f);
    }
    // pannello superiore
    {
        auto top = Rectangle<float> (40, 6, (float) W - 80, 636);
        g.setGradientFill (ColourGradient (kPanelHi, 0, top.getY(), kPanel, 0, top.getBottom(), false));
        g.fillRect (top);
        // grana leggera della vernice
        Random rnd (3);
        g.setColour (Colours::white.withAlpha (0.018f));
        for (int i = 0; i < 4000; ++i) g.fillRect (40.0f + (float) rnd.nextInt (W - 80), 6.0f + (float) rnd.nextInt (636), 1.5f, 1.5f);
        // bordo della cornice inferiore (gradino verso la tastiera)
        g.setGradientFill (ColourGradient (Colour (0xff1c1b18), 0, 615, Colour (0xff080808), 0, 642, false));
        g.fillRect (40.0f, 618.0f, (float) W - 80, 24.0f);
    }
    // guance in legno
    for (float x : { 2.0f, (float) W - 42.0f })
    {
        auto r = Rectangle<float> (x, 0, 40, 646);
        g.setGradientFill (ColourGradient (kWood.brighter (0.25f), r.getX(), 0, kWood.darker (0.45f), r.getRight(), 0, false));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (Colours::black.withAlpha (0.18f));
        for (float y = 8; y < 640; y += 11) g.drawLine (r.getX() + 6, y, r.getRight() - 8, y + 4, 0.8f);
    }

    // ------------------------------------------------ filetti delle sezioni
    const float rr = 18.0f;
    std::vector<Path> boxes;
    auto rect = [&] (float x0, float y0, float x1, float y1) { boxes.push_back (roundedPoly ({ { x0, y0 }, { x1, y0 }, { x1, y1 }, { x0, y1 } }, rr)); };
    rect (60, 26, 447, 524);                                   // LFO's MIXER
    rect (454, 26, 838, 148);                                  // LFO 1
    rect (846, 26, 1238, 148);                                 // LFO 2
    rect (1246, 26, 1432, 148);                                // LFO's DELAY
    boxes.push_back (roundedPoly ({ { 1440, 26 }, { 1808, 26 }, { 1808, 288 }, { 1672, 288 }, { 1672, 524 }, { 1552, 524 }, { 1552, 152 }, { 1440, 152 } }, rr)); // POLY
    rect (1436, 160, 1544, 524);                               // NOISE
    rect (1678, 296, 1806, 374);                               // HEAD / MAIN
    rect (1678, 382, 1806, 524);                               // VOLUME
    boxes.push_back (roundedPoly ({ { 454, 158 }, { 672, 158 }, { 672, 404 }, { 612, 404 }, { 612, 524 }, { 454, 524 } }, rr)); // OSC 1
    boxes.push_back (roundedPoly ({ { 680, 158 }, { 898, 158 }, { 898, 524 }, { 740, 524 }, { 740, 404 }, { 680, 404 } }, rr)); // OSC 2
    rect (620, 412, 732, 524);                                 // KEYBOARD CONTROL
    boxes.push_back (roundedPoly ({ { 906, 158 }, { 1238, 158 }, { 1238, 285 }, { 1160, 285 }, { 1160, 524 }, { 906, 524 } }, rr)); // VCF
    boxes.push_back (roundedPoly ({ { 1246, 158 }, { 1430, 158 }, { 1430, 524 }, { 1168, 524 }, { 1168, 293 }, { 1246, 293 } }, rr)); // VCA
    // pannelli della tastiera
    boxes.push_back (roundedPoly ({ { 30, 660 }, { 270, 660 }, { 270, 946 }, { 30, 946 } }, 14));
    boxes.push_back (roundedPoly ({ { 1700, 660 }, { 1852, 660 }, { 1852, 946 }, { 1700, 946 } }, 14));

    // fondo più scuro dei pannelli della tastiera
    g.setColour (Colour (0xff1d1c19));
    g.fillRoundedRectangle (24, 654, 252, 298, 16);
    g.fillRoundedRectangle (1694, 654, 164, 298, 16);
    g.setColour (kLine);
    for (auto& p : boxes) g.strokePath (p, PathStrokeType (2.2f));

    // righe divisorie del mixer
    g.setColour (kPrint.withAlpha (0.85f));
    for (float y : { 124.0f, 216.0f, 308.0f, 398.0f, 488.0f }) g.fillRect (64.0f, y, 380.0f, 2.2f);
    // divisori interni del pannello sinistro della tastiera (V rovesciata come l'originale)
    g.setColour (kLine);
    {
        Path v; v.startNewSubPath (40, 846); v.lineTo (150, 790); v.lineTo (260, 846);
        g.strokePath (v, PathStrokeType (2.0f));
        g.drawLine (150, 668, 150, 790, 2.0f);
    }

    // ------------------------------------------------ viti
    for (auto pt : { Point<float> (78, 78), Point<float> (78, 170), Point<float> (78, 262), Point<float> (78, 353), Point<float> (78, 443),
                     Point<float> (246, 78), Point<float> (246, 170), Point<float> (246, 262), Point<float> (246, 353), Point<float> (246, 443),
                     Point<float> (1487, 84), Point<float> (1648, 84), Point<float> (64, 562), Point<float> (1816, 562),
                     Point<float> (1826, 714), Point<float> (1826, 888) })
        screw (g, pt);
    for (float x : { 942.0f, 1000.0f, 1058.0f, 1114.0f, 1214.0f, 1270.0f, 1328.0f, 1384.0f })
    { screw (g, { x, 322 }, 5.5f); screw (g, { x, 494 }, 5.5f); }

    g.setColour (kPrint);

    // ------------------------------------------------ scale delle manopole
    for (auto& s : knobSpecs)
    {
        const Point<float> c (s.x, s.y);
        const float rn = 38.0f, nh = 11.0f;
        auto tick = [&] (float a, float r0, float r1, float w) { g.drawLine (Line<float> (polar (c, r0, a), polar (c, r1, a)), w); };
        if (s.scale == Scale::Ten)
        {
            for (int i = 0; i <= 10; ++i)
            {
                const float a = -135.0f + 27.0f * (float) i;
                tick (a, 27.0f, 31.0f, 1.6f);
                text (g, String (i), polar (c, rn, a), nh);
            }
        }
        else
        {
            const int n = s.scale == Scale::Bipolar4 ? 4 : 5;
            for (int i = -n; i <= n; ++i)
            {
                const float a = 135.0f * (float) i / (float) n;
                tick (a, 27.0f, 31.0f, 1.6f);
                if (i == 0) { g.fillRect (Rectangle<float> (12.0f, 5.0f).withCentre (polar (c, 34.0f, 0.0f))); continue; }
                if (s.scale == Scale::Db15)
                {
                    if (std::abs (i) == n) continue;
                    if (std::abs (i) == 2 || std::abs (i) == 4)
                        text (g, std::abs (i) == 2 ? "5" : "10", polar (c, rn + 3.0f, a), nh);
                }
                else if (std::abs (i) < n) text (g, String (std::abs (i)), polar (c, rn, a), nh);
            }
            if (s.scale == Scale::Db15)
            {
                text (g, "db15", c.translated (-36, 37), 9.5f); text (g, "15db", c.translated (36, 37), 9.5f);
                text (g, String::fromUTF8 ("−"), c.translated (-20, 44), 11.0f); text (g, "+", c.translated (20, 44), 11.0f);
            }
            else
            {
                text (g, String::fromUTF8 ("−"), polar (c, rn, -150.0f), 14.0f);
                text (g, "+", polar (c, rn, 150.0f), 14.0f);
            }
        }
        if (*s.label)
        {
            const auto lines = StringArray::fromLines (s.label);
            const float y0 = s.y + (s.scale == Scale::Db15 ? 56.0f : 52.0f);
            for (int i = 0; i < lines.size(); ++i) text (g, lines[i], { s.x, y0 + 14.0f * (float) i }, 11.5f);
        }
    }

    // ------------------------------------------------ selettori
    for (auto& s : selSpecs)
    {
        const Point<float> c (s.x, s.y);
        const int n = (int) s.labels.size();
        for (int i = 0; i < n; ++i)
        {
            const float a = s.a0 + (s.a1 - s.a0) * (float) i / (float) (n - 1);
            g.drawLine (Line<float> (polar (c, s.labelR - 13.0f, a), polar (c, s.labelR - 7.0f, a)), 1.6f);
            const auto pt = polar (c, s.labelR + 4.0f, a);
            const auto& lab = s.labels[(size_t) i];
            if (s.icons == 1 || s.icons == 2 || (s.icons == 3 && lab != "S&H"))
                icon (g, lab, pt.translated (-10.0f, 0.0f), 22.0f);
            else
            {
                const bool left = a < -10.0f, right = a > 10.0f;
                text (g, lab, pt.translated (left ? -2.0f : right ? 2.0f : 0.0f, 0.0f), 10.5f,
                      left ? Justification::centredRight : right ? Justification::centredLeft : Justification::centred, 80.0f);
            }
        }
    }

    // ------------------------------------------------ cursori: scale serigrafate
    auto hScale = [&] (float cx, float y, float len, bool pw)
    {
        const float x0 = cx - len * 0.5f + 7.0f, x1 = cx + len * 0.5f - 7.0f;
        // barre crescenti tra i triangoli 1-3-5-7
        const char* nums[] = { "1", "3", "5", "7" };
        for (int i = 0; i < 4; ++i)
        {
            const float x = x0 + (x1 - x0) * (0.12f + 0.25f * (float) i);
            Path tri; tri.addTriangle (x - 4.0f, y + 6.0f, x + 4.0f, y + 6.0f, x, y - 1.0f);
            g.fillPath (tri);
            if (! pw) text (g, nums[i], { x, y + 14.0f }, 9.0f);
            const float bx = x + (x1 - x0) * 0.125f;
            if (i < 3) g.fillRect (Rectangle<float> (7.0f, 8.0f + 3.0f * (float) i).withCentre ({ bx, y + 9.0f }));
        }
        g.fillRect (x0 - 2.0f, y - 2.0f, 2.0f, 22.0f);
        g.fillRect (x1, y - 2.0f, 4.5f, 22.0f);
        if (pw)
        {
            // estremi della corsa sopra le viti, lontani dal filetto della sezione
            text (g, "50 %", { x0 - 6.0f, y - 52.0f }, 10.0f);
            text (g, "5 %", { x1 + 12.0f, y - 52.0f }, 10.0f);
        }
    };
    for (float y : { 78.0f, 170.0f, 262.0f, 353.0f }) hScale (162, y + 26, 150, false);
    hScale (162, 443 + 26, 150, true);
    hScale (1568, 84 + 24, 150, false);

    auto adsrScale = [&] (float x0, float x1)
    {
        // etichette ◁7▷ ◁5▷ ◁3▷ ◁1▷ tra i cursori, con le barre orizzontali
        const char* lv[] = { "7", "5", "3", "1" };
        for (int col = 0; col < 3; ++col)
        {
            const float cx = x0 + (x1 - x0) * ((float) col + 0.5f) / 3.0f;
            for (int i = 0; i < 4; ++i)
            {
                const float y = 357.0f + 33.0f * (float) i;
                g.fillRect (Rectangle<float> (40.0f, 5.0f).withCentre ({ cx, y - 9.0f }));
                Path l, r;
                l.addTriangle (cx - 15, y + 4, cx - 9, y + 0.5f, cx - 9, y + 7.5f);
                r.addTriangle (cx + 15, y + 4, cx + 9, y + 0.5f, cx + 9, y + 7.5f);
                g.fillPath (l); g.fillPath (r);
                text (g, lv[i], { cx, y + 4.0f }, 9.0f);
            }
        }
    };
    adsrScale (942, 1114);
    adsrScale (1214, 1384);
    for (auto [x, l] : { std::pair<float, const char*> { 942, "A" }, { 1000, "D" }, { 1058, "S" }, { 1114, "R" },
                         { 1214, "A" }, { 1270, "D" }, { 1328, "S" }, { 1384, "R" } })
        text (g, l, { x, 306 }, 12.0f);

    // ------------------------------------------------ deviatori
    text (g, "IN", { 668, 108 }, 10.5f);
    text (g, "DELAY", { 632, 138 }, 11.0f);
    text (g, "IN", { 1076, 108 }, 10.5f);
    text (g, "DELAY", { 1040, 138 }, 11.0f);
    text (g, "IN", { 708, 448 }, 10.5f);
    text (g, "KEYBOARD", { 676, 486 }, 9.5f);
    text (g, "CONTROL", { 676, 500 }, 9.5f);
    text (g, "IN", { 1187, 306 }, 10.5f);   // come sul DS-2: dentro il VCA, a sinistra della A
    text (g, "HEAD.", { 1706, 356 }, 10.0f);
    text (g, "MAIN", { 1768, 356 }, 10.0f);

    // ------------------------------------------------ titoli e scritte
    title (g, "LFO 1", { 622, 46 }, 17.0f);
    title (g, "LFO 2", { 1040, 46 }, 17.0f);
    text (g, "LFO's", { 1300, 134 }, 11.0f); text (g, "DELAY", { 1378, 134 }, 11.0f);
    text (g, "LFO   AMOUNT", { 1568, 138 }, 11.0f);
    title (g, "LFO's  MIXER", { 254, 507 }, 18.0f);
    text (g, "OSC. 1", { 296, 82 }, 11.5f);
    text (g, "OSC. 2", { 378, 172 }, 11.5f);
    text (g, "VCF", { 300, 266 }, 11.5f);
    text (g, "VCA", { 380, 355 }, 11.5f);
    text (g, "PULSE", { 306, 438 }, 11.0f); text (g, "WIDTH", { 306, 452 }, 11.0f);
    text (g, "DIGITAL", { 540, 436 }, 11.0f); text (g, "CONTROLLED", { 540, 452 }, 11.0f);
    text (g, "DIGITAL", { 812, 436 }, 11.0f); text (g, "CONTROLLED", { 812, 452 }, 11.0f);
    title (g, "OSC. 1", { 540, 503 }, 20.0f);
    title (g, "OSC. 2", { 812, 503 }, 20.0f);
    title (g, "VCF", { 1034, 503 }, 24.0f);
    title (g, "VCA", { 1298, 503 }, 24.0f);
    title (g, "NOISE", { 1490, 503 }, 19.0f);
    title (g, "POLY", { 1612, 503 }, 20.0f);
    title (g, "VOLUME", { 1742, 503 }, 18.0f);
    text (g, "OCTAVE", { 92, 768 }, 10.5f); text (g, "POLY", { 92, 782 }, 10.5f);
    text (g, "PITCH   BENDING", { 150, 932 }, 10.5f);
    text (g, String::fromUTF8 ("−"), { 68, 870 }, 14.0f); text (g, "+", { 232, 870 }, 14.0f); text (g, "0", { 150, 913 }, 10.0f);
    {
        Path arc; arc.addCentredArc (150, 1000, 112, 112, 0.0f, degToRad (-38.0f), degToRad (38.0f), true);
        g.strokePath (arc, PathStrokeType (1.6f));
    }
    text (g, "GLIDE", { 1776, 690 }, 10.5f);
    text (g, "IN", { 1716, 752 }, 10.0f); text (g, "IN", { 1716, 862 }, 10.0f);
    text (g, "SPEED", { 1776, 916 }, 10.0f); text (g, "RANGE", { 1776, 930 }, 10.0f);
    {
        // frecce del pannello del glide
        Path a; a.startNewSubPath (1800, 690); a.lineTo (1812, 690); a.startNewSubPath (1740, 690); a.lineTo (1728, 690); a.lineTo (1728, 728);
        a.startNewSubPath (1728, 884); a.lineTo (1728, 922); a.lineTo (1745, 922);
        g.strokePath (a, PathStrokeType (1.6f));
        Path h1; h1.addTriangle (1723, 724, 1733, 724, 1728, 734); g.fillPath (h1);
        Path h2; h2.addTriangle (1723, 888, 1733, 888, 1728, 878); g.fillPath (h2);
        // "SPEED GLIDE" in verticale accanto allo slider
        GlyphArrangement ga;
        ga.addLineOfText (Fonts::get().label (10.0f), "SPEED  GLIDE", 0, 0);
        Path p; ga.createPath (p);
        const auto bb = p.getBounds();
        p.applyTransform (AffineTransform::translation (-bb.getCentreX(), -bb.getCentreY()).rotated (MathConstants<float>::halfPi).translated (1793, 800));
        g.fillPath (p);
    }

    // ------------------------------------------------ sezione EXT. INPUT (aggiunta di DISSONA 2)
    {
        g.setColour (kLine);
        g.drawRoundedRectangle (Rectangle<float> (700, 530, 260, 92), 14.0f, 2.2f);
        g.setColour (kPrint);
        text (g, "EXT. INPUT", { 830, 548 }, 11.0f);
        text (g, "GATE", { 846, 584 }, 9.5f);
        text (g, "OPEN", { 922, 584 }, 9.5f);
        text (g, "VCA", { 884, 608 }, 9.0f);
        text (g, "LEVEL", { 744, 608 }, 9.0f);
    }

    // ------------------------------------------------ targa DISSONA 2 (stile CRUMAR: lettere slab in caselle inclinate)
    {
        const float bw = 54.0f, bh = 56.0f, gap = 7.0f, sl = 0.14f;
        const float y = logoArea.getY();
        float x = logoArea.getX();
        const char* letters[] = { "D", "I", "S", "S", "O", "N", "A", "2" };
        for (int i = 0; i < 8; ++i)
        {
            if (i == 7) x += 12.0f;
            const bool accent = (i == 0 || i == 2);           // la D e la S del nome originale
            const Rectangle<float> cell (x, y, bw, bh);
            const auto c = cell.getCentre();
            const auto italic = AffineTransform::translation (-c.x, -c.y).sheared (-sl, 0.0f).translated (c.x, c.y);
            Path box; box.addRoundedRectangle (cell.reduced (1.5f), 9.0f);
            box.applyTransform (italic);
            g.setColour (Colours::black.withAlpha (0.35f)); g.fillPath (box, AffineTransform::translation (1.5f, 2.5f));
            g.setColour (Colour (0xff1e1c18)); g.fillPath (box);
            g.setColour (accent ? kLine.brighter (0.15f) : kPrint);
            g.strokePath (box, PathStrokeType (accent ? 3.6f : 2.2f));

            GlyphArrangement ga;
            ga.addLineOfText (Fonts::get().logo (100.0f), letters[i], 0, 0);
            Path p; ga.createPath (p);
            const auto bb = p.getBounds();
            const float sc = (bh * 0.66f) / bb.getHeight();
            p.applyTransform (AffineTransform::translation (-bb.getCentreX(), -bb.getCentreY()).scaled (sc).translated (c).followedBy (italic));
            g.setColour (accent || i == 7 ? kPrint : kPrint.withMultipliedBrightness (0.74f));
            g.fillPath (p);
            x += bw + gap;
        }
        g.setColour (kPrint);
        digital (g, "DIGITAL  SYNTHESIZER", { x - gap, y + bh + 22.0f }, 13.0f, true);
        digital (g, "COLLATERAL  EFFECTS", { logoArea.getX() + 4.0f, y + bh + 22.0f }, 9.0f, false);
    }
}

void Panel::paintOverChildren (Graphics& g)
{
    // LED degli LFO
    auto drawLed = [&] (Point<float> c, float level)
    {
        const float r = 7.0f;
        g.setColour (Colour (0xff120000)); g.fillEllipse (c.x - r - 2, c.y - r - 2, (r + 2) * 2, (r + 2) * 2);
        const Colour on (0xffff2a1a), off (0xff3e0b07);
        g.setColour (off.interpolatedWith (on, level)); g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
        if (level > 0.05f)
        {
            g.setGradientFill (ColourGradient (on.withAlpha (0.5f * level), c.x, c.y, on.withAlpha (0.0f), c.x + r * 3.2f, c.y, true));
            g.fillEllipse (c.x - r * 3.2f, c.y - r * 3.2f, r * 6.4f, r * 6.4f);
        }
        g.setColour (Colours::white.withAlpha (0.4f)); g.fillEllipse (c.x - r * 0.45f, c.y - r * 0.65f, r * 0.6f, r * 0.45f);
    };
    drawLed ({ 632, 72 }, led1);
    drawLed ({ 1040, 72 }, led2);
}

void Panel::timerCallback()
{
    const float l1 = proc.lfoLed (1), l2 = proc.lfoLed (2);
    if (l1 != led1) { led1 = l1; repaint (Rectangle<int> (60, 60).withCentre ({ 632, 72 })); }
    if (l2 != led2) { led2 = l2; repaint (Rectangle<int> (60, 60).withCentre ({ 1040, 72 })); }
    lever.refresh();
    presetBar.refresh();
    if (bubble.isVisible() && bubbleParam.isNotEmpty())
        if (auto* p = proc.apvts.getParameter (bubbleParam))
            if (const auto v = p->getCurrentValueAsText(); v != bubble.value) { bubble.value = v; bubble.repaint(); }
    if (learnId.isNotEmpty() && proc.learningIndex() < 0) { learnId.clear(); repaintControls(); }
}

void Panel::showValue (Component& c, const String& id, bool show)
{
    if (! show)
    {
        if (bubbleFor == &c) { bubble.setVisible (false); bubbleFor = nullptr; bubbleParam.clear(); }
        return;
    }
    auto* p = proc.apvts.getParameter (id);
    if (p == nullptr) return;
    bubbleFor = &c;
    bubbleParam = id;
    bubble.title = p->getName (40).toUpperCase();
    bubble.value = p->getCurrentValueAsText();
    placeBubble();
    bubble.setVisible (true);
    bubble.toFront (false);
    bubble.repaint();
}

void Panel::placeBubble()
{
    if (bubbleFor == nullptr) return;
    const auto r = bubbleFor->getBounds();
    auto b = bubble.getBounds().withCentre ({ r.getCentreX(), r.getY() - bubble.getHeight() / 2 - 6 });
    if (b.getY() < 4) b.setY (r.getBottom() + 6);
    b.setX (jlimit (4, W - b.getWidth() - 4, b.getX()));
    bubble.setBounds (b);
}

void Panel::repaintControls()
{
    for (auto* k : knobs) k->repaint();
    for (auto* t : toggles) t->repaint();
    for (auto* f : faders) f->repaint();
    lever.repaint();
}

bool Panel::isLearning (const String& id) const
{
    const int li = proc.learningIndex();
    return li >= 0 && li == proc.paramIndexOf (id);
}

void Panel::showControlMenu (Component& c, const String& id)
{
    const int idx = proc.paramIndexOf (id);
    const int cc = proc.ccForParam (idx);
    PopupMenu m;
    m.addSectionHeader (proc.getParameters()[idx]->getName (64) + (cc >= 0 ? "  (CC " + String (cc) + ")" : String()));
    m.addItem (1, "MIDI Learn: muovi un controllo della tastiera MIDI");
    m.addItem (2, "Rimuovi assegnazione MIDI", cc >= 0);
    if (proc.learningIndex() >= 0) m.addItem (3, "Annulla MIDI Learn");
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&c), [this, idx, id] (int r)
    {
        if (r == 1) { proc.startLearn (idx); learnId = id; }
        else if (r == 2) proc.clearMapping (idx);
        else if (r == 3) proc.cancelLearn();
        repaintControls();
    });
}

void Panel::mouseDown (const MouseEvent& e)
{
    if (logoArea.expanded (8.0f).contains (e.position)) showSettingsMenu();
}

void Panel::showSettingsMenu()
{
    auto& s = proc.apvts;
    auto val = [&s] (const char* id) { return s.getRawParameterValue (id)->load(); };
    auto set = [&s] (const char* id, float plain)
    {
        auto* p = s.getParameter (id);
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
    };

    PopupMenu bm;
    for (int st : { 0, 1, 2, 3, 5, 7, 12 })
        bm.addItem (100 + st, st == 0 ? String ("Spento") : String (st) + (st == 3 ? " semitoni (come la leva originale)" : " semitoni"), true, (int) val (pid::bendRange) == st);
    PopupMenu km;
    km.addItem (200, "Completa (tutte le note MIDI)", true, val (pid::keyRange) < 0.5f);
    km.addItem (201, String::fromUTF8 ("44 tasti FA–DO (MIDI 41–84) come l'originale"), true, val (pid::keyRange) > 0.5f);
    PopupMenu tm;
    tm.addItem (300, "Singolo (come l'originale: il legato non riattacca)", true, val (pid::retrigger) < 0.5f);
    tm.addItem (301, "Multiplo (ogni nuova nota riattacca gli inviluppi)", true, val (pid::retrigger) > 0.5f);
    PopupMenu vm;
    vm.addItem (400, "Manuale Crumar: IN = l'ADSR del VCF comanda anche il VCA", true, val (pid::vcaSwMode) < 0.5f);
    vm.addItem (401, "Macchina di E. Bassi: IN = ADSR del VCA, OUT = solo gate", true, val (pid::vcaSwMode) > 0.5f);
    PopupMenu om;
    om.addItem (500, "Tutto insieme sull'uscita principale", true, val (pid::polySplit) < 0.5f);
    om.addItem (501, "POLY separata: principale a sinistra, POLY a destra (stereo)", true, val (pid::polySplit) > 0.5f);
    PopupMenu qm;
    qm.addItem (600, "Normale (meno CPU)", true, val (pid::quality) < 0.5f);
    qm.addItem (601, "Alta (sovracampionamento doppio)", true, val (pid::quality) > 0.5f);
    PopupMenu pm;
    pm.addItem (800, String::fromUTF8 ("Più acuta (predefinita)"), true, val (pid::priority) < 0.5f);
    pm.addItem (801, String::fromUTF8 ("Più grave"), true, std::lround (val (pid::priority)) == 1);
    pm.addItem (802, "Ultima suonata", true, std::lround (val (pid::priority)) == 2);
    PopupMenu am;
    const float a4 = val (pid::tune);
    static const float tunes[] = { 430, 432, 435, 438, 439, 440, 441, 442, 443, 444, 445, 450 };
    for (int i = 0; i < 12; ++i)
        am.addItem (900 + i, String (tunes[i], 0) + " Hz" + (tunes[i] == 440.0f ? " (standard)" : ""), true, std::abs (a4 - tunes[i]) < 0.05f);
    am.addSeparator();
    am.addItem (950, String::fromUTF8 ("Più alto di 1 cent"));
    am.addItem (951, String::fromUTF8 ("Più basso di 1 cent"));
    PopupMenu tol;
    tol.addItem (1000, "Attive (questo esemplare)", true, val (pid::tolerances) > 0.5f);
    tol.addItem (1001, "Spente (strumento ideale)", true, val (pid::tolerances) < 0.5f);
    tol.addSeparator();
    tol.addItem (1002, "Nuovo esemplare (altre tolleranze)");
    PopupMenu pd;
    static const char* pedals[] = { "Spento", "2 ottave", "4 ottave", "6 ottave" };
    for (int i = 0; i < 4; ++i) pd.addItem (1100 + i, pedals[i], true, std::lround (val (pid::pedalRange)) == i);

    PopupMenu m;
    m.addSectionHeader ("DISSONA 2  -  Collateral Effects");
    m.addSubMenu ("Pitch bend MIDI", bm);
    m.addSubMenu ("Estensione della tastiera", km);
    m.addSubMenu ("Trigger degli inviluppi", tm);
    m.addSubMenu (String::fromUTF8 ("Priorità delle note MONO"), pm);
    m.addSubMenu ("Accordatura: La = " + String (a4, 1) + " Hz", am);
    m.addSubMenu ("Tolleranze analogiche", tol);
    m.addSubMenu ("Pedale del filtro (CC 11)", pd);
    m.addSubMenu ("Deviatore VCF / VCA", vm);
    m.addSubMenu ("Uscite", om);
    m.addSubMenu ("Qualita audio", qm);
    m.addSeparator();
    m.addItem (700, "Ripristina le assegnazioni MIDI predefinite");
    m.addItem (701, "Annulla MIDI Learn", proc.learningIndex() >= 0);
    m.showMenuAsync (PopupMenu::Options(), [this, set] (int r)
    {
        if (r >= 100 && r <= 112) set (pid::bendRange, (float) (r - 100));
        else if (r == 200 || r == 201) set (pid::keyRange, (float) (r - 200));
        else if (r == 300 || r == 301) set (pid::retrigger, (float) (r - 300));
        else if (r == 400 || r == 401) set (pid::vcaSwMode, (float) (r - 400));
        else if (r == 500 || r == 501) set (pid::polySplit, (float) (r - 500));
        else if (r == 600 || r == 601) set (pid::quality, (float) (r - 600));
        else if (r >= 800 && r <= 802) set (pid::priority, (float) (r - 800));
        else if (r >= 900 && r < 912) { static const float t[] = { 430, 432, 435, 438, 439, 440, 441, 442, 443, 444, 445, 450 }; set (pid::tune, t[r - 900]); }
        else if (r == 950 || r == 951)
        {
            const float cur = proc.apvts.getRawParameterValue (pid::tune)->load();
            set (pid::tune, jlimit (430.0f, 450.0f, cur * std::pow (2.0f, (r == 950 ? 1.0f : -1.0f) / 1200.0f)));
        }
        else if (r == 1000 || r == 1001) set (pid::tolerances, r == 1000 ? 1.0f : 0.0f);
        else if (r == 1002) proc.newInstanceSeed();
        else if (r >= 1100 && r <= 1103) set (pid::pedalRange, (float) (r - 1100));
        else if (r == 700) proc.resetDefaultMapping();
        else if (r == 701) proc.cancelLearn();
        repaintControls();
    });
}

//==============================================================================
DsEditor::DsEditor (DsProcessor& p) : AudioProcessorEditor (p), proc (p), panel (p)
{
    addAndMakeVisible (panel);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) Panel::W / (double) Panel::H);
    setResizeLimits (752, 392, 2632, 1372);
    setSize (1316, 686);
}

void DsEditor::resized()
{
    const float s = (float) getWidth() / (float) Panel::W;
    panel.setBounds (0, 0, Panel::W, Panel::H);
    panel.setTransform (AffineTransform::scale (s));
}
