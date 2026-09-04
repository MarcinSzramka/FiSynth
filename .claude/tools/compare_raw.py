#!/usr/bin/env python3
"""Porownuje dwa zrzuty float32 z bench_voice: ile realnie zmienil sie dzwiek.

  compare_raw.py ref.raw new.raw

Raportuje na kazdy patch:
  err SNR  - 20*log10(rms(ref) / rms(ref-new)); im wyzej tym blizej referencji.
             Powyzej ~60 dB roznica jest ponizej progu slyszalnosci dla materialu
             muzycznego; identycznosc bit w bit daje "inf".
  max|d|   - najwiekszy bezwzgledny blad probki.
  dRMS     - wzgledna zmiana glosnosci (musi byc ~0, inaczej cos sie rozjechalo).
"""
import sys, math, struct, array

def load(path):
    a = array.array('f')
    with open(path, 'rb') as f:
        data = f.read()
    a.frombytes(data)
    return a

def main():
    if len(sys.argv) != 3:
        print(__doc__); return 1
    ref, new = load(sys.argv[1]), load(sys.argv[2])
    if len(ref) != len(new):
        print(f"ROZNA DLUGOSC: {len(ref)} vs {len(new)}"); return 1

    # bench_voice renderuje 3 patche po tyle samo probek
    n_patches = 3
    seg = len(ref) // n_patches
    names = ["default", "golden", "stretch"]

    print(f"{'patch':<9} {'err SNR':>10} {'max|d|':>12} {'dRMS':>10}")
    worst = 999.0
    for i, name in enumerate(names):
        r = ref[i*seg:(i+1)*seg]
        v = new[i*seg:(i+1)*seg]
        se = 0.0; sr_ = 0.0; sv = 0.0; mx = 0.0
        for x, y in zip(r, v):
            d = x - y
            se += d*d; sr_ += x*x; sv += y*y
            if abs(d) > mx: mx = abs(d)
        rms_r = math.sqrt(sr_/len(r)); rms_v = math.sqrt(sv/len(v))
        rms_e = math.sqrt(se/len(r))
        snr = float('inf') if rms_e == 0 else 20*math.log10(rms_r/rms_e)
        drms = 0.0 if rms_r == 0 else (rms_v-rms_r)/rms_r*100
        worst = min(worst, snr)
        s = "inf (bit w bit)" if snr == float('inf') else f"{snr:.1f} dB"
        print(f"{name:<9} {s:>10} {mx:12.3e} {drms:9.3f}%")

    print()
    if worst == float('inf'):
        print("WYNIK: identyczne bit w bit")
    elif worst >= 60:
        print(f"WYNIK: najgorszy SNR {worst:.1f} dB — ponizej progu slyszalnosci")
    else:
        print(f"WYNIK: UWAGA, najgorszy SNR {worst:.1f} dB — to moze byc slyszalne")
    return 0

if __name__ == '__main__':
    sys.exit(main())
