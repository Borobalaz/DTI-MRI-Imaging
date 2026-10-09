#pragma once

#include <QFrame>
#include <QString>
#include "qt-adapters/InspectObjectSummary.h"

class QLabel;
class QToolButton;
class QResizeEvent;

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
  void resizeEvent(QResizeEvent *event) override;

private:
  void updateSelectionStyle();
  void updateVisibilityIcon();
  void updateNameLabelElision();

  bool selected = false;

  InspectObjectSummary object;
  QString fullDisplayName;
  QLabel *nameLabel = nullptr;
  QToolButton *visibilityButton = nullptr;
};
