# Decodium 4 FT2 v1.0.604

Version 1.0.604 removes the SSB speech-to-text feature added in 1.0.603.

## English (UK)

### Speech to text has been removed

The transcription introduced one release ago did not reach a quality that is
usable on the air, and it has been taken out in full rather than left in place
behind a switch. What it produced on real signals was too often either nothing
or an invented sentence, and an invented sentence in the decode list is worse
than an empty one: it looks like a decode.

Removed completely — the recogniser, its worker thread, the model download, the
transcribed line in the decode list, the audio tap feeding it, the settings
section, and the vendored `whisper.cpp` tree. 155 files and 273 lines of
integration code. Nothing is left behind a disabled option, and the executable
is 2.5 MB smaller.

**What was learned, for whoever tries again.** Two defects were found in the
last hours of testing and are worth recording, because both would sink a second
attempt in the same way:

- The requested language never reached the recogniser. `whisper_full_params`
  holds a pointer, not a copy, and the temporary buffer it was given died at the
  end of the statement. The engine fell back to automatic language detection,
  which on degraded radio speech is unreliable — and a model that believes it is
  hearing English while listening to Italian does not translate: it invents.
  How much of the observed nonsense came from this was never measured.
- Transcribed lines never reached the panel. The decode list the code appended
  to is not what Full Spectrum draws; that panel renders a native model which
  rebuilds only when asked. The digital modes ask at the end of their cycle.
  Speech has no cycle — it arrives when somebody talks.

### Unchanged

Everything else in 1.0.603 stands: the RTTY window, the two-tone waterfall with
its mark and space reference lines, the tuning bridge, RTTY-U/RTTY-L in the mode
row, and the amplifier support. RTTY keeps its own exemption from the ghost
decode filter, which it needs because it carries free text rather than
structured messages.
