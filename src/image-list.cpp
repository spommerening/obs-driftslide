#include "image-list.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <random>
#include <system_error>

namespace fs = std::filesystem;

static const char *IMAGE_EXTS[] = {
	".png", ".jpg", ".jpeg", ".bmp", ".tga", ".webp", ".gif",
};

ImageList::ImageList(const std::string &dir, bool random) : random_(random)
{
	scan(dir);
	std::sort(paths_.begin(), paths_.end());
	if (random_ && !paths_.empty())
		shuffle();
}

int ImageList::size() const
{
	return static_cast<int>(paths_.size());
}

std::string ImageList::next()
{
	if (paths_.empty())
		return {};

	std::string path = paths_[cursor_];
	cursor_++;

	if (cursor_ >= static_cast<int>(paths_.size())) {
		cursor_ = 0;
		if (random_)
			shuffle();
	}

	return path;
}

void ImageList::scan(const std::string &dir)
{
	if (dir.empty())
		return;

	std::error_code ec;
	for (const auto &entry : fs::directory_iterator(dir, ec)) {
		if (!entry.is_regular_file(ec))
			continue;

		std::string ext = entry.path().extension().string();
		for (char &c : ext)
			c = (char)tolower((unsigned char)c);

		for (const char *valid : IMAGE_EXTS) {
			if (ext == valid) {
				paths_.push_back(entry.path().string());
				break;
			}
		}
	}
}

void ImageList::shuffle()
{
	static std::random_device rd;
	static std::mt19937       rng(rd());
	std::shuffle(paths_.begin(), paths_.end(), rng);
}
