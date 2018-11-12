#include "Index.hpp"

#include <algorithm>
#include <iostream>
#include <unordered_map>

#include "Db.hpp"
#include "Filters.hpp"
#include "Utils.hpp"

// Array of campaigns in the index.
// This vector will be initialized during loading of filters.
std::vector<IndexedCampaign> IndexedCampaigns;

// Array of banners in the index.
// This vector will be initialized during loading of filters.
std::vector<IndexedBanner> IndexedBanners;

// pad_id -> bitset of campaigns that passes positive/negative targetings and filters
// directly for this pad.
// Every bit of a bitset corresponds to the campaign in IndexedCampaigns in the same position.
std::unordered_map<uint32_t, target::dynamic_bitset> PositiveCampaigns;
std::unordered_map<uint32_t, target::dynamic_bitset> NegativeCampaigns;

//  pad_id -> set of banners IDs that:
// 1) are filtered on this pad (directly) by pad's filters.
// AND
// 2) belongs to campaign that is not filtered on this pad (directly).
// Note that if all banners of a campaign are filtered on a pad then the entire campaign
// is crossed out by index and there's no need to store all its filtered banners.
std::unordered_map<uint32_t, std::unordered_set<uint32_t>> FilteredBanners;

// pad effective group id -> array of banners that are filtered on this pad
// (including ancestor's filters) and are not belong to fully filtered campaigns.
// Actually it is joined FilteredBanners by all pad's ancestors and the pad itself.
std::unordered_map<uint32_t, std::unordered_set<uint32_t>> GroupCumulativeFilteredBanners;

struct PadReoder
{
    uint32_t m_PadId;
    size_t m_CampaignPos;

    PadReoder(uint32_t aPadId, size_t aCampaignPos) : m_PadId(aPadId), m_CampaignPos(aCampaignPos) {}

    bool operator<(const PadReoder& aOther) const
    {
        return std::tie(m_PadId, m_CampaignPos) < std::tie(aOther.m_PadId, aOther.m_CampaignPos);
    }
};

// fill PositiveCampaigns and NegativeCampaigns by all targetings.
static void buildTargetings()
{
    std::vector<PadReoder> sPositive;
    std::vector<PadReoder> sNegative;
    {
        CTitle title("Indexing: Collecting targeted pad/campaign pairs");

        for (size_t i = 0; i < IndexedCampaigns.size(); i++)
        {
            Campaign& sCamp = Campaigns[IndexedCampaigns[i].m_CampaignId];
            bool sHasPositiveTargetings = false;
            bool sHasPositivePackageTargetings = false;
            bool sHasPositiveUserTargetings = false;

            for (Pad* sPad : sCamp.m_PositiveTargetingPad)
            {
                sPositive.emplace_back(sPad->m_Id, i);
                sHasPositiveTargetings = true;
            }
            for (Pad* sPad : sCamp.m_NegatineTargetingPad)
                sNegative.emplace_back(sPad->m_Id, i);

            if (sCamp.m_Package != nullptr)
            {
                for (Pad* sPad : sCamp.m_Package->m_PositiveTargetingPad)
                {
                    sPositive.emplace_back(sPad->m_Id, i);
                    sHasPositivePackageTargetings = true;
                }
                for (Pad* sPad : sCamp.m_Package->m_NegatineTargetingPad)
                    sNegative.emplace_back(sPad->m_Id, i);
            }

            User* sUser = sCamp.m_User;
            while (sUser != nullptr)
            {
                for (Pad* sPad : sUser->m_PositiveTargetingPad)
                {
                    sPositive.emplace_back(sPad->m_Id, i);
                    sHasPositiveUserTargetings = true;
                }
                for (Pad* sPad : sUser->m_NegatineTargetingPad)
                    sNegative.emplace_back(sPad->m_Id, i);

                sUser = sUser->m_Parent;
            }

            check(sHasPositiveTargetings != sHasPositivePackageTargetings, "This case will work incorrect in this test");
            check(!sHasPositiveUserTargetings, "This case will work incorrect in this test");
        }
        std::sort(sPositive.begin(), sPositive.end());
        std::sort(sNegative.begin(), sNegative.end());
    }

    std::cout << "Total positive : " << sPositive.size() << std::endl;
    std::cout << "Total negative : " << sNegative.size() << std::endl;

    {
        CTitle title("Indexing: Filling targeting bitsets");
        uint32_t sLastPadId = 0;
        target::dynamic_bitset* sCurrentPadBitset = nullptr;
        for (const PadReoder& sPair : sPositive)
        {
            if (sLastPadId != sPair.m_PadId)
            {
                sLastPadId = sPair.m_PadId;
                sCurrentPadBitset = &PositiveCampaigns[sPair.m_PadId];
                sCurrentPadBitset->resize(IndexedCampaigns.size(), false);
            }
            sCurrentPadBitset->set(sPair.m_CampaignPos);
        }
        for (const PadReoder& sPair : sNegative)
        {
            if (sLastPadId != sPair.m_PadId)
            {
                sLastPadId = sPair.m_PadId;
                sCurrentPadBitset = &NegativeCampaigns[sPair.m_PadId];
                sCurrentPadBitset->resize(IndexedCampaigns.size(), true);
            }
            sCurrentPadBitset->reset(sPair.m_CampaignPos);
        }
    }
}

