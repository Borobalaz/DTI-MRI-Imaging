#pragma once

#include <string>

#include "Preprocessing/FodfResponseFunction.h"

// The single definition of response.json's shape, shared by the fodf writer and reader so the
// two cannot drift (same role as DtiChannelTable for DTI channels).
std::string SerializeFodfResponseJson(const FodfResponseFunction& response);    // dump(2)
FodfResponseFunction DeserializeFodfResponseJson(const std::string& jsonText);  // throws std::runtime_error
