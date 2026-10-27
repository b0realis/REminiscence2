// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "content.h"
#include <map>
#include <memory>
namespace editor {
struct Image {
	int width = 0, height = 0;
	std::vector<uint32_t> pixels;
};
Image readImage(const std::filesystem::path &);
void writePNG(const Image &, const std::filesystem::path &);
uint32_t rgba(int r, int g, int b, int a = 255);
void over(uint32_t &destination, uint32_t source);
void blit(Image &, const Image &, const Art &, int x, int y, int w, int h);
void rectangle(Image &, int x, int y, int w, int h, uint32_t color);
struct Images {
	std::filesystem::path root;
	std::map<std::string, std::shared_ptr<Image>> cache;
	const Image &get(const std::string &relative);
	void clear() { cache.clear(); }
};
Image composeRoom(const Project &, int room, Images &, Layer last = Layer::Front);
void validateImages(const Project &, Images &);
std::vector<Issue> imageIssues(const Project &, Images &);
void drawModule(Image &, const Module &, int part, bool open, uint64_t tick, Images &);
void drawNode(Image &, const Node &, bool active, uint64_t age, Images &, bool editing = false);
} // namespace editor
