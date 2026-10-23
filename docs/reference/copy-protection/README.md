# Flashback protection settings

Original protection prompts can be enabled for supported editions. Players
need their own edition's documentation; this project distributes no challenge
sheets or answers.

| Edition / protection screen | Setting when protection is enabled |
| --- | --- |
| DOS shape challenge | `use_words_protection=false` |
| DOS SSI manual-word challenge | `use_words_protection=true` |
| Amiga shape challenge | `use_words_protection=false` |

## Using the original protection screens

The engine defaults to `bypass_protection=true`. To enable the original
challenge, set the following in your `fb.cfg` or the file selected by `--config`:

```ini
bypass_protection=false
use_words_protection=false
```

This selects the shape-based screen for applicable DOS and Amiga editions.
For the DOS SSI manual-word screen, use `use_words_protection=true` instead.
The engine does not load reference-sheet PNGs.

## Provenance

Three reference sheets were supplied by the project owner on 2026-10-23.
Their creator, source URL and redistribution license were not supplied. The
unchanged files are retained only in ignored
`.local/reference/restricted/copy-protection/` storage and excluded from public
source/release archives. The engine source code's GPL does not relicense them.
Flashback is credited to Delphine Software.
