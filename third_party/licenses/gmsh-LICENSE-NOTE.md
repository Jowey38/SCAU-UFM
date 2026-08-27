# gmsh license note

gmsh is distributed under **GPL-2.0-or-later** (with the additional
clarifications published by the gmsh authors at https://gmsh.info/#Licensing).
It is NOT license-compatible with unrestricted static/dynamic linking into
proprietary or differently-licensed binaries.

SCAU-UFM usage decision (M287-A):

1. gmsh is consumed only as an **isolated subprocess** in the spike/preproc
   layer; its Python API is imported solely inside that subprocess.
2. Nothing in `libs/`, `apps/`, or `tests/` links gmsh; the main CMake graph
   has no gmsh dependency; the interchange contract is on-disk files only
   (JSON config in, `.stcf.nc`/reports/diagnostic GeoJSON out).
3. The same subprocess boundary already serves the QGIS (GPL) UI isolation
   in the M287 workbench design, so the whole GPL surface stays on the
   tooling side of the file-interchange line.
4. Any future in-process embedding or redistribution of gmsh requires a
   fresh license review recorded in this file before implementation.
