#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Db.hpp"
#include "dynamic_bitset.hpp"

// Campaign in index.
struct IndexedCampaign
{
    uint32_t m_UserId;
    uint32_t m_CampaignId;
    // Position of first banner in IndexedBanners array.
    uint32_t m_FirstBannerPosition;
    // Count of banners in IndexedBanners array.
    uint32_t m_BannerCount;
};

// Array of campaigns in the index.
// This vector will be initialized during loading of filters.
// Campaigns must be ordered by users (campaigns of the same user must be nearby)
extern std::vector<IndexedCampaign> IndexedCampaigns;

// Bannerd in index.
struct IndexedBanner
{
    uint32_t m_UserId;
    uint32_t m_CampaignId;
    uint32_t m_BannerId;
};

// Array of banners in the index.
// This vector will be initialized during loading of filters.
// Banners must be ordered by campaigns (banners of the same campaigns must be nearby).
// Also order of campaigns in this array is the same as in IndexedCampaigns.
extern std::vector<IndexedBanner> IndexedBanners;

// Build the index!
void buildIndexes();

// Get campaign bits that can be shown on given pad.
target::dynamic_bitset campaignsByPad(uint32_t aPadId);

// Get list of banners that are prohibited to show on given pad.
// For optimisation the list doesn't include banners from fully filtered campaigns
// (campaigns that are not present in campaignsByPad(aPadId) bitset).
const std::unordered_set<uint32_t>& filteredBannersByPad(uint32_t aPadId);
