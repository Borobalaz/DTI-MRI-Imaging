#include "widgets/inspect_fields/InspectActionFieldWidget.h"

InspectActionFieldWidget::InspectActionFieldWidget(QWidget *parent)
  : QPushButton(parent)
{
  setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  QObject::connect(this, &QPushButton::clicked, this, [this]()
  {
    if (actionCallback) actionCallback();
  });
}

InspectActionFieldWidget::InspectActionFieldWidget(QString fieldId, QString displayName,
                                                   QString groupName, QWidget *parent)
  : InspectActionFieldWidget(parent)
{
  fieldIdValue = std::move(fieldId);
  displayNameValue = std::move(displayName);
  groupNameValue = std::move(groupName);
  setText(displayNameValue);
}

IInspectWidget *InspectActionFieldWidget::addToLayout(QHBoxLayout *layout) { layout->addWidget(this, 1); return this; }
QString InspectActionFieldWidget::fieldId() const { return fieldIdValue; }
QString InspectActionFieldWidget::displayName() const { return displayNameValue; }
QString InspectActionFieldWidget::groupName() const { return groupNameValue; }
bool InspectActionFieldWidget::isReadOnly() const { return true; }
void InspectActionFieldWidget::SetValue(const QVariant &) {}
QVariant InspectActionFieldWidget::GetValue() const { return QVariant(); }