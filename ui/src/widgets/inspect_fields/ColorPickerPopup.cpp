#include "widgets/inspect_fields/ColorPickerPopup.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>

#include "styles/StyleSheetComposer.h"
#include "widgets/inspect_fields/InspectSpinBox.h"

namespace color_picker_internal
{
  constexpr int kPickerSide = 160;
  constexpr int kHueStripWidth = 16;
  constexpr float kKeyboardNudge = 0.01f;

  // QColor's HSV hue fraction must stay below 1.0 to mean "red" rather than wrap ambiguously.
  float HueFraction(float hueDegrees)
  {
    return std::clamp(hueDegrees, 0.0f, 359.99f) / 360.0f;
  }

  // Saturation (x) / value (y) picker for a fixed hue.
  class SaturationValueSquare : public QWidget
  {
  public:
    explicit SaturationValueSquare(QWidget *parent)
      : QWidget(parent)
    {
      setFixedSize(kPickerSide, kPickerSide);
      setFocusPolicy(Qt::StrongFocus);
      setCursor(Qt::CrossCursor);
    }

    void setHue(float hueDegrees)
    {
      hue = hueDegrees;
      update();
    }

    void setSaturationValue(float saturation, float value)
    {
      sat = saturation;
      val = value;
      update();
    }

    std::function<void(float saturation, float value)> pickedCallback;

  protected:
    void paintEvent(QPaintEvent *) override
    {
      QPainter painter(this);
      const QRectF area = rect();

      // Pure hue, washed toward white on the left and darkened toward black at the bottom -
      //  two gradient fills instead of a per-pixel HSV loop.
      painter.fillRect(area, QColor::fromHsvF(HueFraction(hue), 1.0f, 1.0f));

      QLinearGradient whiteToClear(area.topLeft(), area.topRight());
      whiteToClear.setColorAt(0.0, QColor(255, 255, 255, 255));
      whiteToClear.setColorAt(1.0, QColor(255, 255, 255, 0));
      painter.fillRect(area, whiteToClear);

      QLinearGradient clearToBlack(area.topLeft(), area.bottomLeft());
      clearToBlack.setColorAt(0.0, QColor(0, 0, 0, 0));
      clearToBlack.setColorAt(1.0, QColor(0, 0, 0, 255));
      painter.fillRect(area, clearToBlack);

      // Black outer + white inner ring stays visible on both light and dark regions.
      const QPointF marker(sat * (width() - 1), (1.0f - val) * (height() - 1));
      painter.setRenderHint(QPainter::Antialiasing, true);
      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen(Qt::black, 1.5));
      painter.drawEllipse(marker, 6.0, 6.0);
      painter.setPen(QPen(Qt::white, 1.5));
      painter.drawEllipse(marker, 4.5, 4.5);
    }

    void mousePressEvent(QMouseEvent *event) override
    {
      setFocus(Qt::MouseFocusReason);
      pickAt(event->position());
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
      if (event->buttons() & Qt::LeftButton) pickAt(event->position());
    }

    void keyPressEvent(QKeyEvent *event) override
    {
      switch (event->key())
      {
        case Qt::Key_Left: pick(sat - kKeyboardNudge, val); break;
        case Qt::Key_Right: pick(sat + kKeyboardNudge, val); break;
        case Qt::Key_Up: pick(sat, val + kKeyboardNudge); break;
        case Qt::Key_Down: pick(sat, val - kKeyboardNudge); break;
        default: QWidget::keyPressEvent(event); return; // Esc/Enter propagate to the popup
      }
      event->accept();
    }

  private:
    void pickAt(const QPointF &pos)
    {
      pick(static_cast<float>(pos.x() / (width() - 1)), static_cast<float>(1.0 - pos.y() / (height() - 1)));
    }

    void pick(float saturation, float value)
    {
      sat = std::clamp(saturation, 0.0f, 1.0f);
      val = std::clamp(value, 0.0f, 1.0f);
      update();
      if (pickedCallback) pickedCallback(sat, val);
    }

