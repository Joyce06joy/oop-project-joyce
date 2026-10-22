#pragma once
#include <string>

struct GroupContextDXX {
    GroupContextDXX();
    bool active = true;
    bool member = false;
    bool owner = false;
    bool creator = false;
    bool targetMember = false;
    std::string role;
    std::string targetRole;
};
