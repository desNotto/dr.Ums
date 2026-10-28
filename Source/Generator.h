#pragma once

#include "Model.h"

namespace drums
{
// Генерирует паттерн на 4 такта для выбранных стиля, части песни и размера.
Pattern generate (Style style, Part part, int num, int den, juce::Random& rng);

// Берёт base и заменяет в нём все удары дорожки lane ударами из source.
Pattern withLane (const Pattern& base, const Pattern& source, Lane lane);

// Новый паттерн fresh, но заблокированные дорожки остаются как в old.
Pattern mergeLocked (const Pattern& fresh, const Pattern& old, const std::array<bool, (size_t) kNumLanes>& locked);

void sortHits (std::vector<Hit>& hits);
} // namespace drums
