#ifndef __LOAD_CONFIG_FROM_JSON_HH__
#define __LOAD_CONFIG_FROM_JSON_HH__

#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <type_traits>
#include <vector>


#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include <systemc>


#include "Common/CommonDefine.hh"

constexpr const std::string_view headline =
    "--------------------------------------------------------------------------"
    "-------------------------------";

// Compile-time type trait to detect std::vector<T>
template <typename T> struct is_vector : std::false_type {};
template <typename T> struct is_vector<std::vector<T>> : std::true_type {};
template <typename T> inline constexpr bool is_vector_v = is_vector<T>::value;

// Compile-time type trait to detect std::set<T>
template <typename T> struct is_set : std::false_type {};
template <typename T> struct is_set<std::set<T>> : std::true_type {};
template <typename T> inline constexpr bool is_set_v = is_set<T>::value;

#define BEGIN_JSON_MAP(StructType)                                             \
  bool ParseStruct(const rapidjson::Value &json, StructType &obj) {            \
    if (!json.IsObject())                                                      \
      return false;                                                            \
    bool success = true;

#define JSON_FIELD(Type, FieldName)                                            \
  success &= GetValue(json, #FieldName, obj.FieldName);

// Macro for optional JSON fields. Missing fields do not trigger errors.
#define JSON_FIELD_OPTIONAL(Type, FieldName)                                   \
  success &= GetValueOptional(json, #FieldName, obj.FieldName);

#define JSON_NESTED_STRUCT(FieldName)                                          \
  if (!json.HasMember(#FieldName)) {                                           \
    std::cerr << "Missing JSON struct member: " << #FieldName << std::endl;    \
    std::abort();                                                              \
  }                                                                            \
  success &= ParseStruct(json[#FieldName], obj.FieldName);

#define END_JSON_MAP()                                                         \
  return success;                                                              \
  }

namespace dmu {

class LoadConfigFromJson {

protected:
  rapidjson::Document doc;
  std::string _filename;
  virtual ~LoadConfigFromJson() {}

  // Recursively parse a rapidjson::Value into a C++ object.
  // Supports primitive types, std::vector<T>, and nested structs via
  // ParseStruct.
  template <typename T>
  bool ParseJsonValue(const rapidjson::Value &val, T &out) {
    if constexpr (std::is_same_v<T, std::string>) {
      if (!val.IsString())
        return false;
      out = val.GetString();
    } else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
      if (std::is_unsigned_v<T>) {
        if (!val.IsUint())
          return false;
        out = val.GetUint();
      } else {
        if (!val.IsInt())
          return false;
        out = val.GetInt();
      }
    } else if constexpr (std::is_floating_point_v<T>) {
      if (!val.IsDouble())
        return false;
      out = val.GetDouble();
    } else if constexpr (std::is_same_v<T, bool>) {
      if (!val.IsBool())
        return false;
      out = val.GetBool();
    } else if constexpr (is_vector_v<T>) {
      if (!val.IsArray())
        return false;
      out.clear();
      for (rapidjson::SizeType i = 0; i < val.Size(); ++i) {
        typename T::value_type elem{};
        if (!ParseJsonValue(val[i], elem))
          return false;
        out.push_back(elem);
      }
    } else {
      return ParseStruct(val, out);
    }

    return true;
  }

  // Parse a JSON array into std::set<T>, aborting on duplicate elements.
  template <typename T>
  bool ParseJsonSet(const rapidjson::Value &val, const char *name, T &out) {
    if (!val.IsArray())
      return false;
    out.clear();
    for (rapidjson::SizeType i = 0; i < val.Size(); ++i) {
      typename T::value_type elem{};
      if (!ParseJsonValue(val[i], elem))
        return false;
      if (!out.insert(elem).second) {
        std::cerr << "Duplicate element '" << elem << "' in JSON array '"
                  << name << "' in file: '" << _filename << "'" << std::endl;
        std::abort();
      }
    }
    return true;
  }

public:
  virtual void LoadFromJson(const std::string &filename) {
    _filename = filename;
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
      std::cerr << "Failed to open file: " << filename << std::endl;
      std::abort();
    }

    std::string buffer((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

    doc.Parse(buffer.c_str());

    if (doc.HasParseError()) {
      std::cerr << "JSON parse error: " << doc.GetParseError() << std::endl;
      std::abort();
    }
  }

  virtual bool ParseJson() = 0;

  // Parse an optional JSON field. Returns true if the field is missing or
  // parsed successfully.
  template <typename T>
  bool GetValueOptional(const rapidjson::Value &parent, const char *name,
                        T &value) {
    if (!parent.HasMember(name)) {
      return true;
    }

    const rapidjson::Value &val = parent[name];
    if constexpr (is_set_v<T>) {
      if (!ParseJsonSet(val, name, value)) {
        std::cerr << "JSON type mismatch in optional set array: '" << name
                  << "' in file: '" << _filename << "'" << std::endl;
        return false;
      }
      return true;
    } else {
      if (!ParseJsonValue(val, value)) {
        std::cerr << "JSON type mismatch for optional field: '" << name
                  << "' in file: '" << _filename << "'" << std::endl;
        return false;
      }
      return true;
    }
  }

  template <typename T>
  bool GetValue(const rapidjson::Value &parent, const char *name, T &value) {
    if (!parent.HasMember(name)) {
      std::cerr << "Missing JSON member: '" << name << "' in file: '"
                << _filename << "'" << std::endl;
      return false;
    }

    const rapidjson::Value &val = parent[name];
    if constexpr (is_set_v<T>) {
      if (!ParseJsonSet(val, name, value)) {
        std::cerr << "JSON type mismatch in set array: '" << name
                  << "' in file: '" << _filename << "'" << std::endl;
        return false;
      }
      return true;
    } else {
      if (!ParseJsonValue(val, value)) {
        std::cerr << "JSON type mismatch: '" << name << "' in file: '"
                  << _filename << "'" << std::endl;
        return false;
      }
      return true;
    }
  }
};

} // namespace dmu

#endif // __LOAD_CONFIG_FROM_JSON_HH__