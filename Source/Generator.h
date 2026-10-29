#pragma once

#include "Model.h"

namespace drums
{
// Генерирует паттерн на 4 такта для выбранных стиля, части песни и размера.
// Для Djent и Metalcore в 4/4 основой служат фразы из присланных MIDI-грувов,
// для остальных стилей и размеров работает процедурный генератор.
Pattern generate (Style style, Part part, int num, int den, juce::Random& rng);

// Заново придумывает одну дорожку, не трогая остальные (учитывает текущий кик/снейр).
Pattern regenerateLane (const Pattern& current, Lane lane, juce::Random& rng);

// Берёт base и заменяет в нём все удары дорожки lane ударами из source.
Pattern withLane (const Pattern& base, const Pattern& source, Lane lane);

// Новый паттерн fresh, но заблокированные дорожки остаются как в old.
Pattern mergeLocked (const Pattern& fresh, const Pattern& old, const std::array<bool, (size_t) kNumLanes>& locked);

void sortHits (std::vector<Hit>& hits);
} // namespace drums
