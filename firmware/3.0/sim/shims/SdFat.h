#pragma once
struct FsFile {
	bool open(const char*, int = 0) { return false; }
	void close() {}
};
struct SdFs {
	bool chdir(const char*) { return true; }
	bool mkdir(const char*) { return true; }
	bool exists(const char*) { return false; }
	bool remove(const char*) { return true; }
	bool rmdir(const char*) { return true; }
};