// fill NegativeCampaigns and FilteredBanners by all filters.
static void buildFilters()
{
    {
        CTitle title("Indexing: Adding fully filtered campaigns");
        for (auto& sPair : PadFilters)
        {
            uint32_t sPadId = sPair.first;
            PadFilter& sPadFilter = sPair.second;

            target::dynamic_bitset& sCurrentPadBitset = NegativeCampaigns[sPadId];
            if (sCurrentPadBitset.empty())
                sCurrentPadBitset = *sPadFilter.m_Any;
            else
                sCurrentPadBitset &= *sPadFilter.m_Any;
        }
    }

    {
        CTitle title("Indexing: Adding filtered banners of partially filtered campaigns");
        for (auto& sPair : PadFilters)
        {
            uint32_t sPadId = sPair.first;
            PadFilter& sPadFilter = sPair.second;
            std::unordered_set<uint32_t>* sFilteredBannerd = nullptr;

            // Temporarily exclude fully passing campaigns. (1)
            *sPadFilter.m_Any -= *sPadFilter.m_All;
            // Now sPadFilter.m_Any reveals campaigns that have some banners that
            // passes filters and some banners that fails filters.

            for (size_t i = sPadFilter.m_Any->find_first();
                 i != sPadFilter.m_Any->npos;
                 i = sPadFilter.m_Any->find_next(i))
            {
                for (size_t j = 0; j < IndexedCampaigns[i].m_BannerCount; j++)
                {
                    size_t k = IndexedCampaigns[i].m_FirstBannerPosition + j;
                    if (!sPadFilter.m_Banners->test(k))
                    {
                        if (nullptr == sFilteredBannerd)
                            sFilteredBannerd = &FilteredBanners[sPadId];
                        sFilteredBannerd->insert(IndexedBanners[k].m_BannerId);
                    }
                }
            }

            // Restore back. (1)
            *sPadFilter.m_Any |= *sPadFilter.m_All;
        }
    }
}

// Set m_EffectivePads, m_EffectivePadsAreBuilt members for given pad.
static void buildEffectivePads(Pad& aPad)
{
    if (aPad.m_EffectivePadsAreBuilt)
        return;

    if (aPad.m_HasTargetingsOrFilters)
        aPad.m_EffectivePads.push_back(aPad.m_Id);

    for (Pad* sParent : aPad.m_DirectParents)
    {
        buildEffectivePads(*sParent);
        // Append effective pad IDs of that parent.
        aPad.m_EffectivePads.insert(aPad.m_EffectivePads.end(),
                                    sParent->m_EffectivePads.begin(),
                                    sParent->m_EffectivePads.end());
    }

    // Sort and remove duplicates.
    std::sort(aPad.m_EffectivePads.begin(), aPad.m_EffectivePads.end());
    aPad.m_EffectivePads.erase(std::unique(aPad.m_EffectivePads.begin(),
                                           aPad.m_EffectivePads.end()),
                               aPad.m_EffectivePads.end());

    aPad.m_EffectivePadsAreBuilt = true;
}

