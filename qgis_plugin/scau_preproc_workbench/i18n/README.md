# Translations (E6)

UI strings are authored in Simplified Chinese. `plugin.py` installs a
`QTranslator` for `i18n/scau_preproc_workbench_<locale>.qm` when the file exists
(locale from `QgsSettings` `locale/userLocale`, e.g. `en_US` → `en`).

Workflow (needs Qt `lupdate` / `lrelease` from the QGIS install):

```text
pylupdate6 ../*.py -ts scau_preproc_workbench_en.ts   # extract tr() strings
lrelease scau_preproc_workbench_en.ts                 # -> .qm (commit the .qm)
```

`scau_preproc_workbench_en.ts` is the English scaffold; strings not yet wrapped
in `self.tr()` fall back to Chinese. No `.qm` is committed yet, so every locale
currently shows the authored Chinese UI.
