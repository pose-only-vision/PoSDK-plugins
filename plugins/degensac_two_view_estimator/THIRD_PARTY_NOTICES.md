# Third-party notices

## DEGENSAC / LO-RANSAC

`DegensacTwoViewEstimator/pydegensac` contains the DEGENSAC/LO-RANSAC C
implementation derived from the pydegensac project.

- Copyright (c) 2019 Ondra Chum and Dmytro Mishkin.
- License: MIT.
- Upstream: <https://github.com/ducha-aiki/pydegensac>
- Complete license text: `pydegensac/LICENSE`.

## CCMATH 2.2.1

The vendored `pydegensac/src/pydegensac/matutls` sources, and the CCMATH
routine retained in `degensac/DegUtils.c`, contain work by Daniel A. Atkinson.

- Copyright (C) 2000 Daniel A. Atkinson. All rights reserved.
- License: GNU Lesser General Public License 2.1 (`LGPL-2.1-only`).
- Complete license text:
  `pydegensac/src/pydegensac/matutls/lgpl.license`.

The PoSDK adapter and deterministic thread-local RNG changes are licensed
under MPL-2.0. A binary redistribution must preserve all notices and satisfy
the MIT and LGPL-2.1 terms, including the corresponding-source/relinking
obligations applicable to the statically incorporated CCMATH code.

Starting with plugin version 1.1.1, the signed package declares
`open_plugin_open_dependencies`. The complete adapter source, the exact
DEGENSAC and CCMATH source snapshot used by the binary, build instructions,
and relinking instructions are published at the same pinned Git commit and
GitHub Release tag as the `.pospkg`. See `BUILDING.md` and `RELINKING.md` in
that repository. This notice is informational and is not legal advice.
