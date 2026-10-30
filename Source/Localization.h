#pragma once

#include <juce_core/juce_core.h>
#include <atomic>

// Язык интерфейса (русский или английский). Выбор хранится в настройках и общий для всех копий плагина.
namespace loc
{
enum class Lang { Ru = 0, En = 1 };

inline std::atomic<int>& langRef()
{
    static std::atomic<int> v { 0 };
    return v;
}

inline Lang get()           { return (Lang) langRef().load(); }
inline void set (Lang l)    { langRef().store ((int) l); }
inline bool isRu()          { return get() == Lang::Ru; }

// Строка для текущего языка (оба варианта в UTF-8).
inline juce::String tr (const char* ru, const char* en)
{
    return juce::String::fromUTF8 (isRu() ? ru : en);
}
} // namespace loc
