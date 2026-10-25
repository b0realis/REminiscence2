# Supplied application icon

`flashback-app-icon.jpg` is the original 1254×1254 JPEG supplied by the project
owner on 2026-10-25, saved byte-for-byte at their request. It depicts circular
Flashback-themed face artwork. No artist identity or separate artwork license
was included with the attachment. The engine's GPL notice does not relicense
this image; rights in the artwork remain with their respective holders.

The private Mac builder accepts this file through `--icon` and creates the native
macOS icon family without redrawing the image. Public binary packaging does not
automatically include this optional artwork. Generated `.icns` files live in
the private app bundles; the original JPEG is the repository source asset.

`flashback-app-icon.png` is the transparent-background derivative made with the
image-editing tool at the owner's request. Use this PNG for app packaging: its
exterior and corners have alpha zero; bright areas inside the circle are retained. Keep the JPEG as the supplied original. Both have the artwork status
described above. Native `.icns` conversion preserves the PNG alpha channel.
