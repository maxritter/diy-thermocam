#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#ifndef O_READ
#define O_READ 0x01
#endif
#ifndef O_RDONLY
#define O_RDONLY 0x01
#endif
#ifndef O_WRITE
#define O_WRITE 0x02
#endif

struct FsFile {
	bool open_ = false;
	bool is_dir_ = false;
	std::string path_;
	std::string name_;
	std::vector<uint8_t> data_;
	size_t pos_ = 0;
	std::vector<std::string> dir_children_;
	size_t dir_idx_ = 0;

	bool open(const char* path, int oflag = O_RDONLY);
	bool openNext(FsFile* dir, int oflag = O_RDONLY);
	void close();
	bool isOpen() const { return open_; }
	bool isFile() const { return open_ && !is_dir_; }
	bool isDir() const { return open_ && is_dir_; }
	uint32_t fileSize() const { return open_ ? static_cast<uint32_t>(data_.size()) : 0u; }
	int read();
	size_t getName(char* buf, size_t len) const;
};

struct SdFs {
	bool chdir(const char* path);
	bool mkdir(const char* path);
	bool exists(const char* path);
	bool remove(const char* path);
	bool rmdir(const char* path);
};
