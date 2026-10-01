#pragma once

#include <functional>
#include <utility>

#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QString>
#include <QVariant>
#include <QWidget>

#include "widgets/inspect_fields/IInspectWidget.h"

namespace inspect_color_internal
{
  constexpr double kPi = 3.14159265358979323846;

  class ColorWheelWidget : public QWidget
  {
  public:
    explicit ColorWheelWidget(QWidget *parent = nullptr);
    void setCurrentColor(const QColor &color);
    QColor currentColor() const;

    std::function<void(const QColor &)> colorChangedCallback;

  protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

  private:
    void pickFromPosition(const QPoint &pos);

    int hue = 0;
    int saturation = 0;
    int value = 255;
  };
}

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
  void updatePreview();

  inspect_color_internal::ColorWheelWidget *wheel = nullptr;
  QSlider *valueSlider = nullptr;
  QLabel *preview = nullptr;
  QString fieldIdValue;
  QString displayNameValue;
  QString groupNameValue;
  bool readOnlyValue = false;
};
