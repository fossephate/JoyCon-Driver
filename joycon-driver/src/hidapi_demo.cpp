
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <stdbool.h>
//#include <unistd.h>
#include <wchar.h>


#include <Windows.h>

#include <hidapi.h>

#include <fstream>
#include <iostream>

#include <bitset>

#include <random>

/* vJoy */
// Monitor Force Feedback (FFB) vJoy device
#include "stdafx.h"
//#include "Devioctl.h"
#include "public.h"
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include "vjoyinterface.h"
#include "Math.h"
/* end of vJoy includes */


#pragma warning(disable:4996)

#define JOYCON_VENDOR 0x057e
#define JOYCON_PRODUCT_L 0x2006
#define JOYCON_PRODUCT_R 0x2007

#define JOYCON_PRODUCT_CHARGE_GRIP 0x200e

#define SERIAL_LEN 18

//#define WEIRD_VIBRATION_TEST
//#define DEBUG_PRINT


float rand0t1() {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(0.0f, 1.0f);
	float rnd = dis(gen);
	return rnd;
}






uint8_t joycon_crc_lookup[256] = {
    0x00, 0x8D, 0x97, 0x1A, 0xA3, 0x2E, 0x34, 0xB9, 0xCB, 0x46, 0x5C, 0xD1,
    0x68, 0xE5, 0xFF, 0x72, 0x1B, 0x96, 0x8C, 0x01, 0xB8, 0x35, 0x2F, 0xA2,
    0xD0, 0x5D, 0x47, 0xCA, 0x73, 0xFE, 0xE4, 0x69, 0x36, 0xBB, 0xA1, 0x2C,
    0x95, 0x18, 0x02, 0x8F, 0xFD, 0x70, 0x6A, 0xE7, 0x5E, 0xD3, 0xC9, 0x44,
    0x2D, 0xA0, 0xBA, 0x37, 0x8E, 0x03, 0x19, 0x94, 0xE6, 0x6B, 0x71, 0xFC,
    0x45, 0xC8, 0xD2, 0x5F, 0x6C, 0xE1, 0xFB, 0x76, 0xCF, 0x42, 0x58, 0xD5,
    0xA7, 0x2A, 0x30, 0xBD, 0x04, 0x89, 0x93, 0x1E, 0x77, 0xFA, 0xE0, 0x6D,
    0xD4, 0x59, 0x43, 0xCE, 0xBC, 0x31, 0x2B, 0xA6, 0x1F, 0x92, 0x88, 0x05,
    0x5A, 0xD7, 0xCD, 0x40, 0xF9, 0x74, 0x6E, 0xE3, 0x91, 0x1C, 0x06, 0x8B,
    0x32, 0xBF, 0xA5, 0x28, 0x41, 0xCC, 0xD6, 0x5B, 0xE2, 0x6F, 0x75, 0xF8,
    0x8A, 0x07, 0x1D, 0x90, 0x29, 0xA4, 0xBE, 0x33, 0xD8, 0x55, 0x4F, 0xC2,
    0x7B, 0xF6, 0xEC, 0x61, 0x13, 0x9E, 0x84, 0x09, 0xB0, 0x3D, 0x27, 0xAA,
    0xC3, 0x4E, 0x54, 0xD9, 0x60, 0xED, 0xF7, 0x7A, 0x08, 0x85, 0x9F, 0x12,
    0xAB, 0x26, 0x3C, 0xB1, 0xEE, 0x63, 0x79, 0xF4, 0x4D, 0xC0, 0xDA, 0x57,
    0x25, 0xA8, 0xB2, 0x3F, 0x86, 0x0B, 0x11, 0x9C, 0xF5, 0x78, 0x62, 0xEF,
    0x56, 0xDB, 0xC1, 0x4C, 0x3E, 0xB3, 0xA9, 0x24, 0x9D, 0x10, 0x0A, 0x87,
    0xB4, 0x39, 0x23, 0xAE, 0x17, 0x9A, 0x80, 0x0D, 0x7F, 0xF2, 0xE8, 0x65,
    0xDC, 0x51, 0x4B, 0xC6, 0xAF, 0x22, 0x38, 0xB5, 0x0C, 0x81, 0x9B, 0x16,
    0x64, 0xE9, 0xF3, 0x7E, 0xC7, 0x4A, 0x50, 0xDD, 0x82, 0x0F, 0x15, 0x98,
    0x21, 0xAC, 0xB6, 0x3B, 0x49, 0xC4, 0xDE, 0x53, 0xEA, 0x67, 0x7D, 0xF0,
    0x99, 0x14, 0x0E, 0x83, 0x3A, 0xB7, 0xAD, 0x20, 0x52, 0xDF, 0xC5, 0x48,
    0xF1, 0x7C, 0x66, 0xEB};



