#include "widgets/inspect_fields/InspectColorFieldWidget.h"

#include <cmath>

#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QSignalBlocker>

namespace inspect_color_internal
{
  ColorWheelWidget::ColorWheelWidget(QWidget *parent)
    : QWidget(parent)
  {
    setMinimumSize(96, 96);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  }

  void ColorWheelWidget::setCurrentColor(const QColor &color)
  {
    if (!color.isValid()) return;
    value = color.value();
    const int h = color.hsvHue();
    hue = h >= 0 ? h : 0;
    saturation = color.hsvSaturation();
    update();
  }

  QColor ColorWheelWidget::currentColor() const
  {
    return QColor::fromHsv(hue, saturation, value);
  }

  void ColorWheelWidget::paintEvent(QPaintEvent *)
  {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const int side = qMin(width(), height());
    const int radius = side / 2 - 2;
    const QPoint center(width() / 2, height() / 2);
    QImage image(side, side, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    for (int y = 0; y < side; ++y)
    {
      for (int x = 0; x < side; ++x)
      {
        const double dx = static_cast<double>(x - side / 2);
        const double dy = static_cast<double>(y - side / 2);
        const double r = std::sqrt(dx * dx + dy * dy);
        if (r <= radius)
        {
          const double satNorm = qBound(0.0, r / static_cast<double>(radius), 1.0);
          double angle = std::atan2(-dy, dx) * 180.0 / kPi;
          if (angle < 0.0) angle += 360.0;
          const double edgeAlpha = qBound(0.0, (static_cast<double>(radius) - r) / 2.0, 1.0);
          QColor pixel = QColor::fromHsv(static_cast<int>(angle), static_cast<int>(satNorm * 255.0), 255);
          pixel.setAlpha(static_cast<int>(edgeAlpha * 255.0));
          image.setPixelColor(x, y, pixel);
        }
      }
    }

    painter.drawImage(QRect(center.x() - side / 2, center.y() - side / 2, side, side), image);
    const double satNorm = static_cast<double>(saturation) / 255.0;
    const double rad = hue * kPi / 180.0;
    const QPoint marker(center.x() + static_cast<int>(std::cos(rad) * satNorm * radius),
                        center.y() - static_cast<int>(std::sin(rad) * satNorm * radius));
    painter.setPen(QPen(Qt::black, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(marker, 5, 5);
    painter.setPen(QPen(Qt::white, 1));
    painter.drawEllipse(marker, 6, 6);
  }

  void ColorWheelWidget::mousePressEvent(QMouseEvent *event) { pickFromPosition(event->pos()); }

  void ColorWheelWidget::mouseMoveEvent(QMouseEvent *event)
  {
    if (event->buttons() & Qt::LeftButton) pickFromPosition(event->pos());
  }

  void ColorWheelWidget::pickFromPosition(const QPoint &pos)
  {
    const int side = qMin(width(), height());
    const int radius = side / 2 - 2;
    const QPoint center(width() / 2, height() / 2);
    double dx = static_cast<double>(pos.x() - center.x());
    double dy = static_cast<double>(center.y() - pos.y());
    double r = std::sqrt(dx * dx + dy * dy);
    if (r > radius)
    {
      const double scale = static_cast<double>(radius) / r;
      r = radius;
      dx *= scale;
      dy *= scale;
    }
    double angle = std::atan2(dy, dx) * 180.0 / kPi;
    if (angle < 0.0) angle += 360.0;
    hue = static_cast<int>(angle);
    saturation = static_cast<int>((r / static_cast<double>(radius)) * 255.0);
    update();
    if (colorChangedCallback) colorChangedCallback(currentColor());
  }
}

InspectColorFieldWidget::InspectColorFieldWidget(QWidget *parent)
  : QWidget(parent)
{
  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);
  wheel = new inspect_color_internal::ColorWheelWidget(this);
  valueSlider = new QSlider(Qt::Vertical, this);
  valueSlider->setRange(0, 100);
  valueSlider->setValue(100);
  valueSlider->setFixedHeight(96);
  preview = new QLabel(this);
  preview->setFixedSize(24, 24);
  preview->setFrameShape(QFrame::Box);
  preview->setFrameShadow(QFrame::Sunken);
  layout->addWidget(wheel);
  layout->addWidget(valueSlider);
  layout->addWidget(preview);
  layout->addStretch(1);

  wheel->colorChangedCallback = [this](const QColor &)
  {
    updatePreview();
    if (valueChangedCallback) valueChangedCallback(GetValue());
  };
  QObject::connect(valueSlider, &QSlider::valueChanged, this, [this](int sliderValue)
  {
    QColor color = wheel->currentColor();
    color.setHsv(color.hsvHue() >= 0 ? color.hsvHue() : 0, color.hsvSaturation(),
                 static_cast<int>(sliderValue * 2.55));
    wheel->setCurrentColor(color);
    updatePreview();
    if (valueChangedCallback) valueChangedCallback(GetValue());
  });
  updatePreview();
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
  const QColor color = QColor::fromRgbF(list[0].toDouble(), list[1].toDouble(), list[2].toDouble());
  const QSignalBlocker blocker(valueSlider);
  valueSlider->setValue(static_cast<int>(color.valueF() * 100.0));
  wheel->setCurrentColor(color);
  updatePreview();
}

QVariant InspectColorFieldWidget::GetValue() const
{
  const QColor color = wheel->currentColor();
  return QVariantList{color.redF(), color.greenF(), color.blueF()};
}

void InspectColorFieldWidget::updatePreview()
{
  const QColor color = wheel->currentColor();
  preview->setStyleSheet(QStringLiteral("background-color: rgb(%1, %2, %3);")
                           .arg(color.red()).arg(color.green()).arg(color.blue()));
}