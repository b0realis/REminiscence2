# Editor dependencies

- `json.hpp`: nlohmann JSON **3.11.3**, MIT. License: `json-LICENSE.MIT`.
- `stb_image.h`: stb revision **003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5**,
  retaining its MIT/public-domain alternatives. License: `stb-LICENSE` and the
  header's embedded notice. The build enables only PNG, JPEG and BMP decoding.

Headers and licenses are unmodified. `source-manifest.json` records exact
upstream download URLs and SHA-256 checksums. No network request is needed to
build the editor. Release packaging's existing recursive third-party license
collection includes both notices.
