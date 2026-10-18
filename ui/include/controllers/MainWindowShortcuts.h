#pragma once

#include <QObject>

class QShortcut;

class MainWindowShortcuts : public QObject
{
  Q_OBJECT

public:
  explicit MainWindowShortcuts(QWidget *parent = nullptr);

signals:
  void toggleThemeRequested();
  void engineSettingsRequested();

private:
  QShortcut *toggleThemeShortcut = nullptr;
  QShortcut *engineSettingsShortcut = nullptr;
};
