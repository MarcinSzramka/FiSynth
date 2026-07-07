#include "EnvelopeEditor.h"

namespace
{
    constexpr float hitRadius   = 9.0f;
    constexpr float pointRadius = 5.0f;
    constexpr float margin      = 10.0f;
    constexpr float bottomPad   = 4.0f;
}

EnvelopeEditor::EnvelopeEditor (FiSynthAudioProcessor& p)
    : processor (p)
{
    for (int i = 0; i < FiSynthAudioProcessor::kMaxPlayheads; ++i)
    {
        playheadTime[i] = -1.0f;
        playheadNote[i] = -1;
    }

    updateView();
    startTimerHz (60);
}

EnvelopeEditor::~EnvelopeEditor()
{
    stopTimer();
}

void EnvelopeEditor::setActiveEnvelope (int idx)
{
    idx = juce::jlimit (0, kNumEnvelopes - 1, idx);
    if (idx == activeEnv)
        return;

    activeEnv     = idx;
    draggingPoint = -1;
    draggingCurve = -1;
    updateView();
    repaint();
}

juce::Colour EnvelopeEditor::envelopeColour (int idx)
{
    switch (idx)
    {
        case 0:  return juce::Colour (0xff31c75a);  // Amp  — zielony
        case 1:  return juce::Colour (0xff4da6ff);  // Env1 — niebieski
        case 2:  return juce::Colour (0xffffc24d);  // Env2 — żółty
        case 3:  return juce::Colour (0xffb36bff);  // Env3 — fioletowy
        default: return juce::Colours::white;
    }
}

juce::Colour EnvelopeEditor::playheadColour (int idx)
{
    // Sześć dobrze rozróżnialnych odcieni, równomiernie po kole barw.
    static const juce::Colour palette[] = {
        juce::Colour (0xffff5252),  // czerwony
        juce::Colour (0xffffa040),  // pomarańczowy
        juce::Colour (0xff63d24d),  // zielony
        juce::Colour (0xff32d6d6),  // cyjan
        juce::Colour (0xff5c8cff),  // niebieski
        juce::Colour (0xffd66bff),  // fioletowy
    };
    constexpr int n = (int) (sizeof (palette) / sizeof (palette[0]));
    return palette[((idx % n) + n) % n];
}

float EnvelopeEditor::snapTimeToGrid (float t) const
{
    if (! processor.isEnvSync() || ! processor.isEnvSnap())
        return t;

    const float step = processor.gridDivisionBeats();
    if (step <= 0.0f)
        return t;

    return (float) juce::roundToInt (t / step) * step;
}

void EnvelopeEditor::updateView()
{
    // Wykres mieści najdłuższą z wszystkich obwiedni, żeby każda była widoczna.
    float last = 0.2f;
    for (int e = 0; e < kNumEnvelopes; ++e)
    {
        const auto& pts = processor.getEnvelopeModel (e).points;
        if (! pts.empty())
            last = juce::jmax (last, pts.back().time);
    }
    viewLength = juce::jmax (0.2f, last * 1.15f);
}

juce::Rectangle<float> EnvelopeEditor::plotBounds() const
{
    return getLocalBounds().toFloat()
        .reduced (margin)
        .withTrimmedBottom (bottomPad);
}

float EnvelopeEditor::timeToX (float t) const
{
    auto b = plotBounds();
    return b.getX() + (t / juce::jmax (1.0e-3f, viewLength)) * b.getWidth();
}

float EnvelopeEditor::levelToY (float l) const
{
    auto b = plotBounds();
    return b.getBottom() - juce::jlimit (0.0f, 1.0f, l) * b.getHeight();
}

float EnvelopeEditor::xToTime (float x) const
{
    auto b = plotBounds();
    return ((x - b.getX()) / juce::jmax (1.0f, b.getWidth())) * viewLength;
}

float EnvelopeEditor::yToLevel (float y) const
{
    auto b = plotBounds();
    return juce::jlimit (0.0f, 1.0f, (b.getBottom() - y) / juce::jmax (1.0f, b.getHeight()));
}

