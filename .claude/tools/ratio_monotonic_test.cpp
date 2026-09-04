// Sprawdza zalozenie, na ktorym opiera sie `break` (zamiast `continue`) na granicy
// Nyquista w goracej petli: ratio partiali MUSI byc niemalejace po n dla KAZDEJ
// kombinacji (stretchMode, stretch). Gdyby gdzies malalo, `break` uciolby partial,
// ktory jeszcze miesci sie w pasmie -> ciche zniekszalcenie brzmienia.
#include <JuceHeader.h>
#include "PartialTables.h"
#include <cstdio>

int main()
{
    int bad = 0;
    double worstDrop = 0.0;
    float  worstMode = 0.0f, worstStretch = 0.0f;

    // Gesta siatka: tryby ciagle 0..8 co 0.05, stretch 0..1 co 0.02.
    for (float mode = 0.0f; mode <= (float) (fiNumStretchModes - 1) + 1.0e-6f; mode += 0.05f)
    {
        for (float stretch = 0.0f; stretch <= 1.0f + 1.0e-6f; stretch += 0.02f)
        {
            double r[fiNumPartials];
            fiStretchedRatios (mode, stretch, r);

            for (int n = 1; n < fiNumPartials; ++n)
            {
                if (r[n] < r[n - 1])
                {
                    const double drop = r[n - 1] - r[n];
                    if (drop > worstDrop)
                    {
                        worstDrop = drop; worstMode = mode; worstStretch = stretch;
                    }
                    ++bad;
                }
            }
        }
    }

    if (bad == 0)
    {
        std::printf ("[ OK ] ratio niemalejace na calej siatce — `break` jest bezpieczny\n");
        return 0;
    }

    std::printf ("[FAIL] %d naruszen monotonicznosci; najgorsze przy mode=%.2f stretch=%.2f (spadek %.6f)\n",
                 bad, worstMode, worstStretch, worstDrop);
    std::printf ("       => `break` NIE jest bezpieczny, zostawic `continue`\n");
    return 1;
}
