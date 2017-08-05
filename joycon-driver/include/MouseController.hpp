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
		INPUT input;
		input.type = INPUT_MOUSE;
		input.mi.mouseData = 0;
		input.mi.time = 0;
		input.mi.dx = x;
		input.mi.dy = y;
		input.mi.dwFlags = MOUSEEVENTF_MOVE;
		SendInput(1, &input, sizeof(input));
		getPos();
	}

	// move relative:
	// allow for sub pixel mouse movements
	void moveRel2(float x, float y/*, float thresholdX = 0.02, float thresholdY = 0.02*/) {

		int fx = (int)x;
		int fy = (int)y;

		float extraX = x - (int)x;
		float extraY = y - (int)y;

		//if (abs(extraX) > thresholdX) {
		//	relPos.x += extraX;
		//}
		//if (abs(extraY) > thresholdY) {
		//	relPos.y += extraY;
		//}

		relPos.x *= 0.8f;
		relPos.y *= 0.8f;

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


		// move relative:
		INPUT input;
		input.type = INPUT_MOUSE;
		input.mi.mouseData = 0;
		input.mi.time = 0;
		input.mi.dx = fx;
		input.mi.dy = fy;
		input.mi.dwFlags = MOUSEEVENTF_MOVE;
		SendInput(1, &input, sizeof(input));
		//ZeroMemory(&input, sizeof(input));
	}

	// move absolute:
	void moveAbs(int x, int y) {
		INPUT input;
		input.type = INPUT_MOUSE;
		input.mi.mouseData = 0;
		input.mi.time = 0;
		input.mi.dx = x*(65536 / GetSystemMetrics(SM_CXSCREEN));
		input.mi.dy = y*(65536 / GetSystemMetrics(SM_CYSCREEN));
		input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE;
		SendInput(1, &input, sizeof(input));
		getPos();
	}

};