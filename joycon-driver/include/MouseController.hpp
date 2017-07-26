#pragma once

#include <Windows.h>



class MouseController {

public:

	struct Position {
		int x;
		int y;
	} pos;

	// get mouse position:
	void getPos() {
		POINT p;
		if (GetCursorPos(&p)) {
			pos.x = p.x;
			pos.y = p.y;
		}
	}

	// move relative:
	void moveRel(int x, int y) {
		getPos();
		SetCursorPos(pos.x + x, pos.y + y);
		getPos();
	}

	// move absolute:
	void moveAbs(int x, int y) {
		SetCursorPos(x, y);
		getPos();
	}

};