// joycon_1 is R, joycon_2 is L
#define CONTROLLER_TYPE_BOTH 0x1
// joycon_1 is L, joycon_2 is R
#define CONTROLLER_TYPE_LONLY 0x2
// joycon_1 is R, joycon_2 is -1
#define CONTROLLER_TYPE_RONLY 0x3

#define L_OR_R(lr) (lr == 1 ? 'L' : (lr == 2 ? 'R' : '?'))

typedef struct s_joycon {

	hid_device *handle;
	wchar_t *serial;

	unsigned char r_buf[65];// read buffer
	unsigned char w_buf[65];// write buffer, what to hid_write to the device

	int left_right = 0;// 1: left joycon, 2: right joycon

	uint16_t buttons;

	bool buttons2[32];

	int8_t dstick;

	uint8_t battery;

	struct Stick {
		int horizontal;
		int vertical;

		uint8_t horizontal2;
		uint8_t vertical2;
	} stick;

	struct Gyroscope {
		int x;
		int y;
		int z;
	} gyro;

	struct Accelerometer {
		int x;
		int y;
		int z;
	} accel;


	uint8_t newButtons[3];
} t_joycon;


std::vector<t_joycon> g_joycons;

JOYSTICK_POSITION_V2 iReport; // The structure that holds the full position data



void found_joycon(struct hid_device_info *dev) {
	
	
	t_joycon jc;

	int i = g_joycons.size();

	if (dev->product_id == JOYCON_PRODUCT_CHARGE_GRIP) {
		if (dev->interface_number == 0) {
			jc.left_right = 2;// right joycon
		} else if (dev->interface_number == 1) {
			jc.left_right = 1;// left joycon
		}
	}

	if (dev->product_id == JOYCON_PRODUCT_L) {
		jc.left_right = 1;// left joycon
	} else if (dev->product_id == JOYCON_PRODUCT_R) {
		jc.left_right = 2;// right joycon
	}

	jc.serial = wcsdup(dev->serial_number);
	jc.buttons = 0;

	printf("Found joycon %c %i: %ls %s\n", L_OR_R(jc.left_right), i, jc.serial, dev->path);
	errno = 0;
	jc.handle = hid_open_path(dev->path);
	// set non-blocking:
	hid_set_nonblocking(jc.handle, 1);
	//if (jc.handle == nullptr) {
	//	printf("Could not open serial %ls: %s\n", g_joycons[i].serial, strerror(errno));
	//}

	g_joycons.push_back(jc);

}

struct s_button_map {
	int bit;
	char *name;
};

struct s_button_map button_map[16] = {
    {0, "D"},   {1, "R"},   {2, "L"},   {3, "U"},    {4, "SL"},  {5, "SR"},
    {6, "?"},   {7, "?"},   {8, "-"},   {9, "+"},    {10, "LS"}, {11, "RS"},
    {12, "Ho"}, {13, "Sc"}, {14, "LR"}, {15, "ZLR"},
};

void print_buttons(t_joycon *jc) {

	for (int i = 0; i < 16; i++) {
		if (jc->buttons & (1 << button_map[i].bit)) {
			printf("1");
		} else {
			printf("0");
		}
	}
	printf("\n");
}


void print_buttons2(t_joycon *jc) {

	printf("Joycon %c (Unattached): ", L_OR_R(jc->left_right));
	
	for (int i = 0; i < 32; i++) {
		if (jc->buttons2[i]) {
			printf("1");
		} else {
			printf("0");
		}
		
	}
	printf("\n");
}


void printBitsFromInt(uint16_t *s) {
	for (int i = 0; i < 16; i++) {
		if (s[i] & (1 << i)) {
			printf("1");
		} else {
			printf("0");
		}
	}
	printf("\n");
}

