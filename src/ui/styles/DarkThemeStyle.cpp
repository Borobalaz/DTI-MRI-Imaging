#include "ui/styles/DarkThemeStyle.h"

QString DarkThemeStyle::styleSheet() const
{
  return
    "QMainWindow { background-color: #1b2635; }"
    "QFrame#objectsPanel, QFrame#renderStatsPanel, QFrame#inspectorPanel, QFrame#viewportPanel {"
    "  background-color: #131925;"
    "  border: 1px solid #243042;"
    "  border-radius: 12px;"
    "}"
    "QLabel#panelTitle { color: #9aa6b2; font-weight: 700; font-size: 18px; }"
    "QScrollArea#inspectorScrollArea { background-color: #131925; border: none; }"
    "QScrollArea#inspectorScrollArea > QWidget > QWidget { background-color: #131925; }"
    "QWidget#inspectorContent { background-color: #131925; }"
    "QListWidget#objectList { background: transparent; color: #d8e1ea; border: none; }"
    "QListWidget#objectList::item { padding: 8px; border-radius: 6px; }"
    "QListWidget#objectList::item:selected { background: #243042; }"
    "QLabel { color: #d8e1ea; }"
    "QLineEdit, QDoubleSpinBox, QComboBox {"
    "  background: #0f1723; color: #d8e1ea; border: 1px solid #243042; border-radius: 6px;"
    "}"
    "QCheckBox { color: #d8e1ea; }";
}
