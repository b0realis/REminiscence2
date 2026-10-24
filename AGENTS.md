# REminiscence2 project conventions

- Each delivered patch increments the player-facing version once: `1.01`,
  `1.02`, and so on. Use `python3 tools/bump_version.py`; `--dry-run` previews
  the next version. The part after the dot is an integer patch counter with
  at least two digits: `1.09` becomes `1.10`, and `1.99` becomes `1.100`.
- `version.h` is the canonical version source. CMake derives numeric package
  metadata from it. Do not duplicate the current version in tests or other
  source files. Historical screenshots and validation records keep the version
  actually captured. App-version changes do not change save/profile schemas.
- Keep the desktop engine usable from the command line. Target releases include
  Win64, Linux x86-64, Steam Deck, macOS arm64, macOS x86-64, and a sideloaded
  Android arm64 Quest app displayed as a flat panel.
- Preserve imported license/credit notices. Keep original game files and supplied
  publisher/reference scans private. Run `python3 tools/check_release_docs.py`
  for documentation or release changes and appropriate CTest suites for code.
