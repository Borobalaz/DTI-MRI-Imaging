#include "widgets/InspectorWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>

#include <QObject>

#include "styles/StyleSheetComposer.h"
#include "widgets/InspectGroupSection.h"
#include "widgets/inspect_fields/IInspectWidget.h"

InspectorWidget::InspectorWidget(QWidget *parent)
  : QFrame(parent)
{
  inspectorPanelLayout = new QVBoxLayout(this);

  auto *scrollArea = new QScrollArea(this);
  scrollArea->setObjectName("inspectorScrollArea");
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  inspectorContent = new QWidget(scrollArea);
  inspectorContent->setObjectName("inspectorContent");
  inspectorLayout = new QVBoxLayout(inspectorContent);
  inspectorLayout->setContentsMargins(0, 0, 0, 0);
  inspectorLayout->addStretch(1);

  scrollArea->setWidget(inspectorContent);
  inspectorPanelLayout->addWidget(scrollArea, 1);

  // After inspectorLayout exists: it receives the group-card spacing.
  refreshLayoutMetrics();
}

void InspectorWidget::refreshLayoutMetrics()
{
  setMinimumWidth(ResolveCommonToken("inspector-min-width").toInt());

  const int margin = ResolveCommonToken("panel-margin").toInt();
  inspectorPanelLayout->setContentsMargins(margin, margin, margin, margin);
  inspectorPanelLayout->setSpacing(ResolveCommonToken("panel-spacing").toInt());
  inspectorLayout->setSpacing(ResolveCommonToken("group-card-spacing").toInt());
}

void InspectorWidget::setEmptyText(const QString &text)
{
  emptyText = text;
}

void InspectorWidget::setSettingsKey(const QString &key)
{
  settingsKey = key;
  // Entries cached under a previous key (or none) would otherwise shadow this key's saved state.
  collapsedGroups.clear();
}

void InspectorWidget::setFields(const QObjectList &fieldObjects)
{
  editorBindings.clear();
  clearInspector();

  // Grouping is run-based: consecutive fields sharing a group name go into one section, in the
  //  order the provider lists them (no re-ordering).
  QString currentGroup;
  InspectGroupSection *currentSection = nullptr;
  bool hasEditors = false;
  for (QObject *fieldObject : fieldObjects)
  {
    auto *field = dynamic_cast<IInspectWidget *>(fieldObject);
    if (!field)
    {
      continue;
    }

    if (field->fieldId() == "visible" || field->fieldId() == "isVisible")
    {
      continue;
    }

    if (!currentSection || field->groupName() != currentGroup)
    {
      currentGroup = field->groupName();
      currentSection = addGroupSection(currentGroup);
    }

    addFieldEditor(field, currentSection->bodyLayout());
    hasEditors = true;
  }

  if (!hasEditors && !emptyText.isEmpty())
  {
    auto *emptyLabel = new QLabel(emptyText, inspectorContent);
    emptyLabel->setObjectName("inspectorEmptyLabel");
    emptyLabel->setWordWrap(true);
    emptyLabel->setAlignment(Qt::AlignCenter);
    // Color, font style and padding come from QLabel#inspectorEmptyLabel in
    //  ui/styles/widgets/InspectorWidget.qss.
    inspectorLayout->addWidget(emptyLabel);
  }

  inspectorLayout->addStretch(1);
  refreshBoundEditors();
}

void InspectorWidget::refreshBoundEditors()
{
  isApplyingEditorState = true;
  for (const EditorBinding &binding : editorBindings)
  {
    if (!binding.field || !binding.updateEditor)
    {
      continue;
    }

    auto *field = dynamic_cast<IInspectWidget *>(binding.field.data());
    if (field)
    {
      binding.updateEditor(field->GetValue());
    }
  }
  isApplyingEditorState = false;
}

void InspectorWidget::clearInspector()
{
  if (!inspectorLayout)
  {
    return;
  }

  while (QLayoutItem *item = inspectorLayout->takeAt(0))
  {
    if (QWidget *widget = item->widget())
    {
      widget->deleteLater();
    }
    delete item;
  }
}

InspectGroupSection *InspectorWidget::addGroupSection(const QString &groupName)
{
  const QString title = groupName.isEmpty() ? tr("General") : groupName;

  auto *section = new InspectGroupSection(title, inspectorContent);
  section->setExpanded(!isGroupCollapsed(title));
  section->expandedChangedCallback = [this, title](bool expanded)
  {
    setGroupCollapsed(title, !expanded);
  };

  inspectorLayout->addWidget(section);
  return section;
}

bool InspectorWidget::isGroupCollapsed(const QString &groupTitle)
{
  const auto cached = collapsedGroups.constFind(groupTitle);
  if (cached != collapsedGroups.constEnd())
  {
    return cached.value();
  }

  // Loaded lazily, on first sight of a group name, since the set of groups is only known once
  //  a provider's fields arrive.
  const bool collapsed = !settingsKey.isEmpty()
    && QSettings().value(groupSettingsPath(groupTitle), false).toBool();
  collapsedGroups.insert(groupTitle, collapsed);
  return collapsed;
}

void InspectorWidget::setGroupCollapsed(const QString &groupTitle, bool collapsed)
{
  collapsedGroups.insert(groupTitle, collapsed);
  if (!settingsKey.isEmpty())
  {
    QSettings().setValue(groupSettingsPath(groupTitle), collapsed);
  }
}

QString InspectorWidget::groupSettingsPath(const QString &groupTitle) const
{
  return QStringLiteral("inspectorGroups/%1/%2/collapsed").arg(settingsKey, groupTitle);
}

void InspectorWidget::addFieldEditor(IInspectWidget *field, QVBoxLayout *targetLayout)
{
  // A QFrame (not a plain QWidget) so the QSS divider border renders reliably.
  auto *row = new QFrame(targetLayout->parentWidget());
  row->setObjectName("inspectorFieldRow");
  // QSS has no :first-child for widgets; this lets the first row of a card drop its top divider.
  row->setProperty("firstInGroup", targetLayout->count() == 0);
  // Disabling the row (rather than just the editor) also dims the name label via the
  //  palette's disabled color group, matching read-only fields visually, not just functionally.
  row->setEnabled(!field->isReadOnly());

  auto *rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(ResolveCommonToken("field-row-margin-h").toInt(),
                                 ResolveCommonToken("field-row-margin-v").toInt(),
                                 ResolveCommonToken("field-row-margin-h").toInt(),
                                 ResolveCommonToken("field-row-margin-v").toInt());
  rowLayout->setSpacing(ResolveCommonToken("field-row-spacing").toInt());

  auto *nameLabel = new QLabel(field->displayName(), row);
  nameLabel->setObjectName("inspectorFieldLabel");
  const int labelWidth = ResolveCommonToken("field-label-width").toInt();
  nameLabel->setMinimumWidth(labelWidth);
  nameLabel->setMaximumWidth(labelWidth);
  nameLabel->setWordWrap(true);
  rowLayout->addWidget(nameLabel);

  IInspectWidget *editor = field->addToLayout(rowLayout);
  if (editor)
  {
    editorBindings.push_back({dynamic_cast<QObject *>(field), [editor](const QVariant &value)
    {
      editor->SetValue(value);
    }});
  }
  else
  {
    auto *unsupported = new QLabel("Unsupported field", row);
    unsupported->setWordWrap(true);
    rowLayout->addWidget(unsupported, 1);
  }

  targetLayout->addWidget(row);
}

