"""SCAU PreProc Workbench QGIS plugin (M287-E1)."""


def classFactory(iface):  # noqa: N802 (QGIS plugin API name)
    from .plugin import ScauPreprocWorkbenchPlugin

    return ScauPreprocWorkbenchPlugin(iface)