    float hue = 0.0f;
    float sat = 0.0f;
    float val = 0.0f;
  };

  // Vertical hue bar, 0 degrees at the top to 360 at the bottom.
  class HueStrip : public QWidget
  {
  public:
    explicit HueStrip(QWidget *parent)
      : QWidget(parent)
    {
      setFixedSize(kHueStripWidth, kPickerSide);
      setCursor(Qt::PointingHandCursor);
    }

    void setHue(float hueDegrees)
    {
      hue = hueDegrees;
      update();
    }

    std::function<void(float hueDegrees)> hueChangedCallback;

  protected:
    void paintEvent(QPaintEvent *) override
    {
      QPainter painter(this);
      const QRectF area = rect();

      // Red, yellow, green, cyan, blue, magenta, back to red.
      QLinearGradient hueGradient(area.topLeft(), area.bottomLeft());
      for (int stop = 0; stop <= 6; ++stop)
      {
        hueGradient.setColorAt(stop / 6.0, QColor::fromHsv((stop * 60) % 360, 255, 255));
      }
      painter.fillRect(area, hueGradient);

      const qreal markerY = hue / 360.0f * (height() - 1);
      painter.setBrush(Qt::NoBrush);
      painter.setPen(QPen(Qt::black, 1));
      painter.drawRect(QRectF(0.5, markerY - 2.5, width() - 1.0, 5.0));
      painter.setPen(QPen(Qt::white, 1));
      painter.drawRect(QRectF(1.5, markerY - 1.5, width() - 3.0, 3.0));
    }

    void mousePressEvent(QMouseEvent *event) override { pickAt(event->position().y()); }

    void mouseMoveEvent(QMouseEvent *event) override
    {
      if (event->buttons() & Qt::LeftButton) pickAt(event->position().y());
    }

  private:
    void pickAt(qreal y)
    {
      hue = std::clamp(static_cast<float>(y / (height() - 1)), 0.0f, 1.0f) * 360.0f;
      update();
      if (hueChangedCallback) hueChangedCallback(hue);
    }

    float hue = 0.0f;
  };
}

namespace
{
  constexpr int kAnchorGap = 4;

  QToolButton *MakeSwatch(const QString &objectName, const QString &toolTip, QWidget *parent)
  {
    auto *swatch = new QToolButton(parent);
    swatch->setObjectName(objectName);
    swatch->setToolTip(toolTip);
    swatch->setFocusPolicy(Qt::NoFocus);
    swatch->setFixedSize(ResolveCommonToken("color-swatch-width").toInt(),
                         ResolveCommonToken("color-swatch-height").toInt());
    return swatch;
  }

  InspectSpinBox *MakeChannelSpin(QWidget *parent)
  {
    auto *spin = new InspectSpinBox(parent);
    spin->setRange(0.0, 255.0);
    spin->setBaseStep(1.0);
    spin->setDecimals(0); // Whole 8-bit channel values, overriding setBaseStep's fine-step decimals
    return spin;
  }
}

ColorPickerPopup::ColorPickerPopup(QWidget *parent)
  : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint)
{
  setObjectName("colorPickerPopup");
  setAttribute(Qt::WA_StyledBackground, true);
  setAttribute(Qt::WA_DeleteOnClose, true);
  // Leaves the window corners outside QFrame#colorPickerPopup's border-radius unpainted
  //  instead of square.
  setAttribute(Qt::WA_TranslucentBackground, true);

  // No layout margins: the inset comes from QFrame#colorPickerPopup's QSS padding.
  auto *popupLayout = new QVBoxLayout(this);
  popupLayout->setContentsMargins(0, 0, 0, 0);
  popupLayout->setSpacing(8);

  auto *pickerRow = new QHBoxLayout;
  pickerRow->setSpacing(8);
  saturationValueSquare = new color_picker_internal::SaturationValueSquare(this);
  hueStrip = new color_picker_internal::HueStrip(this);
  pickerRow->addWidget(saturationValueSquare);
  pickerRow->addWidget(hueStrip);
  popupLayout->addLayout(pickerRow);

  auto *swatchRow = new QHBoxLayout;
  swatchRow->setSpacing(4);
  oldSwatch = MakeSwatch("colorPickerOldSwatch", tr("Original color - click to revert"), this);
  newSwatch = MakeSwatch("colorPickerNewSwatch", tr("New color"), this);
  hexEdit = new QLineEdit(this);
  hexEdit->setObjectName("colorPickerHexEdit");
  hexEdit->setMaxLength(7);
  hexEdit->setValidator(new QRegularExpressionValidator(
    QRegularExpression(QStringLiteral("#?[0-9A-Fa-f]{6}")), hexEdit));
  swatchRow->addWidget(oldSwatch);
  swatchRow->addWidget(newSwatch);
  swatchRow->addStretch(1);
  swatchRow->addWidget(hexEdit);
  popupLayout->addLayout(swatchRow);

  auto *rgbRow = new QHBoxLayout;
  rgbRow->setSpacing(4);
  redSpin = MakeChannelSpin(this);
  greenSpin = MakeChannelSpin(this);
  blueSpin = MakeChannelSpin(this);
  const std::pair<QString, InspectSpinBox *> channels[] = {
    {QStringLiteral("R"), redSpin}, {QStringLiteral("G"), greenSpin}, {QStringLiteral("B"), blueSpin}};
  for (const auto &[channelName, spin] : channels)
  {
    rgbRow->addWidget(new QLabel(channelName, this));
    rgbRow->addWidget(spin, 1);
    QObject::connect(spin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double)
    {
      applyRgbSpins();
    });
  }
  popupLayout->addLayout(rgbRow);

  saturationValueSquare->pickedCallback = [this](float saturation, float value)
  {
    sat = saturation;
    val = value;
    syncEditors();
    notifyColorChanged();
  };
  hueStrip->hueChangedCallback = [this](float hueDegrees)
  {
    hue = hueDegrees;
    syncEditors();
    notifyColorChanged();
  };
  QObject::connect(hexEdit, &QLineEdit::editingFinished, this, [this]()
  {
    applyHexText();
  });
  QObject::connect(oldSwatch, &QToolButton::clicked, this, [this]()
  {
    revertToOriginal();
  });
}

