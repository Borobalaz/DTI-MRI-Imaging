#include "widgets/inspect_fields/InspectFilePickerWidget.h"

InspectFilePickerWidget::InspectFilePickerWidget(QWidget *parent)
  : QWidget(parent)
{
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  pathEdit = new QLineEdit(this);
  pathEdit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  browseButton = new QPushButton("...", this);
  browseButton->setFixedWidth(32);
  layout->addWidget(pathEdit, 1);
  layout->addWidget(browseButton, 0);

  QObject::connect(pathEdit, &QLineEdit::editingFinished, this, [this]()
  {
    if (valueChangedCallback) valueChangedCallback(GetValue());
  });
  QObject::connect(browseButton, &QPushButton::clicked, this, [this]()
  {
    const QString selectedPath = QFileDialog::getOpenFileName(
        this, dialogTitleValue.isEmpty() ? QStringLiteral("Select file") : dialogTitleValue,
        currentDirectory(), fileFilterValue);
    if (selectedPath.isEmpty()) return;
    SetValue(selectedPath);
    if (valueChangedCallback) valueChangedCallback(GetValue());
  });
}

InspectFilePickerWidget::InspectFilePickerWidget(QString fieldId, QString displayName,
                                                 QString groupName, QString dialogTitle,
                                                 QString fileFilter, bool readOnly, QWidget *parent)
  : InspectFilePickerWidget(parent)
{
  fieldIdValue = std::move(fieldId);
  displayNameValue = std::move(displayName);
  groupNameValue = std::move(groupName);
  dialogTitleValue = std::move(dialogTitle);
  fileFilterValue = std::move(fileFilter);
  readOnlyValue = readOnly;
  pathEdit->setReadOnly(readOnlyValue);
  browseButton->setEnabled(!readOnlyValue);
}

IInspectWidget *InspectFilePickerWidget::addToLayout(QHBoxLayout *layout) { layout->addWidget(this, 1); return this; }
QString InspectFilePickerWidget::fieldId() const { return fieldIdValue; }
QString InspectFilePickerWidget::displayName() const { return displayNameValue; }
QString InspectFilePickerWidget::groupName() const { return groupNameValue; }
bool InspectFilePickerWidget::isReadOnly() const { return readOnlyValue; }

void InspectFilePickerWidget::SetValue(const QVariant &value)
{
  const QSignalBlocker blocker(pathEdit);
  pathEdit->setText(value.toString());
}

QVariant InspectFilePickerWidget::GetValue() const { return pathEdit->text(); }

QString InspectFilePickerWidget::currentDirectory() const
{
  const QString text = pathEdit->text().trimmed();
  if (text.isEmpty()) return QString();
  const QFileInfo info(text);
  return info.exists() ? info.absolutePath() : QString();
}