#pragma once
struct Bounce {
	Bounce(int = 0, unsigned long = 0) {}
	bool update() { return false; }
	int read() { return 0; }
};
