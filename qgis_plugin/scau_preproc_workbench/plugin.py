"""SCAU PreProc Workbench QGIS plugin shell (M287-E1)."""

from __future__ import annotations

from pathlib import Path

from qgis.PyQt.QtCore import QCoreApplication, QTranslator
from qgis.PyQt.QtGui import QIcon
from qgis.PyQt.QtWidgets import QAction


def _default_repo_root() -> str:
    # Deployment is a junction from the repo's qgis_plugin/ directory, so the
    # repo root is three levels above this file when run from the checkout;
    # a copied deployment can override via SCAU_UFM_REPO_ROOT.
    import os

    override = os.environ.get("SCAU_UFM_REPO_ROOT")
    if override:
        return override
    return str(Path(__file__).resolve().parents[2])


class ScauPreprocWorkbenchPlugin:
    def __init__(self, iface) -> None:
        self._iface = iface
        self._action = None
        self._dialog = None
        self._translator = None
        self._install_translator()

    def _install_translator(self) -> None:
        """E6: i18n/scau_preproc_workbench_<locale>.qm when present (Chinese
        source strings otherwise; see i18n/README.md)."""
        from qgis.core import QgsSettings

        locale = str(QgsSettings().value("locale/userLocale", "") or "")[:2]
        if not locale:
            return
        qm = Path(__file__).resolve().parent / "i18n" / f"scau_preproc_workbench_{locale}.qm"
        if qm.is_file():
            self._translator = QTranslator()
            if self._translator.load(str(qm)):
                QCoreApplication.installTranslator(self._translator)

    def initGui(self) -> None:  # noqa: N802 (QGIS plugin API name)
        icon = QIcon(str(Path(__file__).resolve().parent / "icon.svg"))
        self._action = QAction(icon, "SCAU PreProc Workbench", self._iface.mainWindow())
        self._action.triggered.connect(self._show_dialog)
        self._iface.addPluginToMenu("&SCAU-UFM", self._action)
        self._iface.addToolBarIcon(self._action)

    def unload(self) -> None:
        if self._action is not None:
            self._iface.removePluginMenu("&SCAU-UFM", self._action)
            self._iface.removeToolBarIcon(self._action)
            self._action = None
        self._dialog = None
        if self._translator is not None:
            QCoreApplication.removeTranslator(self._translator)
            self._translator = None

    def _show_dialog(self) -> None:
        from .workbench_dialog import WorkbenchDialog

        # Recreated每次打开：UI 无状态，报告一律从磁盘重读。仓库根仅作
        # hint 传入；对话框负责校验与持久化（拷贝部署下 __file__ 推断无效）。
        self._dialog = WorkbenchDialog(_default_repo_root(), self._iface.mainWindow(), iface=self._iface)
        self._dialog.show()
