#pragma once
#include "GroupPolicyDXX.h"
#include <memory>
#include <string>

class GroupPolicyFactoryDXX {
public:
    static std::unique_ptr<GroupPolicyDXX> create(const std::string& type);
};
