#include "Benchmarks.hpp"

#include <iostream>

#include "Db.hpp"
#include "Index.hpp"
#include "Utils.hpp"

static void selectCampaigns()
{
	size_t sWantPads = 10000;
	size_t sGotPads = 0;
	std::cout << "// Test for " << sWantPads << " leaf pads:" << std::endl;
	target::dynamic_bitset sAll(IndexedCampaigns.size());
	{
        CTitle title("Benchmark: select all campaigns for pads");

        for (auto& sPair : Pads)
        {
            uint32_t sPadId = sPair.first;
			const Pad& sPad = sPair.second;
			if (!sPad.m_DirectChildren.empty())
				continue; // Skip non leaf pads.
			sGotPads++;
			sAll |= campaignsByPad(sPadId);
			if (sGotPads >= sWantPads)
				break;
        }
    }
	check(sGotPads == sWantPads, "Not enough leaf pads");
    std::cout << "Total " << sAll.count() << " from " << IndexedCampaigns.size() << std::endl;
}

static void selectCampaignsAndBanners()
{
	size_t sWantPads = 1000;
	size_t sGotPads = 0;
	std::cout << "// Test for " << sWantPads << " leaf pads:" << std::endl;
	size_t sTotal = 0;
    {
        CTitle title("Benchmark: select all campaigns and banners for pads");

        for (auto& sPair : Pads)
        {
            uint32_t sPadId = sPair.first;
			const Pad& sPad = sPair.second;
			if (!sPad.m_DirectChildren.empty())
				continue; // Skip non leaf pads.
			sGotPads++;
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
			if (sGotPads >= sWantPads)
				break;
		}
    }
	check(sGotPads == sWantPads, "Not enough leaf pads");
    std::cout << "Total " << sTotal << std::endl;
}

void runBench()
{
    selectCampaigns();
    selectCampaignsAndBanners();
}