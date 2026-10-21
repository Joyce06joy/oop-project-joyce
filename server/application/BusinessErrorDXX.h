#pragma once
#include <stdexcept>
#include <string>
class BusinessErrorDXX : public std::runtime_error {
public:
    BusinessErrorDXX(std::string code,const std::string& message);
    const std::string& code() const;
private:
    std::string code_;
};
