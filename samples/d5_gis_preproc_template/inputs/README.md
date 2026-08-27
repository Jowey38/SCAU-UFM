# Optional raw inputs

Use this directory for additional source files that are not covered by the standard template directories, for example:

- rainfall/time-series files referenced by SWMM;
- external boundary forcing;
- source lookup tables;
- provider-specific GIS layers;
- checksum manifests.

Every file placed here must also be added to `metadata/dataset_inventory.csv` and `manifest.json`, with format, version, CRS/units where applicable, owner, authorization, and relationship notes.
