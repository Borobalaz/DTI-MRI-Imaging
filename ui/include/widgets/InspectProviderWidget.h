#pragma once

#include <QFrame>
#include "qt-adapters/InspectObjectSummary.h"

class QLabel;
class QToolButton;

class InspectProviderWidget : public QFrame
{
  Q_OBJECT

public:
  explicit InspectProviderWidget(QWidget *parent = nullptr);

  void setObject(const InspectObjectSummary &newObject);
  void refreshFromObject();
  void setSelected(bool selected);
  std::string getName() const { return object.id; }

signals:
  void clicked(std::string providerId);
  void visibilityClicked(std::string providerId);

protected:
  void mousePressEvent(QMouseEvent *event) override;

private:
  void updateSelectionStyle();
  void updateVisibilityIcon();

  bool selected = false;

  InspectObjectSummary object;
  QLabel *nameLabel = nullptr;
  QToolButton *visibilityButton = nullptr;
};
