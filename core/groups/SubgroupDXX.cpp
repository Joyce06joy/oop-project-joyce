#include "SubgroupDXX.h"
#include <utility>
SubgroupDXX::SubgroupDXX(std::int64_t id, std::int64_t parent, std::int64_t creator,
                        std::string name, std::vector<std::int64_t> members)
    : id_(id), parentId_(parent), creatorId_(creator), name_(std::move(name)), members_(std::move(members)) {}
std::int64_t SubgroupDXX::id() const { return id_; }
std::int64_t SubgroupDXX::parentId() const { return parentId_; }
std::int64_t SubgroupDXX::creatorId() const { return creatorId_; }
const std::string& SubgroupDXX::name() const { return name_; }
const std::vector<std::int64_t>& SubgroupDXX::members() const { return members_; }
