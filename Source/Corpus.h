#pragma once

namespace drums
{
// Фраза из набора присланных MIDI-грувов (квантизация 1/16, размер 4/4).
// data: по 3 символа base64 на удар: шаг, голос (индекс Art), громкость/2.
struct CorpusPhrase
{
    unsigned char part;        // индекс Part
    unsigned char styleMask;   // 1 = Djent, 2 = Metalcore
    unsigned char bars;        // длина в тактах (4 для грувов, 1-2 для филлов)
    const char* data;
};

extern const CorpusPhrase kCorpus[];
extern const int kCorpusSize;
} // namespace drums
