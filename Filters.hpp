#pragma once

#include <unordered_map>
#include <vector>

#include "dynamic_bitset.hpp"

// Structure that describes filters of a pad.
struct PadFilter
{
    // Bitsets of campaigns. Every bit corresponds to position in IndexedCampaigns.
    // Non-zero bit claims that all banners of corresponding campaign pass filters.
    target::dynamic_bitset* m_All;
    // Non-zero bit claims that there is at least one banner of corresponding campaign that passes filters.
    target::dynamic_bitset* m_Any;

    // Bitset of banners. Every bit corresponds to position in IndexedBanners.
    // Non-zero bit claims that corresponding banner passes filters.
    target::dynamic_bitset* m_Banners;
};

// Map pad_id -> PadFilter of that pad.
extern std::unordered_map<uint32_t, PadFilter> PadFilters;

// PadFilter pointers to bitsets lead to one of bitsets in this banks.
extern std::vector<target::dynamic_bitset> PadBannerBitsetBank;
extern std::vector<target::dynamic_bitset> PadCampaignBitsetBank;

// Load PadFilters from special file.
// Calculation of filter indexes is rather complex and is excluded from that benchmark.
void loadPrecalculatedFilters();
