#pragma once

#include <functional>
#include <utility>

#include <QColor>
#include <QHBoxLayout>
#include <QPointer>
#include <QString>
#include <QVariant>
#include <QWidget>

#include "widgets/inspect_fields/IInspectWidget.h"

class QLabel;
class QToolButton;
class ColorPickerPopup;

// Inline color field: a swatch plus its #RRGGBB hex code. Clicking the swatch opens a
//  ColorPickerPopup whose edits are forwarded live through valueChangedCallback.
class InspectColorFieldWidget : public QWidget, public IInspectWidget
{
public:
  explicit InspectColorFieldWidget(QWidget *parent = nullptr);
  InspectColorFieldWidget(QString fieldId, QString displayName, QString groupName,
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
  void openPicker();
  void applyPickedColor(const QColor &color);
  void updateSwatch();

  QToolButton *swatchButton = nullptr;
  QLabel *hexLabel = nullptr;
  QPointer<ColorPickerPopup> picker; // Child of this widget; deletes itself on close
  QColor currentColor = Qt::black;
  QString fieldIdValue;
  QString displayNameValue;
  QString groupNameValue;
  bool readOnlyValue = false;
};
