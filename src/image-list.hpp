#pragma once

#include <string>
#include <vector>

class ImageList {
public:
	explicit ImageList(const std::string &dir, bool random);

	int size() const;
	// Returns absolute path to the next image and advances the cursor.
	std::string next();

private:
	std::vector<std::string> paths_;
	int cursor_ = 0;
	bool random_;

	void scan(const std::string &dir);
	void shuffle();
};
