#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

// Contents of a "dmri" dataset's fodf/status.json.
struct DmriFodfStatus
{
  bool available = false;              // the fODF SH, maps and streamlines were computed
  std::string unavailableReason;       // why not; empty when available
  float absoluteAmplitudeCutoff = 0.0f; // peak/tracking cutoff the maps were built with; 0 when unavailable
};

// The single definitions of the "dmri" format's small JSON payloads, shared by its writer and
// reader so the two cannot drift. The deserializers throw std::runtime_error on malformed input.
std::string SerializeSeedPointsJson(const std::vector<glm::vec3> &seedPoints);   // dump(2)
std::vector<glm::vec3> DeserializeSeedPointsJson(const std::string &jsonText);

std::string SerializeDmriFodfStatusJson(const DmriFodfStatus &status);           // dump(2)
DmriFodfStatus DeserializeDmriFodfStatusJson(const std::string &jsonText);
