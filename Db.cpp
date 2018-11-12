#include "Db.hpp"

#include <iostream>

#include "DbFileReader.hpp"
#include "Utils.hpp"

std::unordered_map<uint32_t, Pad> Pads;
std::unordered_map<uint32_t, Package> Packages;
std::unordered_map<uint32_t, User> Users;
std::unordered_map<uint32_t, Campaign> Campaigns;

void loadDb()
{
    CDbFileReader sReader("Data/pad.txt", {"pad_id"});
    while (sReader.ReadLine())
    {
        uint32_t id = std::stoi(sReader.Fields()[0]);
        Pads.emplace(id, Pad(id));
        Pads[id] = Pad(id);
    }
    std::cout << "Num pads: " << Pads.size() << std::endl;

    sReader.Open("Data/pad_relation.txt", {"pad_id", "parent_pad_id"});
    size_t sNumRelations = 0, sNumBadRelations = 0;
    while (sReader.ReadLine())
    {
        uint32_t pad_id = std::stoi(sReader.Fields()[0]);
        uint32_t parent_pad_id = std::stoi(sReader.Fields()[1]);
        if (Pads.count(pad_id) == 0 || Pads.count(parent_pad_id) == 0)
        {
            sNumBadRelations++;
            continue;
        }
        Pads[pad_id].m_DirectParents.push_back(&Pads[parent_pad_id]);
        Pads[parent_pad_id].m_DirectChildren.push_back(&Pads[pad_id]);
        sNumRelations++;
    }
    std::cout << "Num relations: " << sNumRelations
              << " (bad: " << sNumBadRelations << ")" << std::endl;

    sReader.Open("Data/user.txt", {"id", "parent_user_id"});
    while (sReader.ReadLine())
    {
        uint32_t id = std::stoi(sReader.Fields()[0]);
        uint32_t parent_id = std::stoi(sReader.Fields()[1]);
        Users[id] = User(id, parent_id);
    }
    size_t sNumBadUsers = 0;
    for (auto itr = Users.begin(); itr != Users.end(); ++itr)
    {
        User& user = itr->second;
        if (user.m_ParentId != 0)
        {
            if (Users.count(user.m_ParentId) == 0)
                sNumBadUsers++;
            else
                user.m_Parent = &Users[user.m_ParentId];
        }
    }
    std::cout << "Num users: " << Users.size()
              << " (bad: " << sNumBadUsers << ")" << std::endl;

    sReader.Open("Data/campaign.txt", {"id", "user_id", "package_id"});
    size_t sNumBadCampaigns = 0;
    while (sReader.ReadLine())
    {
        uint32_t id = std::stoi(sReader.Fields()[0]);
        uint32_t user_id = std::stoi(sReader.Fields()[1]);
        uint32_t package_id = std::stoi(sReader.Fields()[2]);
        Packages[package_id] = Package(package_id);
        Campaign c(id, user_id, package_id);
        c.m_Package = &Packages[package_id];
        if (Users.count(user_id) == 0)
            sNumBadCampaigns++;
        else
            c.m_User = &Users[user_id];
        Campaigns[id] = c;
    }
    std::cout << "Num campaigns: " << Campaigns.size()
              << " (bad: " << sNumBadCampaigns << ")" << std::endl;

    sReader.Open("Data/targeting_user.txt", {"user_id", "pad_id", "type"});
    size_t sNumTargetingUser = 0;
    size_t sNumBadTargetingUser = 0;
    while (sReader.ReadLine())
    {
        uint32_t id = std::stoi(sReader.Fields()[0]);
        uint32_t pad_id = std::stoi(sReader.Fields()[1]);
        const std::string& type = sReader.Fields()[2];
        check(type == "positive" || type == "negative", "Wrong targeting type!");
        bool positive = type == "positive";
        if (Users.count(id) == 0 || Pads.count(pad_id) == 0)
        {
            sNumBadTargetingUser++;
        }
        else
        {
            Pads[pad_id].m_HasTargetingsOrFilters = true;
            sNumTargetingUser++;
            if (positive)
                Users[id].m_PositiveTargetingPad.push_back(&Pads[pad_id]);
            else
                Users[id].m_NegatineTargetingPad.push_back(&Pads[pad_id]);
        }
    }
    std::cout << "Num targetings: " << sNumTargetingUser
              << " (bad: " << sNumBadTargetingUser << ")" << std::endl;

    sReader.Open("Data/targeting_package.txt", {"package_id", "pad_id", "type"});
    size_t sNumTargetingPackage = 0;
    size_t sNumBadTargetingPackage = 0;
    while (sReader.ReadLine())
    {
        uint32_t id = std::stoi(sReader.Fields()[0]);
        uint32_t pad_id = std::stoi(sReader.Fields()[1]);
        const std::string& type = sReader.Fields()[2];
        check(type == "positive" || type == "negative", "Wrong targeting type!");
        bool positive = type == "positive";
        if (Pads.count(pad_id) == 0)
        {
            sNumBadTargetingPackage++;
        }
        else
        {
            Pads[pad_id].m_HasTargetingsOrFilters = true;
            sNumTargetingPackage++;
            if (positive)
                Packages[id].m_PositiveTargetingPad.push_back(&Pads[pad_id]);
            else
                Packages[id].m_NegatineTargetingPad.push_back(&Pads[pad_id]);
        }
    }
    std::cout << "Num targetings: " << sNumTargetingPackage
              << " (bad: " << sNumBadTargetingPackage << ")" << std::endl;

    sReader.Open("Data/targeting_campaign.txt", {"campaign_id", "pad_id", "type"});
    size_t sNumTargetingCampaign = 0;
    size_t sNumBadTargetingCampaign = 0;
    while (sReader.ReadLine())
    {
        uint32_t id = std::stoi(sReader.Fields()[0]);
        uint32_t pad_id = std::stoi(sReader.Fields()[1]);
        const std::string& type = sReader.Fields()[2];
        check(type == "positive" || type == "negative", "Wrong targeting type!");
        bool positive = type == "positive";
        if (Campaigns.count(id) == 0 || Pads.count(pad_id) == 0)
        {
            sNumBadTargetingCampaign++;
        }
        else
        {
            Pads[pad_id].m_HasTargetingsOrFilters = true;
            sNumTargetingCampaign++;
            if (positive)
                Campaigns[id].m_PositiveTargetingPad.push_back(&Pads[pad_id]);
            else
                Campaigns[id].m_NegatineTargetingPad.push_back(&Pads[pad_id]);
        }
    }
    std::cout << "Num targetings: " << sNumTargetingCampaign
              << " (bad: " << sNumBadTargetingCampaign << ")" << std::endl;
}
