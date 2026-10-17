#include "RokAllianceFlagLocalPreviewData.h"

namespace RokAllianceFlagLocalPreview
{
const TArray<FEntry>& Rows()
{
    static const TArray<FEntry> SourceRows = {
        {320, 3, FIntRect(373, 1805, 493, 1925), FIntRect(373, 1805, 493, 1925), FColor(255, 255, 255, 255), true, false},
        {321, 3, FIntRect(373, 1683, 493, 1803), FIntRect(373, 1683, 493, 1803), FColor(255, 255, 255, 255), true, false},
        {322, 3, FIntRect(373, 1561, 493, 1681), FIntRect(373, 1561, 493, 1681), FColor(255, 255, 255, 255), true, false},
        {323, 3, FIntRect(373, 829, 493, 949), FIntRect(373, 829, 493, 949), FColor(255, 255, 255, 255), true, false},
        {324, 3, FIntRect(739, 1927, 859, 2047), FIntRect(739, 1927, 859, 2047), FColor(255, 255, 255, 255), true, false},
        {407, 4, FIntRect(), FIntRect(), FColor(255, 207, 206, 255), false, true},
        {406, 4, FIntRect(), FIntRect(), FColor(255, 247, 189, 255), false, true},
        {405, 4, FIntRect(), FIntRect(), FColor(255, 255, 255, 255), false, true},
        {201, 2, FIntRect(), FIntRect(), FColor(214, 0, 123, 255), false, true},
        {202, 2, FIntRect(), FIntRect(), FColor(189, 0, 206, 255), false, true},
        {203, 2, FIntRect(), FIntRect(), FColor(123, 0, 206, 255), false, true},
        {204, 2, FIntRect(), FIntRect(), FColor(82, 0, 206, 255), false, true},
        {205, 2, FIntRect(), FIntRect(), FColor(41, 0, 206, 255), false, true},
        {206, 2, FIntRect(), FIntRect(), FColor(0, 52, 206, 255), false, true},
        {207, 2, FIntRect(), FIntRect(), FColor(0, 93, 206, 255), false, true},
        {208, 2, FIntRect(), FIntRect(), FColor(0, 138, 206, 255), false, true},
        {209, 2, FIntRect(), FIntRect(), FColor(0, 199, 206, 255), false, true},
        {210, 2, FIntRect(), FIntRect(), FColor(0, 207, 82, 255), false, true},
        {211, 2, FIntRect(), FIntRect(), FColor(132, 207, 0, 255), false, true},
        {212, 2, FIntRect(), FIntRect(), FColor(206, 203, 0, 255), false, true},
        {213, 2, FIntRect(), FIntRect(), FColor(198, 134, 0, 255), false, true},
        {214, 2, FIntRect(), FIntRect(), FColor(206, 109, 0, 255), false, true},
        {215, 2, FIntRect(), FIntRect(), FColor(206, 73, 0, 255), false, true},
        {216, 2, FIntRect(), FIntRect(), FColor(206, 0, 0, 255), false, true},
        {404, 4, FIntRect(), FIntRect(), FColor(189, 255, 198, 255), false, true},
        {403, 4, FIntRect(), FIntRect(), FColor(181, 247, 255, 255), false, true},
        {402, 4, FIntRect(), FIntRect(), FColor(189, 207, 255, 255), false, true},
        {401, 4, FIntRect(), FIntRect(), FColor(255, 190, 255, 255), false, true},
        {301, 3, FIntRect(373, 1439, 493, 1559), FIntRect(373, 1439, 493, 1559), FColor(255, 255, 255, 255), true, false},
        {302, 3, FIntRect(373, 1317, 493, 1437), FIntRect(373, 1317, 493, 1437), FColor(255, 255, 255, 255), true, false},
        {303, 3, FIntRect(373, 1195, 493, 1315), FIntRect(373, 1195, 493, 1315), FColor(255, 255, 255, 255), true, false},
        {304, 3, FIntRect(373, 1073, 493, 1193), FIntRect(373, 1073, 493, 1193), FColor(255, 255, 255, 255), true, false},
        {305, 3, FIntRect(373, 951, 493, 1071), FIntRect(373, 951, 493, 1071), FColor(255, 255, 255, 255), true, false},
        {306, 3, FIntRect(373, 707, 493, 827), FIntRect(373, 707, 493, 827), FColor(255, 255, 255, 255), true, false},
        {307, 3, FIntRect(373, 585, 493, 705), FIntRect(373, 585, 493, 705), FColor(255, 255, 255, 255), true, false},
        {308, 3, FIntRect(309, 401, 429, 521), FIntRect(309, 401, 429, 521), FColor(255, 255, 255, 255), true, false},
        {101, 1, FIntRect(495, 1541, 559, 1605), FIntRect(1, 305, 185, 521), FColor(255, 255, 255, 255), true, false},
        {102, 1, FIntRect(495, 1079, 559, 1143), FIntRect(187, 523, 371, 739), FColor(255, 255, 255, 255), true, false},
        {103, 1, FIntRect(495, 1607, 559, 1671), FIntRect(1, 523, 185, 739), FColor(255, 255, 255, 255), true, false},
        {104, 1, FIntRect(495, 1673, 559, 1737), FIntRect(1, 741, 185, 957), FColor(255, 255, 255, 255), true, false},
        {105, 1, FIntRect(927, 1983, 991, 2047), FIntRect(1, 959, 185, 1175), FColor(255, 255, 255, 255), true, false},
        {106, 1, FIntRect(67, 21, 131, 85), FIntRect(1, 1177, 185, 1393), FColor(255, 255, 255, 255), true, false},
        {107, 1, FIntRect(617, 1861, 681, 1925), FIntRect(1, 1395, 185, 1611), FColor(255, 255, 255, 255), true, false},
        {108, 1, FIntRect(495, 1739, 559, 1803), FIntRect(187, 1831, 371, 2047), FColor(255, 255, 255, 255), true, false},
        {109, 1, FIntRect(861, 1983, 925, 2047), FIntRect(1, 1613, 185, 1829), FColor(255, 255, 255, 255), true, false},
        {110, 1, FIntRect(1, 21, 65, 85), FIntRect(1, 1831, 185, 2047), FColor(255, 255, 255, 255), true, false},
        {111, 1, FIntRect(495, 1145, 559, 1209), FIntRect(187, 741, 371, 957), FColor(255, 255, 255, 255), true, false},
        {112, 1, FIntRect(495, 1211, 559, 1275), FIntRect(187, 959, 371, 1175), FColor(255, 255, 255, 255), true, false},
        {113, 1, FIntRect(495, 1277, 559, 1341), FIntRect(187, 1177, 371, 1393), FColor(255, 255, 255, 255), true, false},
        {114, 1, FIntRect(495, 1343, 559, 1407), FIntRect(187, 1395, 371, 1611), FColor(255, 255, 255, 255), true, false},
        {115, 1, FIntRect(495, 1409, 559, 1473), FIntRect(187, 1613, 371, 1829), FColor(255, 255, 255, 255), true, false},
        {116, 1, FIntRect(495, 1475, 559, 1539), FIntRect(1, 87, 185, 303), FColor(255, 255, 255, 255), true, false},
        {309, 3, FIntRect(309, 279, 429, 399), FIntRect(309, 279, 429, 399), FColor(255, 255, 255, 255), true, false},
        {310, 3, FIntRect(309, 157, 429, 277), FIntRect(309, 157, 429, 277), FColor(255, 255, 255, 255), true, false},
        {311, 3, FIntRect(187, 35, 307, 155), FIntRect(187, 35, 307, 155), FColor(255, 255, 255, 255), true, false},
        {312, 3, FIntRect(309, 35, 429, 155), FIntRect(309, 35, 429, 155), FColor(255, 255, 255, 255), true, false},
        {313, 3, FIntRect(495, 1927, 615, 2047), FIntRect(495, 1927, 615, 2047), FColor(255, 255, 255, 255), true, false},
        {314, 3, FIntRect(617, 1927, 737, 2047), FIntRect(617, 1927, 737, 2047), FColor(255, 255, 255, 255), true, false},
        {315, 3, FIntRect(495, 1805, 615, 1925), FIntRect(495, 1805, 615, 1925), FColor(255, 255, 255, 255), true, false},
        {316, 3, FIntRect(373, 1927, 493, 2047), FIntRect(373, 1927, 493, 2047), FColor(255, 255, 255, 255), true, false},
        {317, 3, FIntRect(187, 401, 307, 521), FIntRect(187, 401, 307, 521), FColor(255, 255, 255, 255), true, false},
        {318, 3, FIntRect(187, 279, 307, 399), FIntRect(187, 279, 307, 399), FColor(255, 255, 255, 255), true, false},
        {319, 3, FIntRect(187, 157, 307, 277), FIntRect(187, 157, 307, 277), FColor(255, 255, 255, 255), true, false},
    };
    return SourceRows;
}

const FEntry* Find(int32 Type, int32 Id)
{
    for (const FEntry& Entry : Rows())
    {
        if (Entry.Type == Type && Entry.Id == Id) return &Entry;
    }
    return nullptr;
}

bool IsValidDraft(const FDraft& Draft)
{
    for (int32 Type = 1; Type <= 4; ++Type)
    {
        if (!Find(Type, Draft.GetId(Type))) return false;
    }
    return true;
}

FLinearColor LinearColor(const FEntry& Entry)
{
    return FLinearColor::FromSRGBColor(Entry.Rgb);
}
}
