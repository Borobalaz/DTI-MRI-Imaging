#pragma once

#include <functional>

#include <QFrame>
#include <QString>

class QToolButton;
class QVBoxLayout;
class QWidget;

// One collapsible group "card" in an InspectorWidget: a full-width header button (chevron + group
//  name) above a body that holds the group's field rows. Styled from InspectorWidget.qss via the
//  object names inspectorGroupCard/inspectorGroupHeader/inspectorGroupBody and the header's
//  "expanded" dynamic property.
class InspectGroupSection : public QFrame
{
public:
  explicit InspectGroupSection(const QString &title, QWidget *parent = nullptr);

  void setExpanded(bool expanded);
  bool isExpanded() const;
  QVBoxLayout *bodyLayout() const;

  // Fired only when the user toggles the header, not by setExpanded().
  std::function<void(bool expanded)> expandedChangedCallback;

private:
  QToolButton *header = nullptr;
  QWidget *body = nullptr;
  QVBoxLayout *bodyContentLayout = nullptr;
};
