# REminiscence2 licensing and provenance

The project intends to remain free software, with new REminiscence2
contributions available under GNU GPL version 3 or any later version. Imported
copyright and license grants remain attached to their original material.
The combined initial tree can use GPLv3; the inherited later-version grant
needs confirmation before the entire distribution is labeled that way.

## Imported engine

The development checkout was initially cloned from
[chermenin/REminiscence](https://github.com/chermenin/REminiscence) at
`2200e18c6e5eb710b05c5095a751882591c21cf8`. It includes a `LICENSE` containing
GNU GPL version 3, and GitHub identifies that repository as GPL-3.0. The license
file remains unchanged. REminiscence2's publication branch starts with one fresh
root commit; earlier commits remain available in the linked upstream repository
and local comparison refs, rather than as ancestors of our branch. Its SHA-256 is:

```text
3972dc9744f6499f0f9b2dbf76696f2ae7ad8af9b23dde66d6af86c9dfb36986
```

Most engine file headers credit Gregory Montoir without separately stating
an applicable GPL version range. The example later-version wording near the
end of the generic GPL license text is not a project-specific grant. The
inherited README also does not expressly grant later versions. Preserve these
facts rather than silently relabeling other authors' work.

New standalone files use `SPDX-License-Identifier: GPL-3.0-or-later` where
appropriate. The configuration implementation retains Gregory Montoir's
copyright because it adapts the original configuration handling. Modifications
to imported files retain their notices; the contribution policy does not
broaden a pre-existing grant by itself.

The GNU project's guidance distinguishes a specific GPL version from an
explicit later-version permission:
[clear license identification](https://www.gnu.org/licenses/identify-licenses-clearly.html)
and [applying GNU licenses](https://www.gnu.org/licenses/gpl-howto.html).
Before publishing a broader version claim, locate an applicable original
notice or obtain confirmation from the relevant rights holders.

## Bundled and linked components

| Component | Evidence retained in the source | Treatment |
| --- | --- | --- |
| Engine and REinforced changes | Original headers, linked upstream history, pinned revision and `LICENSE` | Preserve attribution and GPLv3 distribution obligations |
| xBRZ | Notices in `scalers/xbr/xbrz.cpp`, `xbrz.h` and `config.h` name Zenju, GPLv3 and a MAME linking exception | Preserve those notices; verify the version range before a later-version claim; currently excluded from CMake builds |
| stb Vorbis | `stb_vorbis.c` contains Sean Barrett's MIT and public-domain alternatives | Preserve the full embedded notices, including when packaging binaries |
| Nuked OPL3 | Unmodified source and LGPL-2.1-or-later notices in `third_party/nuked-opl3`; commit pinned in its README | Keep full source, license and provenance with distributions |
| Dear ImGui | Unmodified v1.91.9b core/SDL2 backends in `third_party/imgui`, pinned commit and file hashes in its README/manifest; MIT license and embedded font/stb notices retained | Include the MIT license and retained source notices; no reference game screenshot or commercial font is bundled |
| FluidSynth and Munt/mt32emu | Optional linked dependencies; library notices installed with development packages | Record versions and package applicable notices; player supplies soundfonts and ROMs |
| SDL, zlib, libmodplug | External dependencies | Record the actual versions and include the applicable dependency notices in binary packages |

The pinned cyxx 0.5.6 comparison tree does not contain the selected fork's
`LICENSE` file. It is retained as a comparison remote. The new modules describe their format
research credit to Gregory Montoir and are implemented in this tree; they are
not a wholesale source replacement. Imported tables and modifications retain
the selected fork's existing notices and provenance.

## Game data and names

Flashback's original game data is separate from this engine's source license.
Players supply their original files. Do not add commercial game archives,
music, artwork, fonts or extracted resources to source or release packages
without permission that covers their redistribution.

The supplied protection sheets, technical PDFs and cover artwork had no recorded
redistribution grant. Their bytes and provenance are preserved only under
ignored `.local/reference/restricted/` storage. Public documentation retains
project-written [protection settings](reference/copy-protection/README.md),
[technical research notes](reference/technical/README.md) and an
[artwork provenance record](reference/artwork/README.md), without distributing
the underlying images or PDFs.

The [player field guide](../manual.md) is original REminiscence2 documentation
under GPL-3.0-or-later. It contains no reproduced publisher-manual passages,
comic, screenshots, game icons or cover art. The earlier transcription,
image manifest and all 54 manual illustrations were removed from the public
tree and retained only as private references. The
[documentation note](manual/README.md) records the new guide's scope and its
implementation references. The new guide's license grants no rights in the
original manual or game assets.

Changing format or adding a new license notice does not grant permission to
reuse someone else's protected material. The U.S. Copyright Office's
[derivative-work guidance](https://www.copyright.gov/circs/circ14.pdf) explains
the separate rights in pre-existing material. This cleanup replaces the
copied content rather than attempting to relicense it.

Keep the source code's inherited tables and notices during this initial pass;
review their provenance as part of release preparation. The name Flashback
and credit to Delphine Software identify the game and its creators and do not
assert affiliation or endorsement.

## Documentation release boundary

Run `python3 tools/check_release_docs.py` before preparing public source or
documentation. CI runs the same check. It rejects the known removed binary
reference locations, images embedded in the text-only guide, and tracked
private reference/build paths. It does not evaluate copyright in arbitrary
prose, embedded engine tables or dependency code.

`.gitignore` keeps local reference and build material out of ordinary staging.
`.gitattributes` adds `export-ignore` rules for those paths and the removed
asset locations when generating source archives with `git archive`. Copying
or zipping an entire working folder does not honor those rules; prepare source
archives from the reviewed public Git tree.

The guide cleanup does not resolve the inherited engine's version-range and
embedded-data provenance questions above, or replace dependency-license
obligations. Those remain part of preparing an engine release.
