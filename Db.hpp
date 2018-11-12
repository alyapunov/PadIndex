#pragma once

#include <unordered_map>
#include <vector>

struct Pad
{
    explicit Pad(uint32_t id = 0) : m_Id(id) {}

    uint32_t m_Id;
    std::vector<Pad*> m_DirectParents;
    std::vector<Pad*> m_DirectChildren;

    // Do this pad have filters or targetings directly?
    bool m_HasTargetingsOrFilters = false;
    // For any pad there is a set of pads that have influence on what banners
    // can be shown on that pads. That could be the pad itself (if it has
    // targeting and/or filters) and all its ancestors, that have targetings
    // and/or fiters.
    // Here is the sorted array of such pad IDs. It will be filled during reindexing.
    std::vector<uint32_t> m_EffectivePads;
    // Flag that shows whether m_EffectivePads was build or not.
    bool m_EffectivePadsAreBuilt = false;
    // Some pad can have identical m_EffectivePads vectors. Let's mark them with
    // the same group ID. If two pads will have equal group ID then they'll have
    // equal m_EffectivePads and vice-versa it they'll have different group ID
    // then they'll have different m_EffectivePads.
    // Actually this ID is pad_id of one of the pads in the group.
    uint32_t m_EffectivePadsGroupId = 0;
};

struct User
{
    User(uint32_t id = 0, uint32_t parent_id = 0) : m_Id(id), m_ParentId(parent_id), m_Parent(nullptr) {}

    uint32_t m_Id;
    uint32_t m_ParentId;
    User* m_Parent;

    std::vector<Pad*> m_PositiveTargetingPad;
    std::vector<Pad*> m_NegatineTargetingPad;
};

struct Package
{
    explicit Package(uint32_t id = 0) : m_Id(id) {}

    uint32_t m_Id;

    std::vector<Pad*> m_PositiveTargetingPad;
    std::vector<Pad*> m_NegatineTargetingPad;
};

struct Campaign
{
    Campaign(uint32_t id = 0, uint32_t user_id = 0, uint32_t package_id = 0)
        : m_Id(id), m_UserId(user_id), m_User(nullptr), m_PackageId(package_id), m_Package(nullptr) {}

    uint32_t m_Id;
    uint32_t m_UserId;
    User* m_User;
    uint32_t m_PackageId;
    Package* m_Package;

    // Apart of all other stuff in this module, I didn't dumped banners of a campaigns.
    // That's why this vector will be loaded later with all filters (in LoadPrecalculatedFilters()).
    std::vector<uint32_t> m_BannerIds;

    std::vector<Pad*> m_PositiveTargetingPad;
    std::vector<Pad*> m_NegatineTargetingPad;
};

extern std::unordered_map<uint32_t, Pad> Pads;
extern std::unordered_map<uint32_t, Package> Packages;
extern std::unordered_map<uint32_t, User> Users;
extern std::unordered_map<uint32_t, Campaign> Campaigns;

void loadDb();
