#include "JsonDXX.h"
#include <json-c/json.h>
#if JSON_C_VERSION_NUM < ((0 << 16) | (16 << 8))
#error json-c 0.16 or newer is required
#endif
#include <stdexcept>
#include <utility>
#include <algorithm>
namespace {
json_object* clone(json_object* value) {
    if(!value) return nullptr;
    json_object* copy=nullptr;
    if(json_object_deep_copy(value,&copy,nullptr)!=0) { json_object_put(copy); throw std::runtime_error("JSON copy failed"); }
    return copy;
}
}
JsonDXX::JsonDXX() = default;
JsonDXX::JsonDXX(std::nullptr_t) {}
JsonDXX::JsonDXX(json_object* value):value_(value) {}
JsonDXX::JsonDXX(const char* value):JsonDXX(std::string(value)) {}
JsonDXX::JsonDXX(const std::string& value):value_(json_object_new_string_len(value.data(),static_cast<int>(value.size()))) {}
JsonDXX::JsonDXX(std::int64_t value):value_(json_object_new_int64(value)) {}
JsonDXX::JsonDXX(int value):JsonDXX(static_cast<std::int64_t>(value)) {}
JsonDXX::JsonDXX(bool value):value_(json_object_new_boolean(value)) {}
JsonDXX::JsonDXX(std::initializer_list<std::pair<std::string,JsonDXX>> fields):value_(json_object_new_object()) { for(const auto& f:fields) set(f.first,f.second); }
JsonDXX::JsonDXX(const JsonDXX& other):value_(clone(other.value_)) {}
JsonDXX::JsonDXX(JsonDXX&& other) noexcept:value_(std::exchange(other.value_,nullptr)) {}
JsonDXX& JsonDXX::operator=(JsonDXX other) { std::swap(value_,other.value_); return *this; }
JsonDXX::~JsonDXX() { json_object_put(value_); }
JsonDXX JsonDXX::array(const std::vector<JsonDXX>& values) { JsonDXX a(json_object_new_array()); for(const auto& v:values) json_object_array_add(a.value_,clone(v.value_)); return a; }
JsonDXX JsonDXX::parse(const std::string& raw) {
    auto* tok=json_tokener_new();
    json_tokener_set_flags(tok,JSON_TOKENER_STRICT|JSON_TOKENER_VALIDATE_UTF8);
    auto* obj=json_tokener_parse_ex(tok,raw.data(),static_cast<int>(raw.size()));
    const bool ok=json_tokener_get_error(tok)==json_tokener_success && json_tokener_get_parse_end(tok)==raw.size();
    json_tokener_free(tok);
    if(!ok) { json_object_put(obj); throw std::runtime_error("invalid UTF-8 JSON"); }
    return JsonDXX(obj);
}
JsonDXX JsonDXX::operator[](const std::string& key) const { json_object* v=nullptr; if(isObject()) json_object_object_get_ex(value_,key.c_str(),&v); return JsonDXX(clone(v)); }
void JsonDXX::set(const std::string& key,const JsonDXX& value) { if(!isObject()) throw std::runtime_error("expected object"); json_object_object_add(value_,key.c_str(),clone(value.value_)); }
std::vector<JsonDXX> JsonDXX::items() const { std::vector<JsonDXX> out; if(isArray()) for(std::size_t i=0;i<json_object_array_length(value_);++i) out.emplace_back(JsonDXX(clone(json_object_array_get_idx(value_,i)))); return out; }
bool JsonDXX::isObject() const { return json_object_is_type(value_,json_type_object); }
bool JsonDXX::isArray() const { return json_object_is_type(value_,json_type_array); }
bool JsonDXX::isString() const { return json_object_is_type(value_,json_type_string); }
bool JsonDXX::isNull() const { return value_==nullptr; }
bool JsonDXX::has(const std::string& key) const { json_object* v=nullptr; return isObject()&&json_object_object_get_ex(value_,key.c_str(),&v); }
std::string JsonDXX::text() const { return isString()?std::string(json_object_get_string(value_),json_object_get_string_len(value_)):std::string(); }
std::int64_t JsonDXX::integer() const { return json_object_get_int64(value_); }
bool JsonDXX::boolean() const { return json_object_is_type(value_,json_type_boolean)&&json_object_get_boolean(value_); }
std::string JsonDXX::dump() const { return json_object_to_json_string_ext(value_,JSON_C_TO_STRING_PLAIN); }

bool JsonDXX::isInteger() const { return json_object_is_type(value_,json_type_int); }
std::string JsonDXX::canonicalDump() const {
    if(isObject()) {
        std::vector<std::string> keys;
        json_object_object_foreach(value_,key,value) { (void)value; keys.emplace_back(key); }
        std::sort(keys.begin(),keys.end()); std::string out="{";
        for(const auto& key:keys) { if(out.size()>1) out+=","; out+=JsonDXX(key).dump()+":"+(*this)[key].canonicalDump(); }
        return out+"}";
    }
    if(isArray()) { std::string out="["; for(const auto& value:items()) { if(out.size()>1) out+=","; out+=value.canonicalDump(); } return out+"]"; }
    return dump();
}