void print_stick2(t_joycon *jc) {

	printf("Joycon %c (Unattached): ", L_OR_R(jc->left_right));


	//printf("%d", jc->stick.horizontal)
	//printf("\n");

	//printf("%d %d\n", jc->stick.horizontal, jc->stick.vertical);
	printf("%d %d\n", jc->stick.horizontal, jc->stick.vertical);

	
}




const char *const dstick_names[9] = {"Up", "UR", "Ri", "DR", "Do", "DL", "Le", "UL", "Neu"};



void hex_dump(unsigned char *buf, int len) {
	for (int i = 0; i < len; i++) {
		printf("%02x ", buf[i]);
	}
	printf("\n");
}



void print_dstick(t_joycon *jc) {

	printf("%s\n", dstick_names[jc->dstick]);
}

void hid_exchange(hid_device *handle, unsigned char *buf, int len) {
	if (!handle) return; //TODO: idk I just don't like this to be honest

	hid_write(handle, buf, len);

	int res = hid_read(handle, buf, 0x41);
#ifdef DEBUG_PRINT
	hex_dump(buf, 0x40);
#endif
}




void handle_input(t_joycon *jc, uint8_t *packet, int len) {



	// 63
	// Upright: LDUR
	// Sideways: DRLU
	if (packet[0] == 0x3F) {

		uint16_t old_buttons = jc->buttons;
		int8_t old_dstick = jc->dstick;
		// button update
		//jc->buttons = packet[1] + packet[2] * 256;

		jc->dstick = packet[3];
		//if (jc->buttons != old_buttons) {
		//	print_buttons(jc);
		//}
		//if (jc->dstick != old_dstick) {
		//	print_dstick(jc);
		//}
	}

	
	//printf(jc->buttons);


	// 33
	if (packet[0] == 0x21) {

		{
			// Button status:
			// Byte 1: 0x8E
			//  Byte 2
			//   Bit 0: JR Y
			//   Bit 1: JR X
			//   Bit 2: JR B
			//   Bit 3: JR A
			//   Bit 4: JR SR
			//   Bit 5: JR SL
			//   Bit 6: JR R
			//   Bit 7: JR ZR
			// Byte 3
			//   Bit 0: Minus
			//   Bit 1: Plus
			//   Bit 2: RStick
			//   Bit 3: LStick
			//   Bit 4: Home
			//   Bit 5: Capture
			// Byte 4
			//   Bit 0: JL Down
			//   Bit 1: JL Up
			//   Bit 2: JL Right
			//   Bit 3: JL Left
			//   Bit 4: JL SR
			//   Bit 5: JL SL
			//   Bit 6: JL L
			//   Bit 7: JL ZL

		}


		uint8_t *pckt = packet + 3;// byte 16/17?
		// Upright: DURL
		// Sideways: RLUD
		//if (pckt[0] == 0x8E && 0) {

			//uint8_t buttons[3];
			//buttons[0] = pckt[0];
			//buttons[1] = pckt[1];
			//buttons[2] = pckt[2];

			//if (jc->left_right == 1) {
			//	uint8_t b = pckt[2];// reversed
			//	//b = ((b * 0x0802LU & 0x22110LU) | (b * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
			//	//jc->buttons = pckt[3] | (b << 8);
			//	jc->buttons = pckt[3] | b;
			//}

			//if (jc->left_right == 2) {
			//	uint8_t b = pckt[2];// reversed
			//	//b = ((b * 0x0802LU & 0x22110LU) | (b * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
			//	//jc->buttons = pckt[1] | (b << 8);
			//}
			//printBitsFromInt(&jc->buttons);

		//}



		if (!false) {

			std::string buttonStates1;
			std::string buttonStates2;

			// get button states
			//printf("Button status: ");
			//for (int i = 0; i < 3; i++) {
			//	for (int b = 0; b < 8; b++) {

			//		char c = (pckt[i] & (1 << b)) ? '1' : '0';

			//		buttonStates1 += c;
			//	}
			//}

			// re-order bits:

			// Left JoyCon:
			if (jc->left_right == 1) {
				//for (int i = 15; i > 7; --i) {
				//	buttonStates2 += buttonStates1[i];
				//}
				////buttonStates2 += " ";
				//for (int i = 23; i > 15; --i) {
				//	buttonStates2 += buttonStates1[i];
				//}

				for (int i = 0; i < 8; ++i) {
					//int b = i + 16;
					//buttonStates2 += buttonStates1[b];
					char c = (pckt[2] & (1 << i)) ? '1' : '0';
					buttonStates2 += c;
				}

				for (int i = 0; i < 8; ++i) {
					//int b = i + 8;
					//buttonStates2 += buttonStates1[b];
					char c = (pckt[1] & (1 << i)) ? '1' : '0';
					buttonStates2 += c;
				}


			}

			// Right JoyCon:
			if (jc->left_right == 2) {
				//for (int i = 15; i > 7; --i) {
				//	buttonStates2 += buttonStates1[i];
				//}

				//for (int i = 7; i > -1; --i) {
				//	buttonStates2 += buttonStates1[i];
				//}

				for (int i = 15; i > 7; --i) {
					buttonStates2 += buttonStates1[i];
				}

				for (int i = 7; i > -1; --i) {
					buttonStates2 += buttonStates1[i];
				}
			}

			printf(buttonStates2.c_str());
			printf("\n");

			//uint16_t states = std::stoi(buttonStates2.c_str(), nullptr, 2);
			//jc->buttons = states;
		}





























		// stick data:

		uint8_t *stick_data = nullptr;
		if (jc->left_right == 1) {
			stick_data = packet + 6;
			//printf("Stick L: %02X %02X %02X\n", pckt[4], pckt[5], pckt[6]);
		} else if(jc->left_right == 2) {
			stick_data = packet + 9;
			//printf("Stick R: %02X %02X %02X\n", pckt[7], pckt[8], pckt[9]);
		}

		

		// it appears that the X component of the stick data isn't just nibble reversed,
		// specifically, the second nibble of second byte is combined with the first nibble of the first byte
		// to get the correct X stick value:
		uint8_t stick_horizontal = ((stick_data[1] & 0x0F) << 4) | ((stick_data[0] & 0xF0) >> 4);// horizontal axis is reversed / combined with byte 0
		
		uint8_t stick_vertical = stick_data[2];

		//jc->stick.unknown2 = stick_unknown;
		jc->stick.horizontal2 = stick_horizontal;
		jc->stick.vertical2 = stick_vertical;

		jc->stick.horizontal = -128 + (int)(unsigned int)stick_horizontal;
		jc->stick.vertical = -128 + (int)(unsigned int)stick_vertical;

		jc->battery = (stick_data[1] & 0xF0) >> 4;
	}




	if (packet[0] != 0x3F && packet[0] != 0x21) {
		//printf("Unknown packet: ");
		//hex_dump(packet, len);
	}


	//print_buttons(jc);
	//print_buttons2(jc);
	//print_stick2(jc);
}


