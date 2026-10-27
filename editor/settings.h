// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
namespace editor {
// Folder-local authoring preferences. These never change campaign content.
struct EditorSettings {
	bool authoredStart = false;
	bool nativeMarkers = true;
};
EditorSettings loadEditorSettings(const std::filesystem::path &folder);
void saveEditorSettings(const std::filesystem::path &folder, const EditorSettings &);
} // namespace editor
