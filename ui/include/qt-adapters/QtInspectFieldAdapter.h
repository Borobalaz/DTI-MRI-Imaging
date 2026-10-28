#pragma once

#include <memory>
#include <vector>

#include <QObject>
#include <QVariant>

#include "inspection/InspectField.h"
#include "widgets/inspect_fields/IInspectWidget.h"

// Converts engine-neutral InspectValue <-> QVariant, keyed by the field's InspectFieldType.
QVariant ToVariant(const InspectValue& value);
InspectValue ToInspectValue(const QVariant& value, InspectFieldType type);

// Adapts a single engine InspectField into a concrete Qt field widget (ui/include/widgets/inspect_fields/),
//  selected by InspectFieldType, and routes edits back through the engine field's getter/setter.
class QtInspectFieldAdapter : public QObject, public IInspectWidget
{
public:
  explicit QtInspectFieldAdapter(InspectFieldPtr engineField);

  QString fieldId() const override;
  QString displayName() const override;
  QString groupName() const override;
  bool isReadOnly() const override;
  double minimum() const override;
  double maximum() const override;
  IInspectWidget* addToLayout(QHBoxLayout* layout) override;
  void SetValue(const QVariant& value) override;
  QVariant GetValue() const override;

private:
  InspectFieldPtr field;
  std::shared_ptr<IInspectWidget> widget;
};

// Adapts a batch of engine InspectFields into Qt field widgets, skipping null entries.
std::vector<std::shared_ptr<IInspectWidget>> AdaptFields(const std::vector<InspectFieldPtr>& fields);
