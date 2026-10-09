#include "widgets/InspectProviderWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QStyle>
#include <QToolButton>

#include "styles/StyleSheetComposer.h"

InspectProviderWidget::InspectProviderWidget(QWidget *parent)
    : QFrame(parent)
{
  setObjectName("inspectProviderItem");
  setFrameShape(QFrame::NoFrame);
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setMinimumHeight(ResolveCommonToken("provider-item-min-height").toInt());

  auto *layout = new QHBoxLayout(this);
  const int marginH = ResolveCommonToken("provider-item-margin-h").toInt();
  const int marginV = ResolveCommonToken("provider-item-margin-v").toInt();
  layout->setContentsMargins(marginH, marginV, marginH, marginV);
  layout->setSpacing(ResolveCommonToken("provider-item-spacing").toInt());

  // Add name (elided to fit, with the full name as a tooltip so it's never fully lost)
  nameLabel = new QLabel(this);
  nameLabel->setWordWrap(false);
  nameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  layout->addWidget(nameLabel, 1);

  visibilityButton = new QToolButton(this);
  visibilityButton->setAutoRaise(true);
  visibilityButton->setIconSize(QSize(16, 16));
  visibilityButton->setCursor(Qt::PointingHandCursor);
  visibilityButton->setVisible(false);
  layout->addWidget(visibilityButton, 0, Qt::AlignVCenter);

  QObject::connect(visibilityButton, &QToolButton::clicked, this, [this]()
                   {
                     if (object.id.empty())
                     {
                       return;
                     }
                     emit visibilityClicked(object.id);
                   });

  updateSelectionStyle();
}

void InspectProviderWidget::setObject(const InspectObjectSummary &newObject)
{
  object = newObject;
  refreshFromObject();
}

void InspectProviderWidget::refreshFromObject()
{
  if (object.id.empty())
  {
    fullDisplayName.clear();
    nameLabel->clear();
    nameLabel->setToolTip(QString());
    visibilityButton->setVisible(false);
    return;
  }

  fullDisplayName = QString::fromStdString(object.displayName);
  nameLabel->setToolTip(fullDisplayName);
  updateNameLabelElision();

  visibilityButton->setVisible(object.hasVisibility);
  if (object.hasVisibility)
  {
    updateVisibilityIcon();
  }
}

/**
 * @brief Elide the display name to fit the label's current width, so a long object name
 *  shrinks the visibility button out of view instead of overflowing the row.
 */
void InspectProviderWidget::updateNameLabelElision()
{
  if (!nameLabel || fullDisplayName.isEmpty())
  {
    return;
  }

  nameLabel->setText(nameLabel->fontMetrics().elidedText(fullDisplayName, Qt::ElideRight, nameLabel->width()));
}

void InspectProviderWidget::resizeEvent(QResizeEvent *event)
{
  QFrame::resizeEvent(event);
  updateNameLabelElision();
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
  if (event && event->button() == Qt::LeftButton && !object.id.empty())
  {
    emit clicked(object.id);
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
  if (object.id.empty() || !visibilityButton)
  {
    return;
  }

  const QIcon themeVisible = QIcon::fromTheme("view-visible");
  const QIcon themeHidden = QIcon::fromTheme("view-hidden");
  const QIcon fallbackVisible = style()->standardIcon(QStyle::SP_DialogYesButton);
  const QIcon fallbackHidden = style()->standardIcon(QStyle::SP_DialogNoButton);
  const bool isVisible = object.isVisible;

  if (isVisible)
  {
    visibilityButton->setIcon(themeVisible.isNull() ? fallbackVisible : themeVisible);
  }
  else
  {
    visibilityButton->setIcon(themeHidden.isNull() ? fallbackHidden : themeHidden);
  }
}
