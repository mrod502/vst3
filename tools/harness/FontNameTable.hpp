#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace harness
{
/** The `name` table of an OpenType face, read out of the bytes that ship.

    JUCE exposes `Typeface::getName()` (nameID 1) and `getStyle()` (nameID 2) and nothing else, and that is
    not enough to say which face drew: four of Clash Display's six weights report `nameID 1` as their own
    weight-specific family and `nameID 2` as "Regular" (F-030), so a style-string identity claim is true of
    four different faces. `nameID 6` — the PostScript name — is distinct per face, and this is the only route
    to it from embedded data.

    A record this cannot decode answers empty rather than throwing: the leg's assertion on the name is the
    check, and it fails closed. */
struct FontNames
{
    std::string family;     ///< nameID 1
    std::string style;      ///< nameID 2
    std::string postScript; ///< nameID 6
};

[[nodiscard]] inline std::uint16_t be16 (const std::uint8_t* at) noexcept
{
    return static_cast<std::uint16_t> ((at[0] << 8) | at[1]);
}

[[nodiscard]] inline std::uint32_t be32 (const std::uint8_t* at) noexcept
{
    return (static_cast<std::uint32_t> (at[0]) << 24) | (static_cast<std::uint32_t> (at[1]) << 16)
           | (static_cast<std::uint32_t> (at[2]) << 8) | static_cast<std::uint32_t> (at[3]);
}

/** Appends one UTF-16 code unit as UTF-8. Surrogates are paired by the caller's sequential walk: a lone
    surrogate cannot occur in a name table, and appending the replacement character would not fail the leg. */
inline void appendUtf8 (std::string& into, std::uint16_t unit)
{
    if (unit < 0x80U)
    {
        into.push_back (static_cast<char> (unit));
        return;
    }

    if (unit < 0x800U)
    {
        into.push_back (static_cast<char> (0xC0U | (unit >> 6)));
        into.push_back (static_cast<char> (0x80U | (unit & 0x3FU)));
        return;
    }

    into.push_back (static_cast<char> (0xE0U | (unit >> 12)));
    into.push_back (static_cast<char> (0x80U | ((unit >> 6) & 0x3FU)));
    into.push_back (static_cast<char> (0x80U | (unit & 0x3FU)));
}

[[nodiscard]] inline std::string decodeName (const std::uint8_t* text, std::size_t length,
                                             std::uint16_t encodingID)
{
    std::string out;

    if (encodingID == 0)
    {
        out.assign (reinterpret_cast<const char*> (text), length);
        return out;
    }

    for (std::size_t i = 0; i + 1 < length; i += 2)
        appendUtf8 (out, be16 (text + i));

    return out;
}

/** Names a face calls itself, preferring the Windows platform's records the way a shaper would. */
[[nodiscard]] inline FontNames readFontNames (const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*> (data);
    FontNames names;

    if (bytes == nullptr || size < 12)
        return names;

    const std::size_t tableCount = be16 (bytes + 4);

    if (size < 12 + tableCount * 16)
        return names;

    std::size_t nameTable = 0;
    std::size_t nameLength = 0;

    for (std::size_t i = 0; i < tableCount; ++i)
    {
        const std::uint8_t* record = bytes + 12 + i * 16;

        if (record[0] == 'n' && record[1] == 'a' && record[2] == 'm' && record[3] == 'e')
        {
            nameTable = be32 (record + 8);
            nameLength = be32 (record + 12);
            break;
        }
    }

    if (nameTable == 0 || nameLength < 6 || nameTable + nameLength > size)
        return names;

    const std::uint8_t* table = bytes + nameTable;
    const std::size_t recordCount = be16 (table + 2);
    const std::size_t stringsAt = be16 (table + 4);

    for (std::size_t pass = 0; pass < 2; ++pass)
    {
        for (std::size_t i = 0; i < recordCount; ++i)
        {
            const std::uint8_t* record = table + 6 + i * 12;
            const std::size_t recordEnd = 6 + (i + 1) * 12;

            if (recordEnd > nameLength)
                return names;

            const std::uint16_t platformID = be16 (record);
            const std::uint16_t encodingID = be16 (record + 2);
            const std::uint16_t nameID = be16 (record + 6);
            const std::size_t length = be16 (record + 8);
            const std::size_t start = stringsAt + be16 (record + 10);

            if (start + length > nameLength)
                return names;

            const bool preferred = pass == 0 ? platformID == 3 : platformID != 3;
            if (! preferred)
                continue;

            std::string* slot = nameID == 1 ? &names.family
                            : nameID == 2 ? &names.style
                            : nameID == 6 ? &names.postScript
                                          : nullptr;

            if (slot != nullptr && slot->empty ())
                *slot = decodeName (table + start, length, encodingID == 0 ? 0 : 1);
        }
    }

    return names;
}

/** The figures the ramp's arithmetic is built from, read out of the tables that declare them.

    JUCE renders these but does not report them: `Font::getAscent()` is a line box the caller may have overridden,
    there is no cap-height accessor at all, and an advance read off a sized font is per line box unless the caller
    has pinned the pair — which is how the first version of the face row over-stated every digit advance by 11 %.
    This reads `head`, `hhea`, `OS/2` and `hmtx` instead, so the row can print the declared figures next to the
    rendered ones and a disagreement is visible rather than assumed away.

    A table this cannot find answers 0 for the fields it owns, and the leg's assertion on them fails closed. */
struct FaceMetrics
{
    std::uint32_t unitsPerEm{};
    float capPerEm{};          ///< OS/2 sCapHeight, 0 before OS/2 v2
    float xHeightPerEm{};      ///< OS/2 sxHeight, 0 before OS/2 v2
    float hheaAscentPerEm{};   ///< the face's own line box, before the ramp's override
    float hheaDescentPerEm{};  ///< negative, as the table declares it
    float digitAdvancePerEm[10]{};
};

/** The glyph a codepoint maps to in the face's Unicode cmap, or 0. Format 4 and format 12 only, which together
    are every cmap a desktop face ships without a legacy Mac platform record. */
[[nodiscard]] inline std::uint32_t glyphOf (const std::uint8_t* cmap, std::size_t cmapLength,
                                            std::uint32_t codepoint)
{
    if (cmapLength < 4 || codepoint > 0x10FFFFU)
        return 0;

    const std::uint16_t subtableCount = be16 (cmap + 2);
    const std::uint8_t* best = nullptr;
    std::size_t bestLength = 0;
    bool bestIsFormat12 = false;

    for (std::uint16_t i = 0; i < subtableCount; ++i)
    {
        /* A cmap encoding record is platformID, encodingID, offset — eight bytes, not the twelve a `>HHI`
           struct read would suggest once padding is imagined. */
        const std::uint8_t* record = cmap + 4 + 8 * i;
        if (4 + 8 * (static_cast<std::size_t> (i) + 1) > cmapLength)
            return 0;

        const std::uint16_t platformID = be16 (record);
        const std::uint32_t offset = be32 (record + 4);
        /* The record's offset is from the start of the `cmap` table, not of the file — reading the subtable's
           format at `bytes + offset` looks at the middle of some other table and finds nothing usable. */
        if (offset + 2 > cmapLength)
            continue;

        const std::uint16_t format = be16 (cmap + offset);
        const bool usable = (platformID == 3 || platformID == 0) && (format == 4 || format == 12);
        if (! usable)
            continue;

        const bool preferred = best == nullptr || (format == 12 && ! bestIsFormat12);
        if (preferred)
        {
            best = cmap + offset;
            bestLength = cmapLength - offset;
            bestIsFormat12 = format == 12;
        }
    }

    if (best == nullptr)
        return 0;

    if (bestIsFormat12)
    {
        if (bestLength < 16)
            return 0;
        const std::uint32_t groups = be32 (best + 12);
        for (std::uint32_t i = 0; i < groups; ++i)
        {
            const std::size_t at = 16 + 12 * static_cast<std::size_t> (i);
            if (at + 12 > bestLength)
                return 0;
            const std::uint32_t start = be32 (best + at);
            const std::uint32_t end = be32 (best + at + 4);
            const std::uint32_t glyph = be32 (best + at + 8);
            if (codepoint >= start && codepoint <= end)
                return glyph + (codepoint - start);
        }
        return 0;
    }

    if (bestLength < 14)
        return 0;

    const std::uint16_t segmentX2 = be16 (best + 6);
    const std::uint32_t segments = segmentX2 / 2U;
    /* endCode starts at 14 and the two-byte reservedPad that follows it shifts every later array along: reading
       startCode at 14 + segCountX2 answers with the pad, and every codepoint then looks absent. */
    const std::size_t ends = 14;
    const std::size_t starts = 16 + static_cast<std::size_t> (segmentX2);
    const std::size_t deltas = 16 + 2 * static_cast<std::size_t> (segmentX2);
    const std::size_t rangeOffsets = 16 + 3 * static_cast<std::size_t> (segmentX2);

    if (rangeOffsets + segmentX2 > bestLength)
        return 0;

    for (std::uint32_t s = 0; s < segments; ++s)
    {
        const std::size_t at = static_cast<std::size_t> (s) * 2U;
        const std::uint16_t end = be16 (best + ends + at);
        if (codepoint > end)
            continue;

        const std::uint16_t start = be16 (best + starts + at);
        if (codepoint < start)
            return 0;

        const std::int16_t delta = static_cast<std::int16_t> (be16 (best + deltas + at));
        const std::uint16_t rangeOffset = be16 (best + rangeOffsets + at);

        if (rangeOffset == 0)
            return static_cast<std::uint32_t> ((codepoint + static_cast<std::uint32_t> (delta)) & 0xFFFFU);

        const std::size_t glyphAt = rangeOffsets + at + rangeOffset + 2 * (codepoint - start);
        if (glyphAt + 2 > bestLength)
            return 0;

        const std::uint32_t glyph = be16 (best + glyphAt);
        if (glyph == 0)
            return 0;

        return static_cast<std::uint32_t> ((glyph + static_cast<std::uint32_t> (delta)) & 0xFFFFU);
    }

    return 0;
}

[[nodiscard]] inline FaceMetrics readFaceMetrics (const void* data, std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*> (data);
    FaceMetrics metrics;

    if (bytes == nullptr || size < 12)
        return metrics;

    const std::uint16_t tableCount = be16 (bytes + 4);
    if (size < 12 + static_cast<std::size_t> (tableCount) * 16)
        return metrics;

    const std::uint8_t* head = nullptr;
    const std::uint8_t* hhea = nullptr;
    const std::uint8_t* os2 = nullptr;
    const std::uint8_t* hmtx = nullptr;
    const std::uint8_t* cmap = nullptr;
    std::size_t cmapLength = 0;

    for (std::uint16_t i = 0; i < tableCount; ++i)
    {
        const std::uint8_t* record = bytes + 12 + static_cast<std::size_t> (i) * 16;
        const std::uint32_t offset = be32 (record + 8);
        const std::uint32_t length = be32 (record + 12);
        if (offset + length > size)
            return metrics;

        const char* tag = reinterpret_cast<const char *> (record);
        if (std::string (tag, 4) == "head") head = bytes + offset;
        else if (std::string (tag, 4) == "hhea") hhea = bytes + offset;
        else if (std::string (tag, 4) == "OS/2") os2 = bytes + offset;
        else if (std::string (tag, 4) == "hmtx") hmtx = bytes + offset;
        else if (std::string (tag, 4) == "cmap")
        {
            cmap = bytes + offset;
            cmapLength = length;
        }
    }

    if (head != nullptr)
        metrics.unitsPerEm = be16 (head + 18);

    if (metrics.unitsPerEm == 0)
        return metrics;

    if (os2 != nullptr && be16 (os2) >= 2)
    {
        metrics.capPerEm = static_cast<float> (static_cast<std::int16_t> (be16 (os2 + 88)))
                           / static_cast<float> (metrics.unitsPerEm);
        metrics.xHeightPerEm = static_cast<float> (static_cast<std::int16_t> (be16 (os2 + 86)))
                               / static_cast<float> (metrics.unitsPerEm);
    }

    if (hhea != nullptr)
    {
        metrics.hheaAscentPerEm = static_cast<float> (static_cast<std::int16_t> (be16 (hhea + 4)))
                                  / static_cast<float> (metrics.unitsPerEm);
        metrics.hheaDescentPerEm = static_cast<float> (static_cast<std::int16_t> (be16 (hhea + 6)))
                                   / static_cast<float> (metrics.unitsPerEm);

        const std::uint16_t metricsCount = be16 (hhea + 34);
        for (std::uint32_t d = 0; d < 10; ++d)
        {
            const std::uint32_t glyph = glyphOf (cmap, cmapLength,
                                                  static_cast<std::uint32_t> ('0') + d);
            if (glyph == 0 || glyph >= 65535U)
                continue;

            /* hmtx holds `numberOfHMetrics` advance pairs and then one lsb per remaining glyph, so the last
               declared advance answers every glyph past it. */
            const std::size_t entry = glyph < metricsCount ? glyph : metricsCount - 1U;
            metrics.digitAdvancePerEm[d]
                = static_cast<float> (be16 (hmtx + 4 * entry)) / static_cast<float> (metrics.unitsPerEm);
        }
    }

    return metrics;
}
}
