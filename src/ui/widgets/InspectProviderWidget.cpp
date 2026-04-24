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

  visibilityButton = new QToolButton(this);
  visibilityButton->setAutoRaise(true);
  visibilityButton->setIconSize(QSize(16, 16));
  visibilityButton->setCursor(Qt::PointingHandCursor);
  visibilityButton->setVisible(false);
  layout->addWidget(visibilityButton, 1, Qt::AlignVCenter);

  QObject::connect(visibilityButton, &QToolButton::clicked, this, [this]()
                   {
                     if (!provider)
                     {
                       return;
                     }
                     emit visibilityClicked(provider->GetInspectDisplayName());
                   });

  updateSelectionStyle();
}

void InspectProviderWidget::setProvider(InspectProvider *newProvider)
{
  provider = newProvider;
  refreshFromProvider();
}

void InspectProviderWidget::refreshFromProvider()
{
  if (!provider)
  {
    nameLabel->clear();
    visibilityButton->setVisible(false);
    return;
  }

  nameLabel->setText(QString::fromStdString(provider->GetInspectDisplayName()));

  const bool hasVisibility = provider->HasVisibility();
  visibilityButton->setVisible(hasVisibility);
  if (hasVisibility)
  {
    updateVisibilityIcon();
  }
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
  if (event && event->button() == Qt::LeftButton && provider)
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
}

void InspectProviderWidget::updateVisibilityIcon()
{
  if (!provider || !visibilityButton)
  {
    return;
  }

  const QIcon themeVisible = QIcon::fromTheme("view-visible");
  const QIcon themeHidden = QIcon::fromTheme("view-hidden");
  const QIcon fallbackVisible = style()->standardIcon(QStyle::SP_DialogYesButton);
  const QIcon fallbackHidden = style()->standardIcon(QStyle::SP_DialogNoButton);
  const bool isVisible = provider->IsVisible();

  if (isVisible)
  {
    visibilityButton->setIcon(themeVisible.isNull() ? fallbackVisible : themeVisible);
  }
  else
  {
    visibilityButton->setIcon(themeHidden.isNull() ? fallbackHidden : themeHidden);
  }
}
