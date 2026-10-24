# REminiscence 0.5.6 integration ledger

The user approved implementing the useful enhancements on 2026-10-23, keeping
chermenin/REminiscence as the base and reimplementing the additions in modules.
Amiga, GOG DOS/Windows data, and Macintosh CD are explicit asset targets. An
edition's higher resolution belongs to its asset renderer, not the simulation.

Comparison snapshots:

- Base: chermenin/REminiscence `2200e18c6e5eb710b05c5095a751882591c21cf8`.
- Reference: cyxx/REminiscence `947a4dce261aacc4f6ec0c2c90cb9d7e37712c7b`,
  the 0.5.6 import. The repositories have no shared Git merge base.
- The local modernization work is included when deciding what is missing.

## Implementation status

| Enhancement | Status and evidence |
| --- | --- |
| Static resource integrity | Restored 3,227 reference-matched literals in 43 tables; regression checks cover sentinels, PAQ sorting, font/sample bytes. A confirmed 2024 migration defect replaced FF with 27; legitimate fork changes remain. |
| Effective bank cache | Implemented missing entry count update, with a bounded index. |
| Replayed effect channel | Implemented search beyond free channels; synthetic regression passes. |
| Configurable audio output | Validated 8,000–192,000 Hz; SDL device negotiation, device-specific pause/lock/close, rate reported to consumers. Dummy-device tests at 8,000/44,100/48,000 Hz pass. |
| Maximized window | Implemented as platform settings; CLI implies window mode. |
| Release diagnostics and feedback | Runtime debug mask available in Release; save/load and other user feedback uses unconditional info messages. |
| Macintosh graphics decoder | Reimplemented bounded C103/C211 parsing into owned images; separate clipped, flipped, transparent, masked/font blitting; synthetic success, repeat, truncation and malformed-input tests pass. Real Mac assets pending. |
| Macintosh resource lookup | Validated AppleDouble/raw/MacBinary forks; typed lookup; both index bounds checked; RAII storage. Synthetic duplicate-name and malformed-payload tests pass. |
| Macintosh menus | Reuse common Start/Skill/Password/Controls/Demo/Quit navigation with edition artwork and font. Synthetic Start/Skill/Controls/Password/Demo/credits navigation passes; real artwork and Mac validation pending. |
| Macintosh demo replay | Embedded resource loading and input-bit conversion, replay-specific positions and random counter present. Replay facing workaround is restricted to demo mode; real replay comparison pending. |
| Macintosh CD text/palettes/audio | Both color/credits prefixes, red-room Conrad palette, Junky palette, inventory sound and effect gain present. Real edition validation pending. |
| Cutscene first-sequence offset | Macintosh absolute offset handled for sequence zero as well as subsequent sequences. |
| Macintosh ellipses/concave shapes | Scale radii with center; concave shapes use clipped even-odd scan conversion; preserve historical convex rounding. Synthetic coverage present. |
| Full alpha palette bank | Corrected blend mask; regression check present. |
| Widescreen menu transitions | Clear stale room margins before presenting a menu and before entering gameplay. |
| Adjacent rooms on Amiga/PC-98 | Shared room decoder retains adjacent-blur; Amiga bank restoration regression passes all 419 valid rooms. |
| Pending cutscene and inventory ordering | Suppress room reveal for queued cutscenes; return after inventory modal. |
| Bytekiller decompression | Bounded reverse input/output, validated references and zero-length handling; synthetic malformed cases and real Amiga resources pass. Bank loading supplies the allocation prefix instead of an unbounded end pointer. |
| File/screenshot diagnostics | Bounded memory stream operations, file size checks, absolute output paths and screenshot failure reporting. |
| PRF/MIDI music | Implemented bounded parser, fractional sequencer and owned backends. AdLib and FluidSynth synthesis pass; MT-32 missing-ROM handling passes. Real DOS songs and MT-32 playback pending assets. |
| Sega Mega Drive extracted data | Experimental extracted-data path implemented, with bounded RLE/tile modules and corrected destination tile placement. Synthetic tests pass; actual Sega data, ROM extraction and Sega sound are not validated/implemented. |
| Sega CD US extension (1.23) | Separate disc adapter, CUE/BIN extraction, MCD movies/audio, VCE dialogue, per-level sprites and WAV soundtrack. Actual supplied disc integration checks pass; see [the format specification](SEGA_CD.md) for scope and remaining validation. |
| External CREDITS.CRD | Implemented bounded loader and display-buffer validation with embedded fallback. |
| Credits menu entry | Common level selector now exposes the existing credits sequence separately from seven playable levels. |
| Edition traits | Implemented immutable `editionTraits(ResourceType)` for byte order, resolution, introduction and protection behavior. |
| Macintosh version reporting | Implemented bounded Pascal-string version reporting; synthetic check passes. |

Statuses distinguish source implementation, synthetic checks, private-asset
integration and complete playthrough validation. No row means a whole edition
has passed a playthrough. No claim of a bug-free engine is made.

## Preserved fork work

Russian localization, adjacent-blur, embedded controller mappings, separate
TUNES discovery, extra MOD filename fallbacks, save feedback, manual save
semantics, invalid-room/demo guards and prior configuration fixes are retained.
Stereo audio, most simulation renames, const annotations, alternate trigonometry
and equivalent loader rewrites are not counted as new playable features.

## Reference assets

User attachments are preserved under ignored `.local/reference`. The WHDLoad
archive can now be prepared with `tools/extract_amiga.py`, using Python's
`lhafile` reader and Ancient for RNC data. The tool produces 128 engine assets
(103 RNC-decompressed, 25 unchanged), skipping the original executable and
WHDLoad support files. The earlier manual extraction's count of 104 RNC files
included the Amiga executable, which the engine does not use.

The tool's `.local/assets/amiga-prepared` output matches the existing reference
resources in `.local/assets/amiga-unpacked` byte for byte and passes the same
seven-level integration harness. The tool writes a hash/size manifest, checks
LHA and RNC integrity, rejects ambiguous paths/resources, and publishes only
after conversion succeeds. Original and unpacked files remain private and
untracked. The engine uses the prepared directory, not the LHA archive or
WHDLoad slave directly.

The 37-page supplied manual is a Macintosh manual. It is useful for menu,
inventory, save-station and action-timing reference, but its Mac menu-bar
shortcuts do not define bindings for the other editions.

The headless Amiga harness loads seven levels, renders 419 valid rooms,
advances 210 simulation frames, checks manual save/load, rewind and opening polygon cutscenes, and writes
screenshots for inspection. This is regression coverage, not a completed game.
