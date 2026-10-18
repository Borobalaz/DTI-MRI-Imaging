#include "Settings/EngineSettingsCatalog.h"

#include "Settings/EngineSettings.h"
#include "Inspection/InspectField.h"

void RegisterEngineSettingsCatalog()
{
  EngineSettings::Declare(MakeInspectField(
      "postprocessing.enabled", "Enable Postprocessing", "Postprocessing",
      InspectFieldType::Boolean, true));
}
