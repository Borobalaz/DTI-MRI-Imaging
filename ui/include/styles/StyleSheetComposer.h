#pragma once

#include <QDateTime>
#include <QString>

// Composes the full application stylesheet for the given theme ("dark" or "light"): discovers
//  every ui/styles/widgets/**/*.qss file, substitutes every "@token-name" reference against
//  ui/styles/tokens/<themeName>.ini, and concatenates the result for setStyleSheet().
QString ComposeThemeStyleSheet(const QString &themeName);

// Resolves a single named token's value for the given theme (e.g. "window-bg" -> "#1b2635"),
//  for C++ code that needs a theme color directly (QPalette, native titlebar) instead of via QSS.
//  Returns an empty string (with a qWarning) if the theme file or the token is missing.
QString ResolveThemeToken(const QString &themeName, const QString &tokenName);

// Returns the most recent modification time across every stylesheet source file for the given
//  theme (every widgets/*.qss file plus that theme's tokens/*.ini) - used to detect on-disk
//  edits for hot reload without relying on OS file-change notifications.
QDateTime LatestStyleSourceModTime(const QString &themeName);
