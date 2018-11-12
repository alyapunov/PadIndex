#include "Benchmarks.hpp"

#include <iostream>

#include "Db.hpp"
#include "Index.hpp"
#include "Utils.hpp"

static void selectCampaigns()
{
    target::dynamic_bitset sAll(IndexedCampaigns.size());
    {
        CTitle title("Benchmark: select all campaigns for all pads");

        for (auto& sPair : Pads)
        {
            uint32_t sPadId = sPair.first;
            sAll |= campaignsByPad(sPadId);
        }
    }
    std::cout << "Total " << sAll.count() << " from " << IndexedCampaigns.size() << std::endl;
}

static void selectCampaignsAndBanners()
{
    size_t sTotal = 0;
    {
        CTitle title("Benchmark: select all campaigns and banners for all pads");

        for (auto& sPair : Pads)
        {
            uint32_t sPadId = sPair.first;
            target::dynamic_bitset sCampBitset = campaignsByPad(sPadId);
            const std::unordered_set<uint32_t>& sFilteredBanners = filteredBannersByPad(sPadId);
            for (size_t sBit = sCampBitset.find_first();
                 sBit != sCampBitset.npos;
                 sBit = sCampBitset.find_next(sBit))
            {
                const IndexedCampaign& sIndCamp = IndexedCampaigns[sBit];
                Campaign& sCamp = Campaigns[sIndCamp.m_CampaignId];
                for (uint32_t sBannId : sCamp.m_BannerIds)
                {
                    if (sFilteredBanners.find(sBannId) != sFilteredBanners.end())
                        sTotal++;
                }
            }
        }
    }
    std::cout << "Total " << sTotal << std::endl;
}

void runBench()
{
    std::cout << "// Benchmarks are made for all pads, real request checks only one pad" << std::endl;
    std::cout << "// So to get real request latency the times must be divided by " << Pads.size() << std::endl;
    selectCampaigns();
    selectCampaignsAndBanners();
}