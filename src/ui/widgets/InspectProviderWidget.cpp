#include "ui/widgets/InspectProviderWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QStyle>
#include <QToolButton>

InspectProviderWidget::InspectProviderWidget(QWidget *parent)
    : QFrame(parent)
{
  setObjectName("inspectProviderItem");
  setFrameShape(QFrame::NoFrame);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setMinimumHeight(30);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(8, 4, 8, 4);
  layout->setSpacing(8);

  // Add name
  nameLabel = new QLabel(this);
  nameLabel->setWordWrap(false);
  layout->addWidget(nameLabel, 0);

  layout->addStretch(1);

  // Add visibility toggle if the provider supports it
  if (provider->HasVisibility())
  {
    visibilityButton = new QToolButton(this);
    visibilityButton->setAutoRaise(true);
    visibilityButton->setIconSize(QSize(16, 16));
    visibilityButton->setCursor(Qt::PointingHandCursor);
    visibilityButton->setVisible(true);
    layout->addWidget(visibilityButton, 1, Qt::AlignVCenter);

    QObject::connect(visibilityButton, &QToolButton::clicked, this, [this]()
                     { emit visibilityClicked(provider->GetInspectDisplayName()); });
  }

  updateSelectionStyle();
}

/**
 * @brief Sets the selection state of the widget
 * 
 * @param isSelected 
 */
void InspectProviderWidget::setSelected(bool isSelected)
{
  selected = isSelected;
  updateSelectionStyle();
}

/**
 * @brief If the widget is clicked, emit the clicked signal with the row index
 * 
 * @param event 
 */
void InspectProviderWidget::mousePressEvent(QMouseEvent *event)
{
  if (event && event->button() == Qt::LeftButton)
  {
    emit clicked(provider->GetInspectDisplayName());
  }

  QFrame::mousePressEvent(event);
}

/**
 * @brief Updates the visual style of the widget based on its selection state
 * 
 */
void InspectProviderWidget::updateSelectionStyle()
{
  setProperty("selected", selected);
  style()->unpolish(this);
  style()->polish(this);
  update();

  if (selected)
  {
    setStyleSheet("QFrame#inspectProviderItem { background-color: rgba(95, 140, 255, 0.25); border-radius: 4px; }");
  }
  else
  {
    setStyleSheet("QFrame#inspectProviderItem { background-color: transparent; border-radius: 4px; }");
  }
}

void InspectProviderWidget::updateVisibilityIcon()
{
  const QIcon themeVisible = QIcon::fromTheme("view-visible");
  const QIcon themeHidden = QIcon::fromTheme("view-hidden");
  const QIcon fallbackVisible = style()->standardIcon(QStyle::SP_DialogYesButton);
  const QIcon fallbackHidden = style()->standardIcon(QStyle::SP_DialogNoButton);

  if (isVisible)
  {
    visibilityButton->setIcon(themeVisible.isNull() ? fallbackVisible : themeVisible);
  }
  else
  {
    visibilityButton->setIcon(themeHidden.isNull() ? fallbackHidden : themeHidden);
  }
}
