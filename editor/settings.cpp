// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings.h"
#include "atomic_file.h"
#include "file.h"
#include "third_party/editor/json.hpp"
#include <fstream>
#include <stdexcept>
namespace editor {
using json = nlohmann::json;
EditorSettings loadEditorSettings(const std::filesystem::path &folder) {
	const auto path = folder / ".editor-settings.json";
	if (!std::filesystem::exists(path))
		return {};
	const auto size = std::filesystem::file_size(path);
	if (size > 1024)
		throw std::runtime_error("Editor settings exceed 1 KiB");
	std::ifstream file(path, std::ios::binary);
	std::string text(size_t(size), '\0');
	if (!file.read(text.data(), text.size()))
		throw std::runtime_error("Cannot read editor settings");
	const auto j = json::parse(text, [](int depth, json::parse_event_t, json &) {
		if (depth > 3)
			throw std::runtime_error("Editor settings nesting is too deep");
		return true;
	});
	if (!j.is_object() || !j.contains("schema") || !j.at("schema").is_number_integer() ||
	    j.at("schema") != 1 || !j.contains("authoredStart") ||
	    !j.at("authoredStart").is_boolean() || !j.contains("nativeMarkers") ||
	    !j.at("nativeMarkers").is_boolean())
		throw std::runtime_error("Unsupported or malformed editor settings");
	return {j.at("authoredStart").get<bool>(), j.at("nativeMarkers").get<bool>()};
}
void saveEditorSettings(const std::filesystem::path &folder, const EditorSettings &settings) {
	std::filesystem::create_directories(folder);
	const auto text = json{{"schema", 1},
	                       {"authoredStart", settings.authoredStart},
	                       {"nativeMarkers", settings.nativeMarkers}}
	                      .dump(2) +
	                  '\n';
	std::string reason;
	if (!writeAtomicFile((folder / ".editor-settings.json").u8string(), false,
	                     [&](File &f) { f.write(text.data(), uint32_t(text.size())); }, reason))
		throw std::runtime_error("Cannot save editor settings: " + reason);
}
} // namespace editor