int joycon_init(hid_device *handle, const char *name) {
	unsigned char buf[0x40];
	memset(buf, 0, 0x40);

	//Get MAC Left
	memset(buf, 0x00, 0x40);
	buf[0] = 0x80;
	buf[1] = 0x01;
	hid_exchange(handle, buf, 0x2);

	if (buf[2] == 0x3) {
		printf("%s disconnected!\n", name);
		return -1;
	} else {
		printf("Found %s, MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", name, buf[9], buf[8], buf[7], buf[6], buf[5], buf[4]);
	}

	//Do handshaking
	memset(buf, 0x00, 0x40);
	buf[0] = 0x80;
	buf[1] = 0x02;
	hid_exchange(handle, buf, 0x2);

	

	
#ifndef WEIRD_VIBRATION_TEST
	// Switch baudrate to 3Mbit
	printf("Switching baudrate...\n");
	memset(buf, 0x00, 0x40);
	buf[0] = 0x80;
	buf[1] = 0x03;
	hid_exchange(handle, buf, 0x2);

	//Do handshaking again at new baudrate so the firmware pulls pin 3 low?
	memset(buf, 0x00, 0x40);
	buf[0] = 0x80;
	buf[1] = 0x02;
	hid_exchange(handle, buf, 0x2);

	//Only talk HID from now on
	memset(buf, 0x00, 0x40);
	buf[0] = 0x80;
	buf[1] = 0x04;
	hid_exchange(handle, buf, 0x2);
#endif

	return 0;
}


