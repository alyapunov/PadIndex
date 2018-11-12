#include "Filters.hpp"

#include <iostream>
#include <fstream>
#include <string>

#include "Db.hpp"
#include "Index.hpp"
#include "Utils.hpp"

// Map pad_id -> PadFilter of that pad.
std::unordered_map<uint32_t, PadFilter> PadFilters;

// PadFilter pointers to bitsets lead to one of bitsets in this bank.
std::vector<target::dynamic_bitset> PadBannerBitsetBank;
std::vector<target::dynamic_bitset> PadCampaignBitsetBank;

static target::dynamic_bitset loadBitsetFromString(const std::string& aStr,
                                                   size_t aOriginalSize,
                                                   const std::vector<uint32_t>& aSkipBits)
{
    assert(aStr.size() == (aOriginalSize + 3)/ 4);
    target::dynamic_bitset sResult(aOriginalSize - aSkipBits.size());

    size_t sPosInSkipBits = 0;
    size_t sResultPos = 0;

    for (size_t i = 0; i < aStr.size(); i++)
    {
        assert((aStr[i] >= '0' && aStr[i] <= '9') || (aStr[i] >= 'a' && aStr[i] <= 'f'));
        int digit = (aStr[i] >= '0' && aStr[i] <= '9') ? aStr[i] - '0' : aStr[i] - 'a' + 10;
        for (size_t j = 0; j < 4; j++)
        {
            size_t sOriginalPos = i * 4 + j;
            if (sPosInSkipBits < aSkipBits.size() && aSkipBits[sPosInSkipBits] == sOriginalPos)
            {
                sPosInSkipBits++;
                continue;
            }
            if ((digit >> j) & 1)
                sResult.set(sResultPos);
            sResultPos++;
        }
    }
    return sResult;
}

