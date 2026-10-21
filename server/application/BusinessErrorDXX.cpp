#include "BusinessErrorDXX.h"
BusinessErrorDXX::BusinessErrorDXX(std::string code,const std::string& message):std::runtime_error(message),code_(std::move(code)) {}
const std::string& BusinessErrorDXX::code() const { return code_; }