int EnvelopeEditor::findPointNear (juce::Point<float> pos, float radius) const
{
    const auto& pts = activeModel().points;
    int   best = -1;
    float bestDist = radius;

    for (int i = 0; i < (int) pts.size(); ++i)
    {
        juce::Point<float> sp (timeToX (pts[(size_t) i].time), levelToY (pts[(size_t) i].level));
        const float d = sp.getDistanceFrom (pos);
        if (d < bestDist)
        {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

int EnvelopeEditor::findCurveHandleNear (juce::Point<float> pos, float radius) const
{
    const auto& pts = activeModel().points;
    int   best = -1;
    float bestDist = radius;

    for (int i = 0; i + 1 < (int) pts.size(); ++i)
    {
        const float midT = (pts[(size_t) i].time + pts[(size_t) (i + 1)].time) * 0.5f;
        const float Li   = pts[(size_t) i].level;
        const float Lj   = pts[(size_t) (i + 1)].level;
        const float midV = Li + (Lj - Li) * fiEnvShape (0.5f, pts[(size_t) (i + 1)].curve);

        juce::Point<float> hp (timeToX (midT), levelToY (midV));
        const float d = hp.getDistanceFrom (pos);
        if (d < bestDist)
        {
            bestDist = d;
            best = i;
        }
    }
    return best;
}

void EnvelopeEditor::rebuildBackgroundCache (float scale)
{
    const auto area = getLocalBounds();
    const int  w = juce::jmax (1, juce::roundToInt ((float) area.getWidth()  * scale));
    const int  h = juce::jmax (1, juce::roundToInt ((float) area.getHeight() * scale));

    bgCache = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (bgCache);

    // Rysujemy we współrzędnych logicznych; transformacja przenosi je na
    // fizyczne piksele obrazka, więc na HiDPI krzywe zostają ostre.
    g.addTransform (juce::AffineTransform::scale (scale));

    auto b = plotBounds();

    // Tło — ciepła czerń w rodzinie palety FiLook (kolor niesie KRZYWA, nie tło).
    g.fillAll (juce::Colour (0xff12100a));
    g.setColour (juce::Colour (0xff17140c));
    g.fillRect (b);

    // Siatka pozioma (0, 0.25, 0.5, 0.75, 1.0)
    g.setColour (juce::Colour (0x22ffffff));
    for (int i = 0; i <= 4; ++i)
    {
        const float y = b.getY() + b.getHeight() * (float) i / 4.0f;
        g.drawHorizontalLine ((int) y, b.getX(), b.getRight());
    }

    // Siatka pionowa tempa (gdy sync wł.): linie co podział, pełne beaty jaśniej.
    if (processor.isEnvSync())
    {
        const float step = processor.gridDivisionBeats();
        if (step > 0.0f)
        {
            for (float t = 0.0f; t <= viewLength + 1.0e-4f; t += step)
            {
                const bool onBeat = std::abs (t - std::round (t)) < 1.0e-3f;
                g.setColour (juce::Colour (onBeat ? 0x55ffffff : 0x1effffff));
                g.drawVerticalLine ((int) timeToX (t), b.getY(), b.getBottom());
            }
        }
    }

    // Rysowanie pojedynczej obwiedni. Aktywna: pełny kolor, wypełnienie,
    // linia sustain i edytowalne punkty. Nieaktywna: cienka, wyblakła linia.
    const int steps = juce::jmax (64, (int) b.getWidth());
    auto drawEnvelope = [&] (const EnvSnapshot& snap, juce::Colour col, bool active)
    {
        if (snap.numPoints < 2)
            return;

        const float total = snap.totalLength();

        juce::Path curve;
        curve.startNewSubPath (timeToX (0.0f), levelToY (snap.levels[0]));
        for (int i = 1; i <= steps; ++i)
        {
            const float t = total * (float) i / (float) steps;
            const float v = fiEnvEvalRange (snap, 0, snap.numPoints - 1, t);
            curve.lineTo (timeToX (t), levelToY (v));
        }

        if (! active)
        {
            g.setColour (col.withAlpha (0.30f));
            g.strokePath (curve, juce::PathStrokeType (1.2f));
            return;
        }

        // Wypełnienie pod aktywną krzywą
        juce::Path fill = curve;
        fill.lineTo (timeToX (total), b.getBottom());
        fill.lineTo (timeToX (0.0f), b.getBottom());
        fill.closeSubPath();
        g.setColour (col.withAlpha (0.20f));
        g.fillPath (fill);

        g.setColour (col);
        g.strokePath (curve, juce::PathStrokeType (2.0f));

        // Linia sustain
        {
            const float sx = timeToX (snap.times[snap.sustainIndex]);
            g.setColour (juce::Colour (0xffffc24d));
            const float dashes[] = { 4.0f, 3.0f };
            juce::Line<float> line (sx, b.getY(), sx, b.getBottom());
            g.drawDashedLine (line, dashes, 2, 1.2f);
        }

        // Uchwyty krzywizny — środek każdego segmentu, na krzywej. Przeciągnięcie
        // w pionie wygina segment; dwuklik resetuje do liniowego.
        for (int i = 0; i + 1 < snap.numPoints; ++i)
        {
            const float midT = (snap.times[i] + snap.times[i + 1]) * 0.5f;
            const float Li   = snap.levels[i];
            const float Lj   = snap.levels[i + 1];
            const float midV = Li + (Lj - Li) * fiEnvShape (0.5f, snap.curves[i + 1]);
            const float hx   = timeToX (midT);
            const float hy   = levelToY (midV);

            g.setColour (col.withAlpha (0.55f));
            g.drawEllipse (hx - 3.0f, hy - 3.0f, 6.0f, 6.0f, 1.3f);
        }

        // Punkty (tylko aktywna obwiednia jest edytowalna)
        for (int i = 0; i < snap.numPoints; ++i)
        {
            const float x = timeToX (snap.times[i]);
            const float y = levelToY (snap.levels[i]);
            const bool isSustain = (i == snap.sustainIndex);

            g.setColour (isSustain ? juce::Colour (0xffffc24d) : juce::Colours::white);
            g.fillEllipse (x - pointRadius, y - pointRadius, pointRadius * 2.0f, pointRadius * 2.0f);
            g.setColour (juce::Colour (0xff17140c));   // obwódka = kolor tła wykresu
            g.drawEllipse (x - pointRadius, y - pointRadius, pointRadius * 2.0f, pointRadius * 2.0f, 1.5f);
        }
    };

    // Najpierw nieaktywne w tle, potem aktywna na wierzchu.
    for (int e = 0; e < kNumEnvelopes; ++e)
        if (e != activeEnv)
            drawEnvelope (processor.getEnvelopeModel (e).makeSnapshot(), envelopeColour (e), false);

    drawEnvelope (activeModel().makeSnapshot(), envelopeColour (activeEnv), true);

    // Podpowiedź
    g.setColour (juce::Colour (0x99ffffff));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (juce::String (juce::CharPointer_UTF8 (
                    "dwuklik: dodaj/usu\xc5\x84 punkt   |   Shift+klik: sustain   |   przeci\xc4\x85gnij uchwyt: krzywizna")),
                getLocalBounds().removeFromBottom (14).reduced (4, 0),
                juce::Justification::centredRight);
}

void EnvelopeEditor::paint (juce::Graphics& g)
{
    if (draggingPoint < 0)
        updateView();

    // Statyka (siatka + krzywe + podpowiedź) z cache — przy samym ruchu
    // playheada (timer 60 Hz) robimy blit zamiast strokowania wszystkich ścieżek.
    const float scale     = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int   editCount = processor.envEditCount.load();
    const bool  sync      = processor.isEnvSync();
    const float grid      = processor.gridDivisionBeats();

    if (bgCache.isNull()
        || cacheBounds != getLocalBounds()
        || cacheEditCount != editCount
        || cacheActiveEnv != activeEnv
        || cacheSync != sync
        || ! juce::exactlyEqual (cacheScale, scale)
        || ! juce::exactlyEqual (cacheGrid, grid)
        || ! juce::exactlyEqual (cacheViewLength, viewLength))
    {
        rebuildBackgroundCache (scale);
        cacheBounds     = getLocalBounds();
        cacheEditCount  = editCount;
        cacheActiveEnv  = activeEnv;
        cacheSync       = sync;
        cacheScale      = scale;
        cacheGrid       = grid;
        cacheViewLength = viewLength;
    }

    g.drawImageTransformed (bgCache, juce::AffineTransform::scale (1.0f / scale));

    auto b = plotBounds();
    const EnvSnapshot activeSnap = activeModel().makeSnapshot();

    // Playheady aktywnej obwiedni (czas rzeczywisty) — po jednym na grający głos,
    // każdy w swoim kolorze. Mapowane przez timeToX, więc kropka leży na krzywej.
    {
        float legendY = b.getY() + 4.0f;
        for (int i = 0; i < FiSynthAudioProcessor::kMaxPlayheads; ++i)
        {
            if (playheadTime[i] < 0.0f)
                continue;

            const juce::Colour col = playheadColour (i);
            const float x = juce::jlimit (b.getX(), b.getRight(), timeToX (playheadTime[i]));
            g.setColour (col);
            g.drawLine (x, b.getY(), x, b.getBottom(), 1.5f);

            // Y liczone z krzywej w tym czasie — kropka zawsze leży na obwiedni.
            const float yVal = (activeSnap.numPoints >= 2)
                ? fiEnvEvalRange (activeSnap, 0, activeSnap.numPoints - 1, playheadTime[i])
                : 0.0f;
            const float y = levelToY (yVal);
            g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);

            // Legenda: próbka koloru + nazwa nuty, w prawym górnym rogu wykresu.
            const juce::String name = (playheadNote[i] >= 0)
                ? juce::MidiMessage::getMidiNoteName (playheadNote[i], true, true, 4)
                : juce::String ("--");

            g.setFont (juce::Font (juce::FontOptions (11.0f)));
            const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), name);
            const float pad = 5.0f, sw = 9.0f, gap = 4.0f, rh = 14.0f;
            const float bw = pad + sw + gap + tw + pad;
            const float bx = b.getRight() - bw - 4.0f;

            g.setColour (juce::Colour (0xcc12100a));   // plakietka w tle palety FiLook
            g.fillRoundedRectangle (bx, legendY, bw, rh, 3.0f);
            g.setColour (col);
            g.fillRoundedRectangle (bx + pad, legendY + (rh - sw) * 0.5f, sw, sw, 2.0f);
            g.setColour (juce::Colours::white);
            g.drawText (name, juce::Rectangle<float> (bx + pad + sw + gap, legendY, tw + 2.0f, rh),
                        juce::Justification::centredLeft);

            legendY += rh + 3.0f;
        }
    }
}

