#include "ui/controllers/MainWindowShortcuts.h"

#include <QKeySequence>
#include <QShortcut>
#include <QWidget>

MainWindowShortcuts::MainWindowShortcuts(QWidget *parent)
  : QObject(parent)
{
  toggleThemeShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+T")), parent);
  toggleThemeShortcut->setContext(Qt::WindowShortcut);

  QObject::connect(toggleThemeShortcut, &QShortcut::activated, this, [this]()
  {
    emit toggleThemeRequested();
  });
}
