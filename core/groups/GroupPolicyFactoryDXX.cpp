#include "GroupPolicyFactoryDXX.h"
#include "QQGroupPolicyDXX.h"
#include "WeChatGroupPolicyDXX.h"
#include <stdexcept>
std::unique_ptr<GroupPolicyDXX> GroupPolicyFactoryDXX::create(const std::string& type) {
    if (type == "QQ") return std::make_unique<QQGroupPolicyDXX>();
    if (type == "WECHAT") return std::make_unique<WeChatGroupPolicyDXX>();
    throw std::invalid_argument("Unsupported group policy: " + type);
}
