# Decodium 4.0 v1.0.506

Version 1.0.506 completes the localisation of the Live Map introduced in
1.0.505. No functional change: the map behaves exactly as before, it simply
speaks the language you selected.

## Changes from 1.0.505 to 1.0.506

### Live Map fully localised

- 1.0.505 introduced 272 new translatable strings but shipped without
  translation catalog entries for them. 254 were missing outright, so Qt fell
  back to the source text and the thirteen non-English languages showed the
  whole Live Map, its roster, statistics and layer descriptions in English.
- All 254 strings are now translated into every supported language: 3556
  catalog entries across Catalan, Danish, Dutch, English, French, German,
  Hungarian, Italian, Japanese, Latvian, Russian, Simplified Chinese,
  Traditional Chinese and Spanish.
- Amateur radio abbreviations are deliberately left untouched (ADIF, PSK, QSL,
  QSO, DXCC, IOTA, POTA, WPX, MUF, foF2, SFI, CQ, CALL), and terminology
  follows the wording already used elsewhere in the interface.

### Verification

- Every catalog validates as XML, all fourteen files hold the same number of
  entries and none is left unfinished.
- Placeholder tokens (%1 … %9) are checked to match between source and
  translation in every entry, which is what prevents formatting failures at
  runtime.
- Compiled catalogs were read back and spot-checked in German, Japanese and
  Russian.

### Also included

- Qt Concurrent linkage fix for release packaging, from upstream 1.0.505.
