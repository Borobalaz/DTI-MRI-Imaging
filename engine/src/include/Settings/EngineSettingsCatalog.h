#pragma once

// Declares every EngineSettings::Declare(...) call for the whole engine in one place.
// Call once, early, before any subsystem could read a setting via EngineSettings::Get*.
void RegisterEngineSettingsCatalog();
