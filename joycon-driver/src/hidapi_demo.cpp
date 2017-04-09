
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

#define SERIAL_LEN 18


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

	wchar_t *serial;

	hid_device *handle;

	int left_right;    // 1: left, 2: right


	int controller_id; // -1: unassigned

	uint16_t buttons;

	bool buttons2[32];
	std::string buttonStatesString;

	int8_t dstick; // TODO get analog stick

	uint8_t stick_v;
	uint8_t stick_h;

	struct Stick {
		int horizontal;
		int vertical;
		int unknown;

		uint8_t horizontal2;
		uint8_t vertical2;
		uint8_t unknown2;
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


t_joycon g_joycons[8];

JOYSTICK_POSITION_V2 iReport; // The structure that holds the full position data



void found_joycon(struct hid_device_info *dev) {
	
	
	t_joycon *jc = nullptr;

	int i;

	for (i = 0; i < 8; i++) {
		// found joycon
		if (g_joycons[i].left_right == 0) {
			jc = &g_joycons[i];
			break;
		}
	}

	if (jc) {
		if (dev->product_id == JOYCON_PRODUCT_L) {
			jc->left_right = 1;
		} else {
			jc->left_right = 2;
		}

		jc->serial = wcsdup(dev->serial_number);
		jc->buttons = 0;
		jc->controller_id = -1;
		printf("Found joycon %c %i: %ls %s\n", L_OR_R(jc->left_right), i,
		       jc->serial, dev->path);
		errno = 0;
		jc->handle = hid_open_path(dev->path);
		hid_set_nonblocking(jc->handle, 1);
		if (jc->handle == NULL) {
			printf("Could not open serial %ls: %s\n", g_joycons[i].serial, strerror(errno));
		}
	}
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
	if (jc->controller_id == -1) {
		printf("Joycon %c (Unattached): ", L_OR_R(jc->left_right));
	} else {
		printf("Joycon %c (Controller %d): ", L_OR_R(jc->left_right), jc->controller_id);
	}

	for (int i = 0; i < 16; i++) {
		if (jc->buttons & (1 << button_map[i].bit)) {
			printf("1");
			//printf("%s", button_map[i].name);
			//printf("\033[0m\033[1m");
		} else {
			printf("0");
			//printf("\033[0m");
		}
		//printf("%s", button_map[i].name);
		//printf("\033[0m ");
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
		
		//printf("\033[0m ");
	}
	//printf(jc->buttonStatesString.c_str());
	printf("\n");
}

void print_stick2(t_joycon *jc) {

	printf("Joycon %c (Unattached): ", L_OR_R(jc->left_right));


	//printf("%d", jc->stick.horizontal)
	//printf("\n");

	//printf("%d %d\n", jc->stick.horizontal, jc->stick.vertical);
	printf("%d %d %d\n", jc->stick.horizontal, jc->stick.vertical, jc->stick.unknown);

	
}




const char *const dstick_names[9] = {"Up", "UR", "Ri", "DR", "Do",
                                     "DL", "Le", "UL", "Neu"};

void print_dstick(t_joycon *jc) {
	if (jc->controller_id == -1) {
		printf("Joycon %c (Unattached): ", L_OR_R(jc->left_right));
	} else {
		printf("Joycon %c (Controller %d): ", L_OR_R(jc->left_right),
		       jc->controller_id);
	}

	printf("%s\n", dstick_names[jc->dstick]);
}




void handle_input(t_joycon *jc, uint8_t *packet, int len) {




	if (packet[0] == 63) {

		uint16_t old_buttons = jc->buttons;
		int8_t old_dstick = jc->dstick;
		// button update
		jc->buttons = packet[1] + packet[2] * 256;
		jc->dstick = packet[3];
		//if (jc->buttons != old_buttons) {
		//	print_buttons(jc);
		//}
		//if (jc->dstick != old_dstick) {
		//	print_dstick(jc);
		//}
	}

	
	//printf(jc->buttons);



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

		

		if (jc->controller_id == -1) {
			//printf("Joycon %c (Unattached): ", L_OR_R(jc->left_right));
		} else {
			//printf("Joycon %c (Controller %d): ", L_OR_R(jc->left_right), jc->controller_id);
		}

		if (len != 6 * 8 + 1) {
			//printf("[!!!] Different length for packet 0x21\n");
		}

		for (int i = 1; i < len; i++) {
			//printf("%02X ", (uint8_t)packet[i]);
			if (i % 8 == 0) {
				//printf("\n");
			}
		}

		uint8_t *pckt = packet + 2;// byte 16/17

		std::string buttonStates1;
		std::string buttonStates2;
		//std::string buttonStates2;
		//buttonStates1.resize(24);
		//buttonStates2.resize(16);

		//bool buttonStatesBools[32];

		if (pckt[0] == 0x8E) {

			// get button states
			//printf("Button status: ");
			for (int i = 1; i < 4; i++) {
				for (int b = 0; b < 8; b++) {

					char c = (pckt[i] & (1 << b)) ? '1' : '0';

					buttonStates1 += c;
				}
			}

			// re-order bits:

			// Left JoyCon:
			if (jc->left_right == 1) {
				for (int i = 15; i > 7; --i) {
					buttonStates2 += buttonStates1[i];
				}
				//buttonStates2 += " ";
				for (int i = 23; i > 15; --i) {
					buttonStates2 += buttonStates1[i];
				}
			
			}

			// Right JoyCon:
			if (jc->left_right == 2) {
				for (int i = 15; i > 7; --i) {
					buttonStates2 += buttonStates1[i];
				}

				for (int i = 7; i > -1; --i) {
					buttonStates2 += buttonStates1[i];
				}
			}

			//printf(buttonStates2.c_str());
			//printf("\n");

			jc->buttonStatesString = buttonStates2;

		}

		uint8_t *stick_data = nullptr;
		if (jc->left_right == 1) {
			stick_data = pckt + 4;
			//printf("Stick L: %02X %02X %02X\n", pckt[4], pckt[5], pckt[6]);
		} else if(jc->left_right == 2) {
			stick_data = pckt + 7;
			//printf("Stick R: %02X %02X %02X\n", pckt[7], pckt[8], pckt[9]);
		}


		//if (jc->side == 1) {
		//	 //Left JoyCon:
		//	 //packet[5];
		//	jc->stick_h = ((packet[6] & 0x0F) << 4) | ((packet[6] & 0xF0) >> 4);
		//	jc->stick_v = packet[7];
		//} else {
		//	// packet[8];
		//	jc->stick_h = ((packet[9] & 0x0F) << 4) | ((packet[9] & 0xF0) >> 4);
		//	jc->stick_v = packet[10];
		//}

		uint8_t stick_unknown = stick_data[0];
		
		//uint8_t stick_unknown = ((stick_data[0] & 0x0F) << 4) | ((stick_data[0] & 0xF0) >> 4);

		//it appears that the X component of the stick data isn't just nibble reversed,
		//specifically, the second nibble of second byte is combined with the first nibble of the first byte
		//to get the correct X stick value:
		
		//uint8_t stick_horizontal = ((stick_data[1] & 0x0F) << 4) | ((stick_data[1] & 0xF0) >> 4);// horizontal axis is reversed
		uint8_t stick_horizontal = ((stick_data[1] & 0x0F) << 4) | ((stick_data[0] & 0xF0) >> 4);// horizontal axis is reversed / combined with byte 0
		//uint8_t stick_horizontal = stick_data[1];
		
		uint8_t stick_vertical = stick_data[2];

		//jc->stick.unknown2 = stick_unknown;
		jc->stick.horizontal2 = stick_horizontal;
		jc->stick.vertical2 = stick_vertical;

		//jc->stick.unknown = -128 + (int)(unsigned int)stick_unknown;
		jc->stick.horizontal = -128 + (int)(unsigned int)stick_horizontal;
		jc->stick.vertical = -128 + (int)(unsigned int)stick_vertical;

		//printf("%d ", jc->stick.horizontal);
		//printf("\n");

		//printf("%02X ", stick_data[0]);
		//printf("%02X ", stick_data[1]);
		//printf("\n");
	}




	if (packet[0] != 63 && packet[0] != 0x21) {
		printf("Unknown packet: ");
		for (int i = 0; i < len; i++) {
			printf("%02X ", packet[i]);
		}
		//system("cls");
		printf("\n");
	}

	//for (int i = 0; i < 40; ++i) {
	//}

	//printf("\r");
	//fflush(stdout);
	//for (int i = 0; i < (len-30); ++i) {
	//	//printf("%02X ", packet[i]);
	//	printf("%02X ", packet[i]);

	//}
	//printf("\n");


	//print_buttons(jc);
	//print_buttons2(jc);
	//print_stick2(jc);
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








void updatevJoyDevice(/*int deviceID, */t_joycon *jc) {

	// If it's the left JoyCon update device 1,
	// if it's the right JoyCon update device 2
	//if (jc->left_right == 1) {
	//	DevID = 1;
	//} else {
	//	DevID = 2;
	//}

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
		x = 240 * (jc->stick.horizontal - 10) + 17000;
		y = 240 * (jc->stick.vertical - 10) + 15000;
		//z = 200 * (jc->stick.unknown - 10) + 15000;
	} else {
		x = 240 * (jc->stick.horizontal - 10) + 19000;
		y = 240 * (jc->stick.vertical - 10) + 22000;
		//z = 200 * (jc->stick.unknown - 10) + 15000;
	}

	// Set position data of 3 first axes
	//iReport.wAxisZ = 250 * jc->stick.unknown;
	iReport.wAxisX = x;
	iReport.wAxisY = y;

	// Set button data
	long btns = strtol(jc->buttonStatesString.c_str(), nullptr, 2);

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

	devs = hid_enumerate(0x0, 0x0);
	cur_dev = devs;

	while (cur_dev) {
		if (cur_dev->vendor_id == JOYCON_VENDOR) {
			if (cur_dev->product_id == JOYCON_PRODUCT_L ||
			    cur_dev->product_id == JOYCON_PRODUCT_R) {
				found_joycon(cur_dev);
			}
		}
		cur_dev = cur_dev->next;
	}
	hid_free_enumeration(devs);



	t_joycon *jc;







	while(true) {

		for (int jci = 0; jci < 8; jci++) {
			jc = &g_joycons[jci];
			if (!jc->handle) {
				continue;
			}

			updatevJoyDevice(jc);


			//updatevJoyDevice(1, jc);
			//updatevJoyDevice(2, jc);

			//if (jci > 0 && jci < 3) {
			//	updatevJoyDevice(jci, jc);
			//}

			//if (jci == 1) {
			//	updatevJoyDevice(1, jc);
			//}
			//if (jci == 2) {
			//	updatevJoyDevice(2, jc);
			//}
			

			//uint8_t rbuf[0x31];
			//memset(rbuf, 0, 0x31);

			//int read_res;
			//read_res = hid_read_timeout((hid_device *)jc->handle, rbuf, 0x31, /*JC_READ_TIMEOUT*/2);

			//if (rbuf[0] == 0x21) {
			//	handle_input(jc, rbuf+1, res);
			//}



			// read response
			res = hid_read(jc->handle, buf, 65);
			if (res < 0) {
				printf("Unable to read from joycon %i, disconnecting\n", jci);
				jc->handle = 0;
				jc->left_right = 0;
			} else if (res > 0) {
				// writing ID 1 causes it to reply with a packet 0x21 (33)

				uint16_t old_buttons = jc->buttons;
				handle_input(jc, buf, res);

				// if L was pressed
				//if (!(old_buttons & (1 << 14)) && (jc->buttons & (1 << 14))) {
					memset(buf, 0, 65);
					buf[0] = 0x01;
					buf[1] = 0x91;
					buf[2] = 0x11;
					buf[3] = 0;
					buf[4] = 0;
					buf[5] = 0;
					buf[6] = 0;
					buf[7] = 0;
					buf[8] = 0;
					errno = 0;
					res = hid_write(jc->handle, buf, 9);
					if (res < 0) {
						printf("write error: %s\n", strerror(errno));
					}
				//}

				// when ZL pressed:
				if (!(old_buttons & (1 << 15)) && (jc->buttons & (1 << 15))) {
					//C: 0x19 0x1 0x3 0x11 0x0 0x92 0x0 0xa 0x0 0x0 0xef 0xa2 0x10 0x8 0x0 0x1 0x40 0x40 0x0 0x1 0x40 0x40

					printf("Sending: 01 ");

					memset(buf, 0, 65);
					//buf[0] = 0x01;
					buf[0] = 0x01;
					for (int i = 1; i < 17; ++i) {
						buf[i] = (int)(rand0t1() * 10);
						printf("%02X ", buf[i]);
					}
					printf("\n");
					//buf[1] = 0x1;
					//buf[2] = 0x3;
					//buf[3] = 0x11;
					//buf[4] = 0x0;
					//buf[5] = 0x92;
					//buf[6] = 0x0;
					//buf[7] = 0xa;
					//buf[8] = 0x0;
					//buf[9] = 0x0;
					//buf[10] = 0xef;
					//buf[11] = 0xa2;
					//buf[12] = 0x10;
					//buf[13] = 0x8;
					//buf[14] = 0x0;
					//buf[15] = 0x5;
					//buf[16] = 0x40;
					//buf[17] = 0x40;
					//buf[18] = 0x0;
					//buf[19] = 0x1;
					//buf[20] = 0x40;
					//buf[21] = 0x40;

					errno = 0;
					res = hid_write(jc->handle, buf, 17);
					if (res < 0) {
						printf("write error: %s\n", strerror(errno));
					}

				}




				//// if ZL was pressed
				//if (!(old_buttons & (1 << 15)) && (jc->buttons & (1 << 15))) {
				//	memset(buf, 0, 65);
				//	buf[0] = 0x01;
				//	buf[1] = 0;
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
			}
		}
		//Sleep(15);
		Sleep(5);
	}

	RelinquishVJD(1);
	return 0;
}
