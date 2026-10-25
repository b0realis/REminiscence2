# Dear ImGui

Core and SDL2 / SDL_Renderer2 backend files imported from
https://github.com/ocornut/imgui at tag v1.91.9b, commit
`f5befd2d29e66809cd1110a152e375a7f1981f06`.

The MIT license is preserved in LICENSE.txt. Embedded font and stb notices
remain in their source files. source-manifest.json records original upstream hashes and sizes.

Local SDL2 backend patch: while the mouse is over this window, poll the
content-relative SDL position instead of subtracting desktop and window origins.
This avoids a fullscreen macOS QoL offset; outside-window capture retains the
upstream fallback. The core and SDL_Renderer2 backend are unmodified.
No demo, third-party game assets or reference screenshot is bundled.

The settings overlay uses these vendored files; builds do not fetch them.
