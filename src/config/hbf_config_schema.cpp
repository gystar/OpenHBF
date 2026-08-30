#include "openhbx/config/hbf_config_schema.h"

#include <cctype>
#include <sstream>
#include <vector>

namespace openhbx::config {
namespace {
std::string trim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
  return value;
}
}

ConfigResult<RawConfigTree> parse_hbf_yaml(const std::string& text) {
  RawConfigTree result;
  std::vector<ConfigIssue> issues;
  std::vector<std::string> parents;
  std::istringstream input(text);
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    const auto comment = line.find('#');
    if (comment != std::string::npos) line.erase(comment);
    if (trim(line).empty()) continue;
    std::size_t indent = 0;
    while (indent < line.size() && line[indent] == ' ') ++indent;
    if (indent % 2 != 0 || line.find('\t') != std::string::npos) {
      issues.push_back({ConfigErrorCode::ParseError, "", "indentation must use two spaces", line_number});
      continue;
    }
    const std::string content = line.substr(indent);
    if (!content.empty() && content.front() == '-') {
      issues.push_back({ConfigErrorCode::ParseError, "", "sequences are not supported by the HBF schema", line_number});
      continue;
    }
    const auto colon = content.find(':');
    if (colon == std::string::npos) {
      issues.push_back({ConfigErrorCode::ParseError, "", "expected key: value", line_number});
      continue;
    }
    const std::string key = trim(content.substr(0, colon));
    std::string value = trim(content.substr(colon + 1));
    if (key.empty()) {
      issues.push_back({ConfigErrorCode::ParseError, "", "empty key", line_number});
      continue;
    }
    const std::size_t depth = indent / 2;
    if (depth > parents.size()) {
      issues.push_back({ConfigErrorCode::ParseError, key, "indentation skips a map level", line_number});
      continue;
    }
    parents.resize(depth);
    std::string path;
    for (const auto& parent : parents) path += (path.empty() ? "" : ".") + parent;
    path += (path.empty() ? "" : ".") + key;
    if (value.empty()) {
      parents.push_back(key);
      continue;
    }
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                             (value.front() == '\'' && value.back() == '\''))) {
      value = value.substr(1, value.size() - 2);
    }
    if (!result.scalars.emplace(path, value).second)
      issues.push_back({ConfigErrorCode::DuplicateKey, path, "duplicate scalar", line_number});
  }
  if (!issues.empty()) return ConfigResult<RawConfigTree>::failure(std::move(issues));
  return ConfigResult<RawConfigTree>::success(std::move(result));
}
}  // namespace openhbx::config
