# Lensfun lens database (vendored)

`db/*.xml` is the lens calibration database of the lensfun project
(https://github.com/lensfun/lensfun, `data/db/`), copied verbatim from upstream master
commit `43f9e001ab66c4fcdd4f400463f13b72f94b2288` (2026-10-08).

Only the DATA is used. Winnow does not link or copy the lensfun library (LGPL-3): the
distortion / TCA / vignetting models and the lens matching are Winnow's own code, written
from the published model formulas (`Develop/lensdb.*`, `Develop/lensprofile.h`).

## Licence

The database is licensed under Creative Commons Attribution-ShareAlike 3.0
(`cc-by-sa-3.0.txt`). Attribution: "Lens profiles from the lensfun project
(lensfun.github.io), CC BY-SA 3.0." It appears in the Lens section's tooltip and in the
Develop Lens help page.

## How it ships

`lensfun.qrc` compiles the XML into the binary (`:/lensfun/*.xml`), so no packaging step
is needed on either platform.

At load, Winnow also reads any `*.xml` in the user override folder
`<AppDataLocation>/lensfun/` (shown in the Lens help). A lens or camera defined there
replaces the bundled one with the same maker + model, the same way lensfun's own local
database works. That is how a user adds a lens or takes a newer database without a
rebuild.

## Updating

1. Copy upstream `data/db/*.xml` over `db/`.
2. Regenerate `lensfun.qrc`: one `<file alias="NAME.xml">db/NAME.xml</file>` line per
   file, under the `/lensfun` prefix.
3. Update the commit hash above.
4. Run `tst_lensprofile`.