void hid_dual_exchange(hid_device *handle_l, hid_device *handle_r, unsigned char *buf_l, unsigned char *buf_r, int len) {
	if (handle_l && buf_l) {
		hid_set_nonblocking(handle_l, 1);
		hid_write(handle_l, buf_l, len);
		hid_read(handle_l, buf_l, 65);
#ifdef DEBUG_PRINT
		hex_dump(buf_l, 0x40);
#endif
		hid_set_nonblocking(handle_l, 0);
	}

	if (handle_r && buf_r) {
		hid_set_nonblocking(handle_r, 1);
		hid_write(handle_r, buf_r, len);
		hid_read(handle_r, buf_r, 65);
#ifdef DEBUG_PRINT
		hex_dump(buf_r, 0x40);
#endif
		hid_set_nonblocking(handle_r, 0);
	}
}





int acquirevJoyDevice(int deviceID) {

	int stat;

	// Get the driver attributes (Vendor ID, Product ID, Version Number)
	if (!vJoyEnabled()) {
		_tprintf("Function vJoyEnabled Failed - make sure that vJoy is installed and enabled\n");
		int dummy = getchar();
		stat = -2;
		throw;
		//goto Exit;
	} else {
		wprintf(L"Vendor: %s\nProduct :%s\nVersion Number:%s\n", static_cast<TCHAR *> (GetvJoyManufacturerString()), static_cast<TCHAR *>(GetvJoyProductString()), static_cast<TCHAR *>(GetvJoySerialNumberString()));
	};

	// Get the status of the vJoy device before trying to acquire it
	VjdStat status = GetVJDStatus(deviceID);

	switch (status) {
		case VJD_STAT_OWN:
			_tprintf("vJoy device %d is already owned by this feeder\n", deviceID);
			break;
		case VJD_STAT_FREE:
			_tprintf("vJoy device %d is free\n", deviceID);
			break;
		case VJD_STAT_BUSY:
			_tprintf("vJoy device %d is already owned by another feeder\nCannot continue\n", deviceID);
			return -3;
		case VJD_STAT_MISS:
			_tprintf("vJoy device %d is not installed or disabled\nCannot continue\n", deviceID);
			return -4;
		default:
			_tprintf("vJoy device %d general error\nCannot continue\n", deviceID);
			return -1;
	};

	// Acquire the vJoy device
	if (!AcquireVJD(deviceID)) {
		_tprintf("Failed to acquire vJoy device number %d.\n", deviceID);
		int dummy = getchar();
		stat = -1;
		//goto Exit;
		throw;
	} else {
		_tprintf("Acquired device number %d - OK\n", deviceID);
	}
}








void updatevJoyDevice(t_joycon *jc) {

	UINT DevID = jc->left_right;

	PVOID pPositionMessage;
	UINT	IoCode = LOAD_POSITIONS;
	UINT	IoSize = sizeof(JOYSTICK_POSITION);
	// HID_DEVICE_ATTRIBUTES attrib;
	BYTE id = 1;
	UINT iInterface = 1;

	// Set destination vJoy device
	id = (BYTE)DevID;
	iReport.bDevice = id;



	

	// Set Stick data
	// todo: calibration of some kind
	int x, y, z;
	if (DevID == 1) {
		x = 240 * (jc->stick.horizontal - 10) + 18000;
		y = 240 * (jc->stick.vertical - 10) + 15000;
		//z = 200 * (jc->stick.unknown - 10) + 15000;
	} else {
		x = 240 * (jc->stick.horizontal - 10) + 18000;
		y = 240 * (jc->stick.vertical - 10) + 22000;
		//z = 200 * (jc->stick.unknown - 10) + 15000;
	}

	// Set position data of 3 first axes
	//iReport.wAxisZ = 250 * jc->stick.unknown;
	iReport.wAxisX = x;
	iReport.wAxisY = y;

	// Set button data
	long btns = jc->buttons;

	//printf(jc->buttonStatesString.c_str());
	//printf("\n");

	iReport.lButtons = btns;

	// Send position data to vJoy device
	pPositionMessage = (PVOID)(&iReport);
	if (!UpdateVJD(DevID, pPositionMessage)) {
		printf("Feeding vJoy device number %d failed - try to enable device then press enter\n", DevID);
		getchar();
		AcquireVJD(DevID);
	}
}




