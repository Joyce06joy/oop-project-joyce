#pragma once
#include <cstdint>
#include <string>
#include <vector>

class SubgroupDXX {
public:
    SubgroupDXX(std::int64_t id, std::int64_t parent, std::int64_t creator,
                std::string name, std::vector<std::int64_t> members);
    std::int64_t id() const;
    std::int64_t parentId() const;
    std::int64_t creatorId() const;
    const std::string& name() const;
    const std::vector<std::int64_t>& members() const;
private:
    std::int64_t id_;
    std::int64_t parentId_;
    std::int64_t creatorId_;
    std::string name_;
    std::vector<std::int64_t> members_;
};