// Set m_EffectivePads, m_EffectivePadsAreBuilt and m_EffectivePadsGroupId members for all pads.
static void buildEffectivePads()
{
    size_t sNumberOfPads = Pads.size();
    size_t sNumberOfEmptyPads = 0; // Pads with empty m_EffectivePads vector

    {
        // Set m_EffectivePads, m_EffectivePadsAreBuilt
        CTitle title("Indexing: Collect effective pads");
        for (auto& sPair : Pads)
        {
            //uint32_t sPadId = sPair.first;
            Pad& sPad = sPair.second;
            buildEffectivePads(sPad);
            if (sPad.m_EffectivePads.empty())
                sNumberOfEmptyPads++;
        }
    }

    std::cout << "All pads / Empty effective pads: "
              << sNumberOfPads << " / " << sNumberOfEmptyPads << std::endl;

    size_t sNumberOfGroups = 0;
    size_t sNumberOfEmptyGroups = 0; // Groups with empty m_EffectivePads vector
    {
        // Set m_EffectivePadsGroupId
        CTitle title("Indexing: Group effective pads");
        // For each pad let's calculate a hash of all its effective pads.
        // For each hash let's store pad IDs that have this hash.
        std::unordered_map<uint32_t, std::vector<uint32_t>> sPadsByHash;

        for (auto& sPair : Pads)
        {
            uint32_t sPadId = sPair.first;
            Pad& sPad = sPair.second;
            uint32_t sHash = 0;
            for (uint32_t sId : sPad.m_EffectivePads)
                sHash ^= sId;

            std::vector<uint32_t>& sPossiblyIdenticalPads = sPadsByHash[sHash];
            bool FoundIdentical = false;
            for (uint32_t sGroupCandidateId : sPossiblyIdenticalPads)
            {
                Pad& sCandidate = Pads[sGroupCandidateId];
                if (sPad.m_EffectivePads.size() != sCandidate.m_EffectivePads.size())
                    continue;
                bool sEqual = true;
                for (size_t i = 0; i < sPad.m_EffectivePads.size(); i++)
                {
                    if (sPad.m_EffectivePads[i] != sCandidate.m_EffectivePads[i])
                    {
                        sEqual = false;
                        break;
                    }
                }
                if (!sEqual)
                    continue;
                FoundIdentical = true;
                sPad.m_EffectivePadsGroupId = sGroupCandidateId;
                break;
            }
            if (!FoundIdentical)
            {
                sPad.m_EffectivePadsGroupId = sPadId;
                sPossiblyIdenticalPads.push_back(sPadId);
                sNumberOfGroups++;
                if (sPad.m_EffectivePads.empty())
                    sNumberOfEmptyGroups++;
            }
        }
    }

    std::cout << "All groups / Empty effective groups: "
              << sNumberOfGroups << " / " << sNumberOfEmptyGroups << std::endl;
}

// Fill GroupCumulativeFilteredBanners..
static void buildGroupCumulativeFilteredBanners()
{
    CTitle title("Indexing: Group cumulative filtered banners");
    for (auto& sPair : Pads)
    {
        uint32_t sPadId = sPair.first;
        Pad& sPad = sPair.second;
        if (GroupCumulativeFilteredBanners.find(sPadId) != GroupCumulativeFilteredBanners.end())
        {
            // already caclulated
            continue;
        }
        std::unordered_set<uint32_t>& sBlockedBanners = GroupCumulativeFilteredBanners[sPadId];
        for (uint32_t sFilteringPadId : sPad.m_EffectivePads)
        {
            const std::unordered_set<uint32_t>& sBlockedDirectly = FilteredBanners[sFilteringPadId];
            sBlockedBanners.insert(sBlockedDirectly.begin(), sBlockedDirectly.end());
        }
    }
}