ColorPickerPopup::~ColorPickerPopup()
{
  // QWidget's destructor can still move focus out of the hex/RGB editors, whose
  //  editingFinished/valueChanged would then call into this already-destroyed subclass.
  for (QObject *editor : std::initializer_list<QObject *>{hexEdit, redSpin, greenSpin, blueSpin})
  {
    editor->blockSignals(true);
  }
}

void ColorPickerPopup::open(QWidget *anchor, const QColor &initial)
{
  originalColor = initial;
  SetSwatchFill(oldSwatch, initial);
  setColor(initial);

  // Polish first so the QSS padding/border are part of the size used for placement.
  ensurePolished();
  adjustSize();
  moveNextTo(anchor);
  show();
  saturationValueSquare->setFocus(Qt::PopupFocusReason);
}

void ColorPickerPopup::setColor(const QColor &color)
{
  adoptColor(color);
  syncEditors();
}

void ColorPickerPopup::SetSwatchFill(QWidget *swatch, const QColor &color)
{
  swatch->setStyleSheet(QStringLiteral("background-color: %1;").arg(color.name(QColor::HexRgb)));
}

void ColorPickerPopup::keyPressEvent(QKeyEvent *event)
{
  switch (event->key())
  {
    case Qt::Key_Escape:
      revertToOriginal();
      close();
      return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
      // A focused hex/RGB editor has already committed its text before ignoring the key.
      close();
      return;
    default:
      QFrame::keyPressEvent(event);
  }
}

QColor ColorPickerPopup::currentColor() const
{
  return QColor::fromHsvF(color_picker_internal::HueFraction(hue), sat, val);
}

void ColorPickerPopup::adoptColor(const QColor &color)
{
  float colorHue = 0.0f;
  float colorSat = 0.0f;
  float colorVal = 0.0f;
  color.getHsvF(&colorHue, &colorSat, &colorVal);

  // Hue is undefined (-1) for greys and saturation is meaningless for black; keep the previous
  //  values so the square/strip stay where the user left them.
  if (colorHue >= 0.0f) hue = colorHue * 360.0f;
  if (colorVal > 0.0f) sat = colorSat;
  val = colorVal;
}

void ColorPickerPopup::syncEditors()
{
  syncing = true;
  const QColor color = currentColor();
  saturationValueSquare->setHue(hue);
  saturationValueSquare->setSaturationValue(sat, val);
  hueStrip->setHue(hue);
  hexEdit->setText(color.name(QColor::HexRgb).toUpper());
  redSpin->setValue(color.red());
  greenSpin->setValue(color.green());
  blueSpin->setValue(color.blue());
  SetSwatchFill(newSwatch, color);
  syncing = false;
}

void ColorPickerPopup::notifyColorChanged()
{
  if (colorChangedCallback) colorChangedCallback(currentColor());
}

void ColorPickerPopup::applyEditedColor(const QColor &color)
{
  adoptColor(color);
  syncEditors();
  notifyColorChanged();
}

void ColorPickerPopup::applyHexText()
{
  if (syncing) return;

  QString text = hexEdit->text();
  if (!text.startsWith(QLatin1Char('#'))) text.prepend(QLatin1Char('#'));
  const QColor color(text);
  if (!color.isValid()) return;

  if (color.rgb() == currentColor().rgb())
  {
    // Same color (e.g. focus left without an edit): only normalize the text, don't re-derive HSV.
    syncEditors();
    return;
  }
  applyEditedColor(color);
}

void ColorPickerPopup::applyRgbSpins()
{
  if (syncing) return;
  applyEditedColor(QColor(qRound(redSpin->value()), qRound(greenSpin->value()), qRound(blueSpin->value())));
}

void ColorPickerPopup::revertToOriginal()
{
  applyEditedColor(originalColor);
}

void ColorPickerPopup::moveNextTo(QWidget *anchor)
{
  const QRect anchorRect(anchor->mapToGlobal(QPoint(0, 0)), anchor->size());
  const QRect screenRect = anchor->screen()->availableGeometry();
  const QSize popupSize = size();

  // Below the anchor, or above it when that would run off the bottom of the screen.
  int y = anchorRect.bottom() + 1 + kAnchorGap;
  if (y + popupSize.height() > screenRect.bottom() + 1)
  {
    y = anchorRect.top() - kAnchorGap - popupSize.height();
  }

  const int maxX = std::max(screenRect.left(), screenRect.right() + 1 - popupSize.width());
  const int x = std::clamp(anchorRect.left(), screenRect.left(), maxX);
  move(x, y);
}
