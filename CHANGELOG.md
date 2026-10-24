<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Changes

## 1.05

- Fix Windows runtime-DLL discovery during packaging and include toolchain notices.
- Link Android system logging explicitly for the Quest host.
- Include a Quest deployment bundle with assets folder and Linux one-command helper.
- Verify same-commit CI, archive contents and source versions before tagged publishing.
- Ship corresponding SDL/zlib source, dependency manifests, checksums and public docs.

## 1.04

- Recover settings font textures alongside game textures after a renderer reset.
- Initialize the complete mirror-side texture before retaining its CPU backup.
- Test relocated-executable asset discovery and Unicode filesystem/save paths.
- Replace the obsolete Android SDK tools package in the build workflow.

## 1.03

- Add native Win64, Linux64, Steam Deck, Apple Silicon and Intel Mac packaging.
- Build pinned SDL 2.32.10 and zlib 1.3.1; remove local package-manager library paths.
- Add precreated assets folders, Linux/Deck launchers, Windows convenience launcher,
  and a native Mac app launcher alongside the command-line executable.
- Add an Android arm64 flat-panel host, bounded ZIP importer and signed APK recipe.
- Add a checksum-verified Linux ADB deployment helper that preserves saves/settings.
- Resolve Windows C++ standard, close-wrapper and path portability issues.

## 1.02

- Index assets deterministically without platform-specific handle errors, raw
  realloc arrays or recursive directory-symlink traversal.
- Write game saves atomically and propagate stream/compression close failures.
- Validate ABA/PAQ records and wrappers; bound PC-98 decompression input and output.
- Use writable user-data save defaults; retain explicit paths and portable mode.
- Handle controller hotplug, instance IDs, disconnect clearing and renderer resets.
- Size footer controls for enlarged text, retaining the turquoise EXIT GAME button.
- Reuse rendering/blur and T-shirt workspaces, cache keyboard geometry and avoid
  uploading unchanged paused frames. Simulation/gameplay timing remains unchanged.

## 1.01

- Establish the integer patch sequence, canonical version header and bump helper.
- Audit release blockers and define six distribution targets.

Device validation on Steam Deck and Quest, and full GOG/Macintosh data
playthroughs, remain separate from automated build/synthetic test coverage.
