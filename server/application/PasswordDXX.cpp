#include "PasswordDXX.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include <openssl/sha.h>
#include <array>
#include <sstream>
#include <vector>
#include <stdexcept>
namespace {
constexpr int iterations=600000;
std::string hex(const unsigned char* data,std::size_t n) { const char* chars="0123456789abcdef"; std::string s; for(std::size_t i=0;i<n;++i) { s+=chars[data[i]>>4]; s+=chars[data[i]&15]; } return s; }
std::vector<unsigned char> unhex(const std::string& s) { if(s.size()%2) throw std::runtime_error("bad hex"); std::vector<unsigned char> v; for(std::size_t i=0;i<s.size();i+=2) { auto digit=[](char c)->int { if(c>='0'&&c<='9') return c-'0'; if(c>='a'&&c<='f') return c-'a'+10; throw std::runtime_error("bad hex"); }; v.push_back(static_cast<unsigned char>(digit(s[i])*16+digit(s[i+1]))); } return v; }
std::string derive(const std::string& p,const std::vector<unsigned char>& salt,int rounds) { std::array<unsigned char,32> out{}; if(PKCS5_PBKDF2_HMAC(p.data(),static_cast<int>(p.size()),salt.data(),static_cast<int>(salt.size()),rounds,EVP_sha256(),static_cast<int>(out.size()),out.data())!=1) throw std::runtime_error("password derivation failed"); return hex(out.data(),out.size()); }
}
std::string PasswordDXX::randomId() { std::array<unsigned char,16> bytes{}; if(RAND_bytes(bytes.data(),bytes.size())!=1) throw std::runtime_error("random unavailable"); return hex(bytes.data(),bytes.size()); }
std::string PasswordDXX::hash(const std::string& p) { auto salt=randomId(); return "pbkdf2-sha256$"+std::to_string(iterations)+"$"+salt+"$"+derive(p,unhex(salt),iterations); }
bool PasswordDXX::verify(const std::string& p,const std::string& encoded) {
    try {
        std::istringstream in(encoded); std::string alg,rounds,salt,expected,extra;
        std::getline(in,alg,'$'); std::getline(in,rounds,'$'); std::getline(in,salt,'$'); std::getline(in,expected,'$');
        if(alg!="pbkdf2-sha256"||rounds!=std::to_string(iterations)||salt.size()!=32||expected.size()!=64||std::getline(in,extra,'$')) return false;
        auto actual=derive(p,unhex(salt),iterations); return CRYPTO_memcmp(actual.data(),expected.data(),64)==0;
    } catch(...) { return false; }
}
std::string PasswordDXX::fingerprint(const std::string& value) { std::array<unsigned char,SHA256_DIGEST_LENGTH> bytes{}; SHA256(reinterpret_cast<const unsigned char*>(value.data()),value.size(),bytes.data()); return hex(bytes.data(),bytes.size()); }