void EnvelopeEditor::resized()
{
    updateView();
}

void EnvelopeEditor::mouseDown (const juce::MouseEvent& e)
{
    auto& model = activeModel();
    const int idx = findPointNear (e.position, hitRadius);

    // Shift + klik na punkcie => ustaw jako sustain
    if (e.mods.isShiftDown() && idx >= 0)
    {
        model.sustainIndex = idx;
        processor.commitEnvelope (activeEnv);
        repaint();
        return;
    }

    if (idx >= 0)
    {
        draggingPoint = idx;
        return;
    }

    // Brak punktu pod kursorem — może uchwyt krzywizny segmentu.
    draggingCurve = findCurveHandleNear (e.position, hitRadius);
    draggingPoint = -1;
}

void EnvelopeEditor::mouseDrag (const juce::MouseEvent& e)
{
    // Wyginanie segmentu: pozycję pionową kursora przeliczamy na krzywiznę.
    if (draggingCurve >= 0)
    {
        auto& pts = activeModel().points;
        const int i = draggingCurve;
        if (i + 1 >= (int) pts.size())
            return;

        const float Li   = pts[(size_t) i].level;
        const float Lj   = pts[(size_t) (i + 1)].level;
        const float span = Lj - Li;

        // Płaski segment — krzywizna i tak niewidoczna, nie ma czego wyginać.
        if (std::abs (span) > 1.0e-3f)
        {
            const float v = yToLevel (e.position.y);
            // g = ułamek wzdłuż poziomu w punkcie f=0.5; odwracamy fiEnvShape:
            // fiEnvShape(0.5,c) = 1/(e^{3c}+1) => c = ln((1-g)/g)/3.
            const float g = juce::jlimit (0.02f, 0.98f, (v - Li) / span);
            float c = std::log ((1.0f - g) / g) / 3.0f;
            pts[(size_t) (i + 1)].curve = juce::jlimit (-1.0f, 1.0f, c);

            processor.commitEnvelope (activeEnv);
            repaint();
        }
        return;
    }

    if (draggingPoint < 0)
        return;

    auto& model = activeModel();
    auto& pts   = model.points;
    const int last = (int) pts.size() - 1;
    const int i    = draggingPoint;

    float newLevel = yToLevel (e.position.y);
    float newTime  = pts[(size_t) i].time;

    if (i == 0)
    {
        newTime = 0.0f;  // pierwszy punkt zakotwiczony w czasie 0
    }
    else
    {
        const float lower = pts[(size_t) (i - 1)].time + 1.0e-3f;
        newTime = juce::jmax (lower, snapTimeToGrid (xToTime (e.position.x)));
        if (i < last)
            newTime = juce::jmin (newTime, pts[(size_t) (i + 1)].time - 1.0e-3f);
        else
            newTime = juce::jmin (newTime, maxTime);
    }

    pts[(size_t) i].time  = newTime;
    pts[(size_t) i].level = newLevel;

    processor.commitEnvelope (activeEnv);
    repaint();
}

