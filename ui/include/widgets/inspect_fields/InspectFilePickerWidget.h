#pragma once

#include <functional>
#include <utility>

#include <QFileDialog>
#include <QHBoxLayout>
#include <QFileInfo>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QString>
#include <QVariant>
#include <QWidget>

#include "widgets/inspect_fields/IInspectWidget.h"

class InspectFilePickerWidget : public QWidget, public IInspectWidget
{
public:
  explicit InspectFilePickerWidget(QWidget *parent = nullptr);
  InspectFilePickerWidget(QString fieldId, QString displayName, QString groupName,
                          QString dialogTitle = QString(),
                          QString fileFilter = QStringLiteral("All files (*.*)"),
                          bool readOnly = false, QWidget *parent = nullptr);
  IInspectWidget *addToLayout(QHBoxLayout *layout) override;
  QString fieldId() const override;
  QString displayName() const override;
  QString groupName() const override;
  bool isReadOnly() const override;
  void SetValue(const QVariant &value) override;
  QVariant GetValue() const override;

  std::function<void(const QVariant &)> valueChangedCallback;

private:
  QString currentDirectory() const;

  QString fieldIdValue;
  QString displayNameValue;
  QString groupNameValue;
  QString dialogTitleValue;
  QString fileFilterValue;
  bool readOnlyValue = false;
  QLineEdit *pathEdit = nullptr;
  QPushButton *browseButton = nullptr;
};