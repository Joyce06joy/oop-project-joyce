#pragma once
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>
struct json_object;
// Small value-semantic adapter. No JSON coercion is used at trust boundaries.
class JsonDXX {
public:
    JsonDXX();
    JsonDXX(std::nullptr_t);
    JsonDXX(const char* value);
    JsonDXX(const std::string& value);
    JsonDXX(std::int64_t value);
    JsonDXX(int value);
    JsonDXX(bool value);
    JsonDXX(std::initializer_list<std::pair<std::string, JsonDXX>> fields);
    JsonDXX(const JsonDXX& other);
    JsonDXX(JsonDXX&& other) noexcept;
    JsonDXX& operator=(JsonDXX other);
    ~JsonDXX();
    static JsonDXX array(const std::vector<JsonDXX>& values = {});
    static JsonDXX parse(const std::string& raw);
    JsonDXX operator[](const std::string& key) const;
    void set(const std::string& key, const JsonDXX& value);
    std::vector<JsonDXX> items() const;
    bool isObject() const;
    bool isArray() const;
    bool isString() const;
    bool isNull() const;
    bool has(const std::string& key) const;
    std::string text() const;
    std::int64_t integer() const;
    bool boolean() const;
    std::string dump() const;
private:
    explicit JsonDXX(json_object* value);
    json_object* value_ = nullptr;
};