void updatevJoyDevice2(t_joycon *jc, bool combineJoyCons) {

	UINT DevID;

	if (!combineJoyCons) {
		DevID = jc->left_right;
	} else {
		DevID = 1;
	}

	PVOID pPositionMessage;
	UINT	IoCode = LOAD_POSITIONS;
	UINT	IoSize = sizeof(JOYSTICK_POSITION);
	// HID_DEVICE_ATTRIBUTES attrib;
	BYTE id = 1;
	UINT iInterface = 1;

	// Set destination vJoy device
	id = (BYTE)DevID;
	iReport.bDevice = id;



	// todo: calibration of some kind
	int leftJoyConXOffset = 16000;
	int leftJoyConYOffset = 13000;

	int rightJoyConXOffset = 15000;
	int rightJoyConYOffset = 19000;

	// multipliers, these shouldn't really be different from one another
	int leftJoyConXMultiplier = 240;
	int leftJoyConYMultiplier = 240;
	int rightJoyConXMultiplier = 240;
	int rightJoyConYMultiplier = 240;

	// Set Stick data
	
	int x, y, z;
	int rx = 0;
	int ry = 0;
	int rz = 0;

	if (!combineJoyCons) {

		if (jc->left_right == 1) {
			x = leftJoyConXMultiplier * (jc->stick.horizontal) + leftJoyConXOffset;
			y = leftJoyConYMultiplier * (jc->stick.vertical) + leftJoyConYOffset;
		} else if (jc->left_right == 2) {
			x = rightJoyConXMultiplier * (jc->stick.horizontal) + rightJoyConXOffset;
			y = rightJoyConYMultiplier * (jc->stick.vertical) + rightJoyConYOffset;
		}
	} else {
		if (jc->left_right == 1) {
			x = leftJoyConXMultiplier * (jc->stick.horizontal) + leftJoyConXOffset;
			y = leftJoyConYMultiplier * (jc->stick.vertical) + leftJoyConYOffset;
		} else if (jc->left_right == 2) {
			rx = rightJoyConXMultiplier * (jc->stick.horizontal) + rightJoyConXOffset;
			ry = rightJoyConYMultiplier * (jc->stick.vertical) + rightJoyConYOffset;
		}
	}

	// both left and right joycons
	if (!combineJoyCons) {
		iReport.wAxisX = x;
		iReport.wAxisY = y;
	} else {

		// Set position data
		if (jc->left_right == 1) {
			iReport.wAxisX = x;
			iReport.wAxisY = y;
		}

		if (jc->left_right == 2) {
			iReport.wAxisXRot = rx;
			iReport.wAxisYRot = ry;
		}
	}

	// Set button data

	long btns;
	if (!combineJoyCons) {
		//btns = strtol(jc->buttonStatesString.c_str(), nullptr, 2);
		btns = jc->buttons;
	} else {
		if (jc->left_right == 2) {
			//btns = iReport.lButtons | strtol(jc->buttonStatesString.c_str(), nullptr, 2) << 16;
			btns = iReport.lButtons | jc->buttons << 16;
		} else {
			//btns = strtol(jc->buttonStatesString.c_str(), nullptr, 2);
			btns = jc->buttons;
		}
	}

	iReport.lButtons = btns;

	// Send position data to vJoy device
	pPositionMessage = (PVOID)(&iReport);
	if (!UpdateVJD(DevID, pPositionMessage)) {
		printf("Feeding vJoy device number %d failed - try to enable device then press enter\n", DevID);
		getchar();
		AcquireVJD(DevID);
	}
	//Sleep(2);
}