void EnvelopeEditor::mouseUp (const juce::MouseEvent&)
{
    draggingPoint = -1;
    draggingCurve = -1;
    updateView();
    repaint();
}

void EnvelopeEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    auto& model = activeModel();
    auto& pts   = model.points;
    const int idx = findPointNear (e.position, hitRadius);

    // Dwuklik na uchwycie krzywizny (a nie na punkcie) => reset segmentu do liniowego.
    if (idx < 0)
    {
        const int seg = findCurveHandleNear (e.position, hitRadius);
        if (seg >= 0)
        {
            pts[(size_t) (seg + 1)].curve = 0.0f;
            processor.commitEnvelope (activeEnv);
            repaint();
            return;
        }
    }

    // Zapamiętaj czas punktu sustain, by go odtworzyć po reindeksacji.
    const float susTime = pts[(size_t) juce::jlimit (0, (int) pts.size() - 1,
                                                      model.sustainIndex)].time;

    if (idx >= 0)
    {
        // Nie pozwalamy usunąć skrajnych punktów (kotwice obwiedni).
        if (idx == 0 || idx == (int) pts.size() - 1)
            return;
        pts.erase (pts.begin() + idx);
    }
    else
    {
        EnvPoint np;
        np.time  = juce::jlimit (0.0f, maxTime, snapTimeToGrid (xToTime (e.position.x)));
        np.level = yToLevel (e.position.y);
        np.curve = 0.0f;
        pts.push_back (np);
    }

    model.sortAndClamp();

    // Odtwórz indeks sustain jako najbliższy poprzedniemu czasowi.
    int   best = 0;
    float bestD = 1.0e9f;
    for (int i = 0; i < (int) pts.size(); ++i)
    {
        const float d = std::abs (pts[(size_t) i].time - susTime);
        if (d < bestD) { bestD = d; best = i; }
    }
    model.sustainIndex = best;

    processor.commitEnvelope (activeEnv);
    updateView();
    repaint();
}

void EnvelopeEditor::timerCallback()
{
    const bool  sync = processor.isEnvSync();
    const float grid = processor.gridDivisionBeats();
    const bool  snap = processor.isEnvSnap();
    bool        changed = (sync != lastSync || ! juce::exactlyEqual (grid, lastGrid) || snap != lastSnap);
    lastSync = sync; lastGrid = grid; lastSnap = snap;

    for (int i = 0; i < FiSynthAudioProcessor::kMaxPlayheads; ++i)
    {
        const float t = processor.envPlayheadTime[activeEnv][i].load();
        const int   n = processor.envPlayheadNote[i].load();
        if (! juce::exactlyEqual (t, playheadTime[i]) || n != playheadNote[i])
        {
            playheadTime[i] = t;
            playheadNote[i] = n;
            changed = true;
        }
    }

    if (changed)
        repaint();
}
