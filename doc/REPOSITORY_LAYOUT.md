# Repository layout

Decodium keeps source files, runtime resources, packaging definitions, developer
tools, and generated output in separate locations.

## Maintained directories

- `src/`: Decodium application code organized by ownership:
  - `app/`: startup, configuration, settings, migration, and update handling.
  - `bridge/`: QML/native bridge, diagnostics, logging, and legacy adapters.
  - `core/`: shared utilities, runtime policies, and low-level helpers.
  - `models/`: application data models.
  - `radio/`: CAT, transceiver, OmniRig, and radio profiles.
  - `security/`: certificates and protected settings.
  - `services/`: alerts, propagation, cluster, reporting, and lookup services.
  - `ui/`: native widgets, waterfall, panadapter, map, palette, and theme code.
- `Decoder/`, `Detector/`, `Modulator/`, `Transceiver/`, `Network/`, `widgets/`:
  established DSP, protocol, networking, and legacy widget subsystems.
- `qml/`: Qt Quick user interface.
- `artwork/`, `icons/`, `Palettes/`, `sounds/`, `shaders/`: runtime resources.
- `packaging/`: Windows installer definitions and Docker build environments.
- `scripts/build/`: platform and container build entry points.
- `scripts/`: CI and release helpers shared by multiple platforms.
- `tools/`: diagnostics, analysis, migration, and maintenance utilities.
- `tests/`: automated tests and compact regression data.
- `doc/`: user, release, and developer documentation.
- `contrib/`: vendored third-party runtime dependencies. Keep the upstream
  license with every retained dependency.

## Generated directories

Build and release output must not be committed. This includes:

- `build*`
- `dist-*`
- `release-*`
- `local-release`
- `installer-output`
- `packaging/windows/output`
- `tmp`
- `build-logs`

GitHub Actions recreates portable bundles and installers from the source tree.

## Root directory

The root contains CMake entry points, release metadata, licenses, and installed
runtime data. New native application sources belong in the matching `src/`
subsystem instead of restoring the historical flat source layout.

## Legacy references

Historical UI and test notes live under `doc/development/legacy/`. They are not
runtime inputs. Standalone analysis scripts live under `tools/analysis/`, while
obsolete platform-specific helpers belong under `tools/legacy/` only when they
remain useful for reference.