int main(int argc, char *argv[]) {

	// get vJoy Device 1
	acquirevJoyDevice(1);
	// get vJoy Device 2
	acquirevJoyDevice(2);


	int res;
	unsigned char buf[65];
	#define MAX_STR 255
	wchar_t wstr[MAX_STR];
	hid_device *handle;
	int i;

	// Enumerate and print the HID devices on the system
	struct hid_device_info *devs, *cur_dev;

	res = hid_init();

	devs = hid_enumerate(0x0, 0x0);
	cur_dev = devs;

	bool usingGrip = false;

	while (cur_dev) {
		// identify by vendor:
		if (cur_dev->vendor_id == JOYCON_VENDOR) {

			// bluetooth, left or right joycon
			if (cur_dev->product_id == JOYCON_PRODUCT_L || cur_dev->product_id == JOYCON_PRODUCT_R) {
				found_joycon(cur_dev);
			}
			// USB, charging grip
			if (cur_dev->product_id == JOYCON_PRODUCT_CHARGE_GRIP) {
				found_joycon(cur_dev);
				usingGrip = true;
			}
		}


		cur_dev = cur_dev->next;
	}
	hid_free_enumeration(devs);


	t_joycon *jc;


	bool combineJoyCons = false;




	if (usingGrip) {

		unsigned char buf_l[65];
		unsigned char buf_r[65];

		hid_device *handle_l = g_joycons[0].handle;
		hid_device *handle_r = g_joycons[1].handle;

		joycon_init(handle_l, "Left JoyCon");
		joycon_init(handle_r, "Right JoyCon");

	}

	//hid_set_nonblocking(g_joycons[0].handle, 1);
	//hid_set_nonblocking(g_joycons[1].handle, 1);
	



	while(true) {

			//for (int i = 0; i < g_joycons.size(); i++) {
			//	jc = &g_joycons[i];
			//	if (!jc->handle) {
			//		continue;
			//	}

				//updatevJoyDevice(jc);
				//updatevJoyDevice2(jc, combineJoyCons);



				//// read response
				//res = hid_read(jc->handle, buf, 64);
				//if (res < 0) {
				//	printf("Unable to read from joycon %i, disconnecting\n", i);
				//	jc->handle = 0;
				//	jc->left_right = 0;
				//} else if (res > 0) {

					// writing ID 1 causes it to reply with a packet 0x21 (33)

					//uint16_t old_buttons = jc->buttons;
					//handle_input(jc, buf, res);

					// if L was pressed
					//if (!(old_buttons & (1 << 14)) && (jc->buttons & (1 << 14))) {
						// request update
						//memset(buf, 0, 65);
						//buf[0] = 0x01;
						//buf[1] = 0x91;
						//buf[2] = 0x11;
						//buf[3] = 0;
						//buf[4] = 0;
						//buf[5] = 0;
						//buf[6] = 0;
						//buf[7] = 0;
						//buf[8] = 0;
						//errno = 0;
						//res = hid_write(jc->handle, buf, 9);
						//if (res < 0) {
						//	printf("write error: %s\n", strerror(errno));
						//}
					//}

					//if (jc->left_right == 1) {
					//	memset(buf, 0, 64);
					//	buf[0] = 0x80; // 80     Do custom command
					//	buf[1] = 0x92; // 92     Post-handshake type command
					//	buf[2] = 0x00; // 0001   u16 second part size
					//	buf[3] = 0x01;
					//	buf[8] = 0x1F; // 1F     Get input command
					//	errno = 0;
					//	res = hid_write(jc->handle, buf, 9);
					//	if (res < 0) {
					//		printf("write error: %s\n", strerror(errno));
					//	}

					//	res = hid_read(jc->handle, buf, 64);
					//	hex_dump(buf, 64);
					//}




					// testing:

					// findings:
					// 0x6 on byte 10 causes a disconnect
					// 0x3 on byte 10 gets some wierd response that doesn't stop until the joycon disconnects
					// 0x10, 0x11, and 0x12 on byte 0 don't produce an error

					// when ZL pressed:
					////if (!(old_buttons & (1 << 15)) && (jc->buttons & (1 << 15))) {
					//	//printf("Sending: ");

					//	memset(buf, 0, 65);
					//	//buf[0] = 0x1;
					//	//buf[0] = 0x19;
					//	// 10-12
					//	//for (int i = 0; i < 1; ++i) {
					//		//int rand = (int)(rand0t1() * 255);
					//		////if (rand == 6) continue;
					//		//buf[0] = rand;
					//		//printf("%02X ", buf[0]);
					//	//}
					//	//printf("\n");


					//	buf[0] = 0x10;
					//	//buf[1] = 0x81;
					//	//buf[0] = (int)(rand0t1() * 255);
					//	//buf[1] = 0x80;
					//	//buf[1] = 0x01;
					//	//buf[2] = 0x03;
					//	//buf[3] = 0x08;
					//	//buf[4] = 0x00;
					//	//buf[5] = 0x92;
					//	//buf[6] = 0x00;
					//	//buf[7] = 0x01;
					//	//buf[8] = 0x00;
					//	//buf[9] = 0x00;
					//	//buf[10] = 0x69;
					//	//buf[11] = 0x2D;
					//	//buf[12] = 0x1F;
					//	//buf[13] = 0x8;
					//	//buf[14] = 0x0;
					//	//buf[15] = 0x5;
					//	//buf[16] = 0x40;
					//	//buf[17] = 0x40;
					//	//buf[18] = 0x0;
					//	//buf[19] = 0x1;
					//	//buf[20] = 0x40;
					//	//buf[21] = 0x40;

					//	errno = 0;
					//	res = hid_write(jc->handle, buf, 65);
					//	if (res < 0) {
					//		printf("write error: %s\n", strerror(errno));
					//	}

					////}




					//// if ZL was pressed
					//if (!(old_buttons & (1 << 15)) && (jc->buttons & (1 << 15))) {
					//	memset(buf, 0, 65);
					//	buf[0] = 0x01;
					//	buf[1] = 0x00;
					//	buf[2] = 0x92;
					//	buf[3] = 0x00;
					//	buf[4] = 0x00;
					//	buf[5] = 0x01;
					//	buf[6] = 0;
					//	buf[7] = 0;
					//	buf[8] = 0x69;
					//	buf[9] = 0x2d;
					//	buf[10] = 0;
					//	buf[11] = 0;
					//	buf[12] = 0;
					//	errno = 0;
					//	res = hid_write(jc->handle, buf, 9);
					//	if (res < 0) {
					//		printf("write error: %s\n", strerror(errno));
					//	}
					//}

				//}
			//}
		



		//hid_device *handle_l = g_joycons[0].handle;
		//hid_device *handle_r = g_joycons[1].handle;


#ifdef WEIRD_VIBRATION_TEST
		for (int l = 0x10; l < 0x20; l++) {
			for (int i = 0; i < 8; i++) {
				for (int k = 0; k < 256; k++) {
					memset(buf, 0, 0x40);
					buf[0] = 0x80;
					buf[1] = 0x92;
					buf[2] = 0x0;
					buf[3] = 0xa;
					buf[4] = 0x0;
					buf[5] = 0x0;
					buf[8] = 0x10;
					for (int j = 0; j <= 8; j++) {
						buf[10 /*+ i*/] = 0x1;//(i + j) & 0xFF;
					}

					// Set frequency to increase
					buf[10 + 0] = k;
					buf[10 + 4] = k;

					//if (buf[1]) {
					//	memcpy(buf[1], buf[0], 0x40);
					//}

					hid_dual_exchange(handle_l, handle_r, buf, buf, 0x40);
					printf("Sent %x %x %u\n", i & 0xFF, l, k);
				}
			}
		}
#endif



		for (int i = 0; i < g_joycons.size(); ++i) {
			t_joycon *jc = &g_joycons[i];
			if (!jc->handle) {
				continue;
			}

			updatevJoyDevice2(jc, combineJoyCons);

			if (usingGrip) {
				memset(buf, 0, 65);
				buf[0] = 0x80; // 80     Do custom command
				buf[1] = 0x92; // 92     Post-handshake type command
				buf[2] = 0x00; // 0001   u16 second part size
				buf[3] = 0x01;
				buf[8] = 0x1F; // 1F     Get input command
			} else {
				buf[0] = 0x1;
				buf[1] = 0x91;
				buf[2] = 0x11;
			}

			hid_set_nonblocking(jc->handle, 1);
			// send data
			hid_write(jc->handle, buf, 9);
			
			
			// read response
			res = hid_read(jc->handle, buf, 65);// returns length of actual bytes read
			
			if (res > 0) {
				// handle input data
				handle_input(jc, buf, res);
			} else if (res < 0) {
				printf("Unable to read from joycon %i, disconnecting\n", i);
				jc->handle = 0;
				jc->left_right = 0;
				continue;
			}

		}








		
		//Sleep(15);
		Sleep(1);
	}

	RelinquishVJD(1);
	RelinquishVJD(2);
	return 0;
}
