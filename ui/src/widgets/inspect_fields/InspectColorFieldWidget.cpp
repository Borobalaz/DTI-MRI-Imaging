#include "widgets/inspect_fields/InspectColorFieldWidget.h"

#include <cmath>

#include <QLabel>
#include <QToolButton>

#include "styles/StyleSheetComposer.h"
#include "widgets/inspect_fields/ColorPickerPopup.h"

namespace
{
  // Half of one 8-bit step: anything closer is the same displayed color.
  constexpr double kChannelEpsilon = 1.0 / 510.0;

  bool IsSameColor(const QColor &a, const QColor &b)
  {
    return std::abs(a.redF() - b.redF()) <= kChannelEpsilon
        && std::abs(a.greenF() - b.greenF()) <= kChannelEpsilon
        && std::abs(a.blueF() - b.blueF()) <= kChannelEpsilon;
  }
}

InspectColorFieldWidget::InspectColorFieldWidget(QWidget *parent)
  : QWidget(parent)
{
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  swatchButton = new QToolButton(this);
  swatchButton->setObjectName("colorSwatchButton");
  swatchButton->setCursor(Qt::PointingHandCursor);
  swatchButton->setFixedSize(ResolveCommonToken("color-swatch-width").toInt(),
                             ResolveCommonToken("color-swatch-height").toInt());

  // Monospace font and muted color come from QLabel#colorHexLabel in InspectFields.qss.
  hexLabel = new QLabel(this);
  hexLabel->setObjectName("colorHexLabel");
  hexLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

  layout->addWidget(swatchButton);
  layout->addWidget(hexLabel);
  layout->addStretch(1);

  QObject::connect(swatchButton, &QToolButton::clicked, this, [this]()
  {
    openPicker();
  });
  updateSwatch();
}

InspectColorFieldWidget::InspectColorFieldWidget(QString fieldId, QString displayName,
                                                 QString groupName, bool readOnly, QWidget *parent)
  : InspectColorFieldWidget(parent)
{
  fieldIdValue = std::move(fieldId);
  displayNameValue = std::move(displayName);
  groupNameValue = std::move(groupName);
  readOnlyValue = readOnly;
  setEnabled(!readOnlyValue);
}

IInspectWidget *InspectColorFieldWidget::addToLayout(QHBoxLayout *layout) { layout->addWidget(this, 1); return this; }
QString InspectColorFieldWidget::fieldId() const { return fieldIdValue; }
QString InspectColorFieldWidget::displayName() const { return displayNameValue; }
QString InspectColorFieldWidget::groupName() const { return groupNameValue; }
bool InspectColorFieldWidget::isReadOnly() const { return readOnlyValue; }

void InspectColorFieldWidget::SetValue(const QVariant &value)
{
  QVariantList list = value.toList();
  if (list.size() < 3) list = {0.0, 0.0, 0.0};
  const QColor color = QColor::fromRgbF(static_cast<float>(qBound(0.0, list[0].toDouble(), 1.0)),
                                        static_cast<float>(qBound(0.0, list[1].toDouble(), 1.0)),
                                        static_cast<float>(qBound(0.0, list[2].toDouble(), 1.0)));

  // Called on every sync revision with the engine's echo of what the picker just sent; ignoring
  //  sub-step differences keeps the open picker's hue/saturation from snapping mid-drag.
  if (IsSameColor(color, currentColor)) return;

  currentColor = color;
  updateSwatch();
  if (picker) picker->setColor(color);
}

QVariant InspectColorFieldWidget::GetValue() const
{
  return QVariantList{currentColor.redF(), currentColor.greenF(), currentColor.blueF()};
}

void InspectColorFieldWidget::openPicker()
{
  if (readOnlyValue || (picker && picker->isVisible())) return;

  // Parented to this field so it can't outlive it (e.g. a setFields() rebuild while open).
  picker = new ColorPickerPopup(this);
  picker->colorChangedCallback = [this](const QColor &color)
  {
    applyPickedColor(color);
  };
  picker->open(swatchButton, currentColor);
}

void InspectColorFieldWidget::applyPickedColor(const QColor &color)
{
  currentColor = color;
  updateSwatch();
  if (valueChangedCallback) valueChangedCallback(GetValue());
}

void InspectColorFieldWidget::updateSwatch()
{
  ColorPickerPopup::SetSwatchFill(swatchButton, currentColor);
  hexLabel->setText(currentColor.name(QColor::HexRgb).toUpper());
}
