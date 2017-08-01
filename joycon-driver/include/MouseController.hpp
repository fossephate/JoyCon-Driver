#pragma once

#include <Windows.h>



class MouseController {

public:

	struct Position {
		int x;
		int y;
	} pos;

	struct RelativePos {
		float x;
		float y;
	} relPos;

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

	// move relative:
	// allow for sub pixel mouse movements
	void moveRel2(float x, float y) {

		int fx = (int)x;
		int fy = (int)y;

		float extraX = x - (int)x;
		float extraY = y - (int)y;

		relPos.x += extraX;
		relPos.y += extraY;

		if (relPos.x > 1) {
			relPos.x -= 1;
			fx += 1;
		}
		if (relPos.x < -1) {
			relPos.x += 1;
			fx -= 1;
		}
		if (relPos.y > 1) {
			relPos.y -= 1;
			fy += 1;
		}
		if (relPos.y < -1) {
			relPos.y += 1;
			fy -= 1;
		}


		getPos();
		SetCursorPos(pos.x + fx, pos.y + fy);
		getPos();
	}

	// move absolute:
	void moveAbs(int x, int y) {
		SetCursorPos(x, y);
		getPos();
	}

};