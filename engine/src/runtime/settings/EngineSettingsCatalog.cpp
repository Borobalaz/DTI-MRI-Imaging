#include "runtime/settings/EngineSettingsCatalog.h"

#include "runtime/settings/EngineSettings.h"
#include "inspection/InspectField.h"

void RegisterEngineSettingsCatalog()
{
  EngineSettings::Declare(MakeInspectField(
      "postprocessing.enabled", "Enable Postprocessing", "Postprocessing",
      InspectFieldType::Boolean, true));

  // Engine-thread tick pacing in threaded mode (Engine::Start); 0 = uncapped.
  InspectFieldPtr maxFramerate = MakeInspectField(
      "engine.maxFramerate", "Max Framerate", "Performance",
      InspectFieldType::Number, 60.0);
  maxFramerate->minimum = 0.0;
  maxFramerate->maximum = 1000.0;
  maxFramerate->step = 1.0;
  EngineSettings::Declare(maxFramerate);
}
