#include "styles/StyleSheetComposer.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>

namespace
{
// Resolves the root "ui/styles" directory: prefers the source tree (so a dev build picks up
//  edits directly) when it exists, falling back to the copy deployed next to the executable.
QString ResolveStylesRootDir()
{
#ifdef CONNECTOMICS_STYLE_SOURCE_DIR
  const QString sourceDir = QStringLiteral(CONNECTOMICS_STYLE_SOURCE_DIR);
  if (QFileInfo::exists(sourceDir))
  {
    return sourceDir;
  }
#endif

  return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("styles"));
}

QString TokensFilePath(const QString &themeName)
{
  return QDir(ResolveStylesRootDir()).filePath(QStringLiteral("tokens/%1.ini").arg(themeName));
}

// Every *.qss file under ui/styles/widgets/, discovered recursively so adding a new widget's
//  stylesheet requires no registration anywhere - just drop the file in.
QStringList DiscoverWidgetQssFiles()
{
  const QString widgetsDir = QDir(ResolveStylesRootDir()).filePath(QStringLiteral("widgets"));

  QStringList files;
  QDirIterator it(widgetsDir, {QStringLiteral("*.qss")}, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext())
  {
    files << it.next();
  }
  files.sort();
  return files;
}

QMap<QString, QString> LoadTokenMap(const QString &themeName)
{
  const QString path = TokensFilePath(themeName);
  QSettings settings(path, QSettings::IniFormat);

  QMap<QString, QString> tokens;
  const QStringList keys = settings.allKeys();
  if (keys.isEmpty())
  {
    qWarning() << "StyleSheetComposer: no tokens found for theme" << themeName << "at" << path;
  }

  for (const QString &key : keys)
  {
    tokens.insert(key, settings.value(key).toString());
  }
  return tokens;
}

QString SubstituteTokens(const QString &text, const QMap<QString, QString> &tokens)
{
  static const QRegularExpression tokenPattern(QStringLiteral("@([A-Za-z0-9_-]+)"));

  QString result;
  result.reserve(text.size());

  int lastEnd = 0;
  QRegularExpressionMatchIterator it = tokenPattern.globalMatch(text);
  while (it.hasNext())
  {
    const QRegularExpressionMatch match = it.next();
    result += text.mid(lastEnd, match.capturedStart() - lastEnd);

    const QString tokenName = match.captured(1);
    const auto found = tokens.constFind(tokenName);
    if (found == tokens.constEnd())
    {
      qWarning() << "StyleSheetComposer: unresolved style token" << ("@" + tokenName);
      result += match.captured(0);
    }
    else
    {
      result += found.value();
    }

    lastEnd = match.capturedEnd();
  }
  result += text.mid(lastEnd);

  return result;
}

QString ReadFileText(const QString &path)
{
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
  {
    qWarning() << "StyleSheetComposer: could not open" << path;
    return QString();
  }
  return QTextStream(&file).readAll();
}
}

QString ComposeThemeStyleSheet(const QString &themeName)
{
  const QMap<QString, QString> tokens = LoadTokenMap(themeName);

  QString combined;
  for (const QString &qssPath : DiscoverWidgetQssFiles())
  {
    combined += ReadFileText(qssPath);
    combined += QLatin1Char('\n');
  }

  return SubstituteTokens(combined, tokens);
}

QString ResolveThemeToken(const QString &themeName, const QString &tokenName)
{
  const QMap<QString, QString> tokens = LoadTokenMap(themeName);
  const auto found = tokens.constFind(tokenName);
  if (found == tokens.constEnd())
  {
    qWarning() << "StyleSheetComposer: no such token" << tokenName << "for theme" << themeName;
    return QString();
  }
  return found.value();
}

QDateTime LatestStyleSourceModTime(const QString &themeName)
{
  QDateTime latest;

  QStringList watchedPaths = DiscoverWidgetQssFiles();
  watchedPaths << TokensFilePath(themeName);

  for (const QString &path : watchedPaths)
  {
    QFileInfo info(path);
    info.refresh();
    const QDateTime modTime = info.lastModified();
    if (modTime.isValid() && (!latest.isValid() || modTime > latest))
    {
      latest = modTime;
    }
  }

  return latest;
}
