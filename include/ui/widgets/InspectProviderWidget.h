#pragma once

#include <QFrame>
#include "ui/widgets/inspect_fields/InspectProvider.h"

class QLabel;
class QToolButton;

class InspectProviderWidget : public QFrame
{
  Q_OBJECT

public:
  explicit InspectProviderWidget(QWidget *parent = nullptr);

  void setProvider(std::shared_ptr<InspectProvider> provider);
  void setSelected(bool selected);
  std::string getName() const { return provider ? provider->GetInspectDisplayName() : ""; }

signals:
  void clicked(std::string providerName);
  void visibilityClicked(std::string providerName);

protected:
  void mousePressEvent(QMouseEvent *event) override;

private:
  void updateSelectionStyle();
  void updateVisibilityIcon();

  bool selected = false;

  std::shared_ptr<InspectProvider> provider;
  QLabel *nameLabel = nullptr;
  QToolButton *visibilityButton = nullptr;
};
