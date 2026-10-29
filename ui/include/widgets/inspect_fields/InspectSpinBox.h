#pragma once

#include <QDoubleSpinBox>

class QKeyEvent;
class QWheelEvent;

// Numeric editor shared by the inspect field widgets. The wheel only edits the value while the
//  box has keyboard focus (otherwise the event reaches the enclosing scroll area, so scrolling a
//  panel never changes a field by accident), and wheel/arrow-key steps are scaled by modifiers:
//  Shift x0.1 (fine), Ctrl x10 (coarse).
class InspectSpinBox : public QDoubleSpinBox
{
public:
  explicit InspectSpinBox(QWidget *parent = nullptr);

  // Sets the unmodified step and enough decimals that a Shift (x0.1) step is never rounded away.
  void setBaseStep(double step);

  void stepBy(int steps) override;

  // Shift -> 0.1, Ctrl -> 10.0, otherwise 1.0; Shift wins when both are held.
  static double StepMultiplier(Qt::KeyboardModifiers modifiers);
  static int DecimalsForStep(double step);

protected:
  void wheelEvent(QWheelEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  void stepWithModifiers(int steps, Qt::KeyboardModifiers modifiers);

  double baseStep = 1.0;
  int wheelRemainder = 0; // Accumulated angle delta below one 120-unit notch (high-resolution touchpads)
};