// Calculate how many advertisments and campaingns are allowed to show on every pad.
// For simplification we don't store this statistics; in real program we do.
// TODO: is it possible to calculate how many banners are allowed to show on every pad?
static void calcPadStat()
{
    size_t sTotalUsers = 0;
    size_t sTotalCampaigns = 0;

    {
        CTitle title("Indexing: calculate pad stats");

        for (auto& sPair : Pads)
        {
            uint32_t sPadId = sPair.first;

            size_t sUsers = 0;
            size_t sCampaigns = 0;
            uint32_t sLastUser = 0;
            target::dynamic_bitset sCampaignBitset = campaignsByPad(sPadId);
            for (size_t sBit = sCampaignBitset.find_first();
                 sBit != sCampaignBitset.npos;
                 sBit = sCampaignBitset.find_next(sBit))
            {
                sCampaigns++;
                if (sLastUser != IndexedCampaigns[sBit].m_UserId)
                {
                    sUsers++;
                    sLastUser = IndexedCampaigns[sBit].m_UserId;
                }
            }

            sTotalUsers += sUsers;
            sTotalCampaigns += sCampaigns;
        }
    }

    std::cout << "Total statat : advertisments / campaigns "
              << sTotalUsers << " / " << sTotalCampaigns << std::endl;
}

static void reportIndexSizes()
{
    std::cout << "!!!Approximate size of the index!!!:" << std::endl;
    size_t sPositiveBitsets = 0;
    for (const auto& sPair : PositiveCampaigns)
    {
        const target::dynamic_bitset& sBitset = sPair.second;
        sPositiveBitsets += sBitset.mem_size();
    }
    size_t sNegativeBitsets = 0;
    for (const auto& sPair : NegativeCampaigns)
    {
        const target::dynamic_bitset& sBitset = sPair.second;
        sNegativeBitsets += sBitset.mem_size();
    }
    size_t sBannerHashTableEntries = 0;
    for (const auto& sPair : GroupCumulativeFilteredBanners)
    {
        sBannerHashTableEntries++;
        sBannerHashTableEntries += sPair.second.size();
    }
    size_t sBannerHashTableMemSize = sBannerHashTableEntries * 36; // Approximate..
    size_t sTotal = sPositiveBitsets + sNegativeBitsets + sBannerHashTableMemSize;

    std::cout << "BannerHashTableEntries: " << sBannerHashTableEntries << std::endl;
    std::cout << "PositiveBitsets: " << sPositiveBitsets / 1024 / 1024 << "MB" << std::endl;
    std::cout << "NegativeBitsets: " << sNegativeBitsets / 1024 / 1024 << "MB" << std::endl;
    std::cout << "BannerHashTableMemSize: " << sBannerHashTableMemSize / 1024 / 1024 << "MB" << std::endl;
    std::cout << "Total: " << sTotal / 1024 / 1024 << "MB" << std::endl;
}

void buildIndexes()
{
    buildTargetings();
    buildFilters();
    buildEffectivePads();
    buildGroupCumulativeFilteredBanners();
    calcPadStat();
    reportIndexSizes();
}

// Get campaign bits that can be shown on given pad.
target::dynamic_bitset campaignsByPad(uint32_t aPadId)
{
    target::dynamic_bitset sResult(IndexedCampaigns.size(), false);
    const Pad& sPad = Pads[aPadId];

    // Positive targetings: every campaign that is allowed directly on the pad
    // or on any ancestor is allowed to show.
    for (uint32_t sEffectivePadId : sPad.m_EffectivePads)
    {
        auto sItr = PositiveCampaigns.find(sEffectivePadId);
        if (sItr == PositiveCampaigns.end())
            continue;
        const target::dynamic_bitset& sAllowed = sItr->second;
        sResult |= sAllowed;
    }

    // Negative targetings: every campaign that is not allowed directly on the pad
    // or on any ancestor is not allowed to show.
    for (uint32_t sEffectivePadId : sPad.m_EffectivePads)
    {
        auto sItr = NegativeCampaigns.find(sEffectivePadId);
        if (sItr == NegativeCampaigns.end())
            continue;
        const target::dynamic_bitset& sAllowed = sItr->second;
        sResult &= sAllowed;
    }

    return sResult;
}

// Get list of banners that are prohibited to show on given pad.
// For optimisation the list doesn't include banners from fully filtered campaigns
// (campaigns that are not present in campaignsByPad(aPadId) bitset).
const std::unordered_set<uint32_t>& filteredBannersByPad(uint32_t aPadId)
{
    const Pad& sPad = Pads[aPadId];
    auto sItr = GroupCumulativeFilteredBanners.find(sPad.m_EffectivePadsGroupId);
    if (sItr == GroupCumulativeFilteredBanners.end())
    {
        // No banners are filtered
        static const std::unordered_set<uint32_t> sEmpty;
        return sEmpty;
    }
    return sItr->second;
}