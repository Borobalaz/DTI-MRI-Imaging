#include "widgets/DockTitleBarWidget.h"

#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QToolButton>

DockTitleBarWidget::DockTitleBarWidget(QDockWidget *dockWidget, QWidget *parent)
  : QWidget(parent)
  , dockWidget(dockWidget)
{
  setObjectName("dockTitleBar");
  setAttribute(Qt::WA_StyledBackground, true);

  titleLabel = new QLabel(dockWidget->windowTitle(), this);
  titleLabel->setObjectName("dockTitleLabel");

  floatButton = new QToolButton(this);
  floatButton->setObjectName("dockFloatButton");
  floatButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarNormalButton));
  floatButton->setToolTip(tr("Float"));
  floatButton->setAutoRaise(true);
  floatButton->setVisible(dockWidget->features().testFlag(QDockWidget::DockWidgetFloatable));
  QObject::connect(floatButton, &QToolButton::clicked, dockWidget, [dockWidget]()
  {
    dockWidget->setFloating(!dockWidget->isFloating());
  });

  closeButton = new QToolButton(this);
  closeButton->setObjectName("dockCloseButton");
  closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
  closeButton->setToolTip(tr("Close"));
  closeButton->setAutoRaise(true);
  closeButton->setVisible(dockWidget->features().testFlag(QDockWidget::DockWidgetClosable));
  QObject::connect(closeButton, &QToolButton::clicked, dockWidget, &QDockWidget::close);

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(8, 4, 4, 4);
  layout->setSpacing(2);
  layout->addWidget(titleLabel, 1);
  layout->addWidget(floatButton);
  layout->addWidget(closeButton);
}
