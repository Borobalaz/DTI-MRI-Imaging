#include "widgets/InspectGroupSection.h"

#include <QIcon>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include "styles/StyleSheetComposer.h"

InspectGroupSection::InspectGroupSection(const QString &title, QWidget *parent)
  : QFrame(parent)
{
  setObjectName("inspectorGroupCard");

  // No layout margins: the card's 1px QSS border already shrinks the QFrame's contents rect.
  auto *cardLayout = new QVBoxLayout(this);
  cardLayout->setContentsMargins(0, 0, 0, 0);
  cardLayout->setSpacing(0);

  header = new QToolButton(this);
  header->setObjectName("inspectorGroupHeader");
  header->setText(title);
  header->setCheckable(true);
  // TextBesideIcon draws the chevron at the left edge with the text left-aligned after it.
  header->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  header->setIconSize(QSize(12, 12));
  header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  header->setFixedHeight(ResolveCommonToken("group-header-height").toInt());
  cardLayout->addWidget(header);

  body = new QWidget(this);
  body->setObjectName("inspectorGroupBody");
  bodyContentLayout = new QVBoxLayout(body);
  bodyContentLayout->setContentsMargins(0, 0, 0, 0);
  bodyContentLayout->setSpacing(0);
  cardLayout->addWidget(body);

  QObject::connect(header, &QToolButton::clicked, this, [this](bool checked)
  {
    setExpanded(checked);
    if (expandedChangedCallback) expandedChangedCallback(checked);
  });

  setExpanded(true);
}

void InspectGroupSection::setExpanded(bool expanded)
{
  body->setVisible(expanded);
  header->setChecked(expanded);
  header->setIcon(QIcon(expanded ? QStringLiteral(":/icons/chevron-down.svg")
                                 : QStringLiteral(":/icons/chevron-right.svg")));

  // Re-polish so QSS rules keyed on [expanded="false"] (rounded bottom corners, no bottom
  //  border on a header-only card) take effect immediately.
  header->setProperty("expanded", expanded);
  header->style()->unpolish(header);
  header->style()->polish(header);
}

bool InspectGroupSection::isExpanded() const { return header->isChecked(); }

QVBoxLayout *InspectGroupSection::bodyLayout() const { return bodyContentLayout; }
