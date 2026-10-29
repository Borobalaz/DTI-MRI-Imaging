#pragma once

#include <functional>

#include <QColor>
#include <QFrame>

class QKeyEvent;
class QLineEdit;
class QToolButton;
class InspectSpinBox;

namespace color_picker_internal
{
  class SaturationValueSquare;
  class HueStrip;
}

// Popup color editor opened from an InspectColorFieldWidget swatch: a saturation/value square,
//  a hue strip, hex and RGB inputs, and old/new swatches. Every edit is reported live through
//  colorChangedCallback; Esc reverts to the color the popup was opened with, while Enter or a
//  click outside (Qt::Popup's auto-close) keeps the current one. Deletes itself on close.
class ColorPickerPopup : public QFrame
{
public:
  explicit ColorPickerPopup(QWidget *parent = nullptr);
  ~ColorPickerPopup() override;

  // Records `initial` as the revert color, places the popup below `anchor` (above it if it
  //  would leave the anchor's screen, clamped horizontally) and shows it.
  void open(QWidget *anchor, const QColor &initial);

  // Updates the editors to an externally changed color without firing colorChangedCallback.
  void setColor(const QColor &color);

  // Fills a swatch widget with `color` (border/radius come from QSS); shared with the field row.
  static void SetSwatchFill(QWidget *swatch, const QColor &color);

  std::function<void(const QColor &)> colorChangedCallback;

protected:
  void keyPressEvent(QKeyEvent *event) override;

private:
  QColor currentColor() const;
  void adoptColor(const QColor &color);
  void syncEditors();
  void notifyColorChanged();
  void applyEditedColor(const QColor &color);
  void applyHexText();
  void applyRgbSpins();
  void revertToOriginal();
  void moveNextTo(QWidget *anchor);

  // HSV is the source of truth (not a QColor) so the hue survives greys and the saturation
  //  survives black, where a QColor round-trip would lose them.
  float hue = 0.0f; // 0-360
  float sat = 0.0f; // 0-1
  float val = 0.0f; // 0-1
  QColor originalColor;
  bool syncing = false; // Set while syncEditors() pushes state into the sub-editors

  color_picker_internal::SaturationValueSquare *saturationValueSquare = nullptr;
  color_picker_internal::HueStrip *hueStrip = nullptr;
  QLineEdit *hexEdit = nullptr;
  InspectSpinBox *redSpin = nullptr;
  InspectSpinBox *greenSpin = nullptr;
  InspectSpinBox *blueSpin = nullptr;
  QToolButton *oldSwatch = nullptr;
  QToolButton *newSwatch = nullptr;
};