void loadPrecalculatedFilters()
{
    // Due to lags in dumping data files, some campaigns can be not present in local DB.
    // Let's skip them and delete their bit from bitsets.
    size_t sOriginalCampaignCount = 0, sOriginalBannerCount = 0;
    std::vector<uint32_t> sSkippedCampaigns, sSkippedBanners;

    const std::string sFilename = "Data/index.txt";
    std::fstream sFile(sFilename, std::fstream::in);
    if (!sFile.is_open())
        sFile.open("../" + sFilename, std::fstream::in);
    check(sFile.is_open(), "sFile not found!");
    std::string sLine;

    {
        CTitle title("Reading pad index sFile: campaigns/banners");

        std::unordered_set<uint32_t> sUserIds;
        size_t sCountOfUsers = 0;
        uint32_t sLastUser = 0;

        std::getline(sFile, sLine);
        check(sLine == "Campaigns (id)", "Wrong file format");
        sFile >> sOriginalCampaignCount;
        check(sOriginalCampaignCount != 0, "Must have at least one campaign");
        for (size_t i = 0; i < sOriginalCampaignCount; i++)
        {
            uint32_t campaign_id = 0;
            sFile >> campaign_id;
            if (Campaigns.count(campaign_id) == 0)
            {
                sSkippedCampaigns.push_back(i);
            }
            else
            {
                IndexedCampaign camp;
                camp.m_UserId = Campaigns[campaign_id].m_UserId;
                camp.m_CampaignId = campaign_id;
                IndexedCampaigns.push_back(camp);

                if (sLastUser != camp.m_UserId)
                {
                    sLastUser = camp.m_UserId;
                    sCountOfUsers++;
                }
                sUserIds.insert(camp.m_UserId);
            }
        }

        check(sUserIds.size() == sCountOfUsers, "Order of users is broken!");

        size_t sPosInIndexedCampaigns = 0;
        IndexedCampaigns[0].m_FirstBannerPosition = 0;
        IndexedCampaigns[0].m_BannerCount = 0;

        std::getline(sFile, sLine);
        if (sLine != "Banners (id, campaign_id)")
            std::getline(sFile, sLine);
        check(sLine == "Banners (id, campaign_id)", "Wrong file format");
        sFile >> sOriginalBannerCount;
        check(sOriginalBannerCount != 0, "Must have at least one banner");
        for (size_t i = 0; i < sOriginalBannerCount; i++)
        {
            uint32_t id = 0;
            sFile >> id;
            uint32_t campaign_id = 0;
            sFile >> campaign_id;
            if (Campaigns.count(campaign_id) == 0)
            {
                sSkippedBanners.push_back(i);
            }
            else
            {
                // Banners must follow the same order of campaigns as in IndexedCampaigns.
                // A campaign must have at least one banner.
                if (IndexedCampaigns[sPosInIndexedCampaigns].m_CampaignId != campaign_id)
                {
                    ++sPosInIndexedCampaigns;
                    check(sPosInIndexedCampaigns != IndexedCampaigns.size(), "Campaign disorder");
                    IndexedCampaigns[sPosInIndexedCampaigns].m_FirstBannerPosition = IndexedBanners.size();
                    IndexedCampaigns[sPosInIndexedCampaigns].m_BannerCount = 0;
                }
                check(IndexedCampaigns[sPosInIndexedCampaigns].m_CampaignId == campaign_id, "Campaign disorder");
                IndexedCampaigns[sPosInIndexedCampaigns].m_BannerCount++;

                Campaigns[campaign_id].m_BannerIds.push_back(id);
                IndexedBanner bann;
                bann.m_UserId = Campaigns[campaign_id].m_UserId;
                bann.m_CampaignId = campaign_id;
                bann.m_BannerId = id;
                IndexedBanners.push_back(bann);
            }
        }

        // Paranoiac check.
        size_t sCheckBannersCount = 0;
        for (size_t i = 0; i < IndexedCampaigns.size(); i++)
        {
            sCheckBannersCount += IndexedCampaigns[i].m_BannerCount;
            for (size_t j = 0; j < IndexedCampaigns[i].m_BannerCount; j++)
            {
                size_t k = IndexedCampaigns[i].m_FirstBannerPosition + j;
                check(k < IndexedBanners.size(), "Smth went wrong");
                check(IndexedBanners[k].m_UserId == IndexedCampaigns[i].m_UserId, "Smth went wrong");
                check(IndexedBanners[k].m_CampaignId == IndexedCampaigns[i].m_CampaignId, "Smth went wrong");
            }
        }
        check(IndexedBanners.size() == sCheckBannersCount, "Smth went wrong");
    }

    std::cout << "Num campaings in index " << IndexedCampaigns.size()
              << " (was skipped " << sSkippedCampaigns.size() << ")" << std::endl;

    std::cout << "Num banners in index " << IndexedBanners.size()
              << " (was skipped " << sSkippedBanners.size() << ")" << std::endl;

    {
        CTitle title("Reading pad index sFile: bitset catalogue");

        sFile >> sLine;
        check(sLine == "Campaign", "Wrong file format");
        sFile >> sLine;
        check(sLine == "bitsets:", "Wrong file format");

        size_t sBitsetCount = 0;
        sFile >> sBitsetCount;
        PadCampaignBitsetBank.resize(sBitsetCount);
        for (size_t i = 0; i < sBitsetCount; i++)
        {
            uint32_t sBitsetId;
            sFile >> sBitsetId >> sLine;
            PadCampaignBitsetBank[sBitsetId] = loadBitsetFromString(sLine, sOriginalCampaignCount, sSkippedCampaigns);
        }

        sFile >> sLine;
        check(sLine == "Banner", "Wrong file format");
        sFile >> sLine;
        check(sLine == "bitsets:", "Wrong file format");

        sBitsetCount = 0;
        sFile >> sBitsetCount;
        PadBannerBitsetBank.resize(sBitsetCount);
        for (size_t i = 0; i < sBitsetCount; i++)
        {
            uint32_t sBitsetId;
            sFile >> sBitsetId >> sLine;
            PadBannerBitsetBank[sBitsetId] = loadBitsetFromString(sLine, sOriginalBannerCount, sSkippedBanners);
        }
    }

    std::cout << "Bitset bank size " << PadCampaignBitsetBank.size()
              << " + " << PadBannerBitsetBank.size() << std::endl;

    size_t sSkippedPads = 0;
    {
        CTitle title("Reading pad index sFile: pad filters");

        sFile >> sLine;
        if (sLine != "pad_id/full/any/banner:")
            sFile >> sLine;
        check(sLine == "pad_id/full/any/banner:", "Wrong file format");

        size_t filterPadCount = 0;
        sFile >> filterPadCount;
        for (size_t i = 0; i < filterPadCount; i++)
        {
            uint32_t pad_id, full_id, any_id, banners_id;
            sFile >> pad_id >> full_id >> any_id >> banners_id;

            PadFilter pf;
            pf.m_All = &PadCampaignBitsetBank[full_id];
            pf.m_Any = &PadCampaignBitsetBank[any_id];
            pf.m_Banners = &PadBannerBitsetBank[banners_id];
            assert(pf.m_All->size() == IndexedCampaigns.size());
            assert(pf.m_Any->size() == IndexedCampaigns.size());
            assert(pf.m_Banners->size() == IndexedBanners.size());
            check(pf.m_All->size() == IndexedCampaigns.size(), "Wrong file format");
            check(pf.m_Any->size() == IndexedCampaigns.size(), "Wrong file format");
            check(pf.m_Banners->size() == IndexedBanners.size(), "Wrong file format");

            if (Pads.count(pad_id) == 0)
            {
                sSkippedPads++;
                continue;
            }
            else
            {
                Pads[pad_id].m_HasTargetingsOrFilters = true;
            }

            PadFilters[pad_id] = pf;
        }
    }

    std::cout << "Loaded pad filters " << PadFilters.size() << " (was skipped: " << sSkippedPads << ")" << std::endl;

    sFile >> sLine;
    if (sLine.empty())
        sFile >> sLine;
    check(sLine == "Done", "Wrong file format");
}

