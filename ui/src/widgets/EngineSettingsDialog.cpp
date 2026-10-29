#include "widgets/EngineSettingsDialog.h"
#include <QVBoxLayout>
#include "widgets/InspectorWidget.h"

EngineSettingsDialog::EngineSettingsDialog(QWidget *parent)
  : QDialog(parent)
{
  setObjectName("engineSettingsDialog");
  setWindowTitle(tr("Engine Settings"));
  setAttribute(Qt::WA_StyledBackground, true);
  resize(360, 280);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);

  inspector = new InspectorWidget(this);
  inspector->setSettingsKey("engineSettings");
  layout->addWidget(inspector, 1);
}

void EngineSettingsDialog::refreshLayoutMetrics() { inspector->refreshLayoutMetrics(); }
