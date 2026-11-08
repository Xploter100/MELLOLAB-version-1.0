#pragma once
#include "Common.h"

namespace ml
{
// Sonidos incluidos: se SINTETIZAN por codigo (no son grabaciones de cintas reales). Cada uno trae 3 zonas (DO3, DO4, DO5).
constexpr int kSynthCount = 6;
const char* synthName (int idx);
std::shared_ptr<Tape> synthTape (int idx);
}
