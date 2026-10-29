#include "widgets/inspect_fields/InspectSpinBox.h"

#include <algorithm>
#include <cmath>

#include <QGuiApplication>
#include <QKeyEvent>
#include <QWheelEvent>

namespace
{
  // One "notch" of a classic mouse wheel, in QWheelEvent::angleDelta() units (1/8 degree).
  constexpr int kWheelNotchDelta = 120;
}

InspectSpinBox::InspectSpinBox(QWidget *parent)
  : QDoubleSpinBox(parent)
{
  // StrongFocus (not Qt's default WheelFocus): click or Tab focuses the box, but hovering and
  //  scrolling never does - wheelEvent() relies on that to tell "edit" from "scroll the panel".
  setFocusPolicy(Qt::StrongFocus);
  setButtonSymbols(QAbstractSpinBox::NoButtons);
  // Commit typed text on Enter/focus-out rather than pushing every intermediate keystroke.
  setKeyboardTracking(false);
  // Enables the :hover QSS state that, together with :focus, shows whether the wheel is live.
  setAttribute(Qt::WA_Hover);
}

void InspectSpinBox::setBaseStep(double step)
{
  baseStep = step;
  setSingleStep(step);
  setDecimals(DecimalsForStep(step * 0.1));
}

double InspectSpinBox::StepMultiplier(Qt::KeyboardModifiers modifiers)
{
  if (modifiers & Qt::ShiftModifier) return 0.1;
  if (modifiers & Qt::ControlModifier) return 10.0;
  return 1.0;
}

int InspectSpinBox::DecimalsForStep(double step)
{
  if (!(step > 0.0)) return 2;
  const double digits = -std::log10(step);
  if (digits <= 0.0) return 0;
  return std::clamp(static_cast<int>(std::ceil(digits)), 0, 10);
}

void InspectSpinBox::stepBy(int steps)
{
  stepWithModifiers(steps, QGuiApplication::keyboardModifiers());
}

void InspectSpinBox::wheelEvent(QWheelEvent *event)
{
  if (!hasFocus())
  {
    // Let the event propagate to the enclosing QScrollArea so the panel scrolls instead.
    event->ignore();
    return;
  }

  // Some platforms turn Shift+wheel into a horizontal scroll, so fall back to the x delta.
  const QPoint angleDelta = event->angleDelta();
  wheelRemainder += angleDelta.y() != 0 ? angleDelta.y() : angleDelta.x();
  const int notches = wheelRemainder / kWheelNotchDelta;
  wheelRemainder -= notches * kWheelNotchDelta;
  if (notches != 0)
  {
    stepWithModifiers(notches, event->modifiers());
  }

  // Deliberately not calling the base implementation: Qt's own step modifier
  //  (SH_SpinBox_StepModifier, Ctrl by default) would double-apply the x10.
  event->accept();
}

void InspectSpinBox::keyPressEvent(QKeyEvent *event)
{
  // QAbstractSpinBox multiplies Up/Down by 10 for its own step modifier before calling stepBy(),
  //  which would compound with StepMultiplier(); step them here so only one multiplier applies.
  const bool isUp = event->key() == Qt::Key_Up;
  const bool isDown = event->key() == Qt::Key_Down;
  if ((isUp || isDown) && !isReadOnly())
  {
    stepWithModifiers(isUp ? 1 : -1, event->modifiers());
    event->accept();
    return;
  }

  QDoubleSpinBox::keyPressEvent(event);
}

void InspectSpinBox::stepWithModifiers(int steps, Qt::KeyboardModifiers modifiers)
{
  // Scaling singleStep for just this call (instead of setValue(value() + ...)) keeps the base
  //  class's handling of not-yet-committed typed text, range clamping and select-all.
  setSingleStep(baseStep * StepMultiplier(modifiers));
  QDoubleSpinBox::stepBy(steps);
  setSingleStep(baseStep);
}
