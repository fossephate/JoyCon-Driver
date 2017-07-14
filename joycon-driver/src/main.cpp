
#include <Windows.h>
#pragma comment(lib, "user32.lib")

#include <bitset>
#include <random>
#include <stdafx.h>
#include <string.h>
#include <chrono>

#include <hidapi.h>



#include "public.h"
#include "vjoyinterface.h"

#include "packet.h"
#include "joycon.h"


#pragma warning(disable:4996)

#define JOYCON_VENDOR 0x057e

#define JOYCON_L_BT 0x2006
#define JOYCON_R_BT 0x2007

#define PRO_CONTROLLER 0x2009

#define JOYCON_CHARGING_GRIP 0x200e

#define SERIAL_LEN 18


//#define DEBUG_PRINT
//#define LED_TEST



// joycon_1 is R, joycon_2 is L
#define CONTROLLER_TYPE_BOTH 0x1
// joycon_1 is L, joycon_2 is R
#define CONTROLLER_TYPE_LONLY 0x2
// joycon_1 is R, joycon_2 is -1
#define CONTROLLER_TYPE_RONLY 0x3

#define L_OR_R(lr) (lr == 1 ? 'L' : (lr == 2 ? 'R' : '?'))

unsigned short product_ids[] = { JOYCON_L_BT, JOYCON_R_BT, PRO_CONTROLLER, JOYCON_CHARGING_GRIP };


float rand0t1() {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(0.0f, 1.0f);
	float rnd = dis(gen);
	return rnd;
}



std::vector<Joycon> joycons;

JOYSTICK_POSITION_V2 iReport; // The structure that holds the full position data
uint8_t global_count = 0;


struct Settings {

	// there appears to be a good amount of variance between JoyCons,
	// but they work well once you find the right offsets
	// these are the values that worked well for my JoyCons:
	// alternatively just use --auto-center, it overrides these settings
	int leftJoyConXOffset = 16000;
	int leftJoyConYOffset = 13000;

	int rightJoyConXOffset = 15000;
	int rightJoyConYOffset = 19000;

	// multipliers to go from the range (-128,128) to (-32768, 32768)
	// These shouldn't need to be changed too much, but the option is there
	// I found that 250 works for me
	int leftJoyConXMultiplier = 250;
	int leftJoyConYMultiplier = 250;
	int rightJoyConXMultiplier = 250;
	int rightJoyConYMultiplier = 250;

	// Enabling this combines both JoyCons to a single vJoy Device(#1)
	// when combineJoyCons == false:
	// JoyCon(L) is mapped to vJoy Device #1
	// JoyCon(R) is mapped to vJoy Device #2
	// when combineJoyCons == true:
	// JoyCon(L) and JoyCon(R) are mapped to vJoy Device #1
	bool combineJoyCons = false;

	bool reverseX = false;// reverses joystick x (both sticks)
	bool reverseY = false;// reverses joystick y (both sticks)

	// Automatically center sticks
	// works by getting joystick position at start
	// and assumes that to be (0,0), and uses that to calculate the offsets
	bool autoCenterSticks = false;

	bool usingGrip = false;
	bool usingBluetooth = true;
	bool disconnect = false;

	// enables motion controls
	bool enableGyro = false;

	// plays a version of the mario theme by vibrating
	// the first JoyCon connected.
	bool marioTheme = false;

	// bool to restart the program
	bool restart = false;

} settings;





void found_joycon(struct hid_device_info *dev) {
	
	
	Joycon jc;

	if (dev->product_id == JOYCON_CHARGING_GRIP) {
		
		//if (dev->interface_number == 0) {
		if (dev->interface_number == 0 || dev->interface_number == -1) {
			jc.name = std::string("Joy-Con (R)");
			jc.left_right = 2;// right joycon
		} else if (dev->interface_number == 1) {
			jc.name = std::string("Joy-Con (L)");
			jc.left_right = 1;// left joycon
		}
	}

	if (dev->product_id == JOYCON_L_BT) {
		jc.name = std::string("Joy-Con (L)");
		jc.left_right = 1;// left joycon
	} else if (dev->product_id == JOYCON_R_BT) {
		jc.name = std::string("Joy-Con (R)");
		jc.left_right = 2;// right joycon
	}

	jc.serial = wcsdup(dev->serial_number);
	jc.buttons = 0;

	printf("Found joycon %c %i: %ls %s\n", L_OR_R(jc.left_right), joycons.size(), jc.serial, dev->path);
	//errno = 0;
	jc.handle = hid_open_path(dev->path);
	// set non-blocking:
	//hid_set_nonblocking(jc.handle, 1);


	if (jc.handle == nullptr) {
		printf("Could not open serial %ls: %s\n", jc.serial, strerror(errno));
		throw;
	}

	joycons.push_back(jc);
}

unsigned createMask(unsigned a, unsigned b) {
	unsigned r = 0;
	for (unsigned i = a; i <= b; i++)
		r |= 1 << i;

	return r;
}

inline int mk_even(int n) {
	return n - n % 2;
}

inline int mk_odd(int n) {
	return n - (n % 2 ? 0 : 1);
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

void print_buttons(Joycon *jc) {

	for (int i = 0; i < 16; i++) {
		if (jc->buttons & (1 << button_map[i].bit)) {
			printf("1");
		} else {
			printf("0");
		}
	}
	printf("\n");
}


void print_buttons2(Joycon *jc) {

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

void print_stick2(Joycon *jc) {

	printf("Joycon %c (Unattached): ", L_OR_R(jc->left_right));

	printf("%d %d\n", jc->stick.horizontal, jc->stick.vertical);

	
}




const char *const dstick_names[9] = {"Up", "UR", "Ri", "DR", "Do", "DL", "Le", "UL", "Neu"};



void hex_dump(unsigned char *buf, int len) {
	for (int i = 0; i < len; i++) {
		printf("%02x ", buf[i]);
	}
	printf("\n");
}

void hex_dump2(unsigned char *buf, int len) {
	for (int i = 0; i < len; i++) {
		printf("%02x ", buf[i]);
	}
}

void hex_dump_0(unsigned char *buf, int len) {
	for (int i = 0; i < len; i++) {
		if (buf[i] != 0) {
			printf("%02x ", buf[i]);
		}
	}
}

void device_print(struct hid_device_info *dev) {
	printf("USB device info:\n  vid: 0x%04hX pid: 0x%04hX\n  path: %s\n  serial_number: %ls\n  interface_number: %d\n",
		dev->vendor_id, dev->product_id, dev->path, dev->serial_number, dev->interface_number);
	printf("  Manufacturer: %ls\n", dev->manufacturer_string);
	printf("  Product:      %ls\n\n", dev->product_string);
}



void print_dstick(Joycon *jc) {
	printf("%s\n", dstick_names[jc->dstick]);
}

void hid_exchange(hid_device *handle, unsigned char *buf, int len) {
	if (!handle) return; //TODO: idk I just don't like this to be honest
	
	int res;

	res = hid_write(handle, buf, len);

	if (res < 0) {
		//printf("Number of bytes written was < 0!\n");
	} else {
		//printf("%d bytes written.\n", res);
	}

	//// set non-blocking:
	//hid_set_nonblocking(handle, 1);

	//res = hid_read(handle, buf, 0x41);
	res = hid_read(handle, buf, 0x40);

	if (res < 1) {
		//printf("Number of bytes read was < 1!\n");
	} else {
		//printf("%d bytes read.\n", res);
	}

#ifdef DEBUG_PRINT
	hex_dump(buf, 0x40);
#endif
}






void hid_dual_write(hid_device *handle_l, hid_device *handle_r, unsigned char *buf_l, unsigned char *buf_r, int len) {
	int res;

	if (handle_l && buf_l) {
		hid_set_nonblocking(handle_l, 1);
		res = hid_write(handle_l, buf_l, len);
		if (res < 0) {
			settings.disconnect = true;
			return;
		}

		hid_set_nonblocking(handle_l, 0);
	}

	if (handle_r && buf_r) {
		hid_set_nonblocking(handle_r, 1);
		res = hid_write(handle_r, buf_r, len);
		if (res < 0) {
			settings.disconnect = true;

			//throw;
			return;
		}

		hid_set_nonblocking(handle_r, 0);
	}
}



void handle_input(Joycon *jc, uint8_t *packet, int len) {



	
	// Upright: LDUR
	// Sideways: DRLU
	// bluetooth button pressed packet:
	if (packet[0] == 0x3F/*63*/) {

		uint16_t old_buttons = jc->buttons;
		int8_t old_dstick = jc->dstick;

		jc->dstick = packet[3];
	}

	// input update packet:
	if (packet[0] == 0x21 || packet[0] == 0x31) {
		
		// offset for usb or bluetooth data:
		int offset = settings.usingBluetooth ? 0 : 10;


		uint8_t *btn_data = packet + offset + 3;


		// get button states:
		{
			uint16_t states = 0;
			// Left JoyCon:
			if (jc->left_right == 1) {
				states = (btn_data[1] << 8) | (btn_data[2] & 0xFF);
			// Right JoyCon:
			} else if (jc->left_right == 2) {
				states = (btn_data[1] << 8) | (btn_data[0] & 0xFF);
			}

			jc->buttons = states;
		}


		//int mask = (1111 << 12);
		//int mask = 0b1111000000000000;
		if (jc->left_right == 1) {
			if (jc->buttons == 207) {
				settings.restart = true;
				//goto init_start;
			}
		}

		// get stick data:

		//uint8_t *stick_data = nullptr;
		//if (jc->left_right == 1) {
		//	stick_data = packet + 6 + offset;
		//} else if(jc->left_right == 2) {
		//	stick_data = packet + 9 + offset;
		//}
		uint8_t *stick_data = packet + offset;
		if (jc->left_right == 1) {
			stick_data = stick_data + 6;
		} else if (jc->left_right == 2) {
			stick_data = stick_data + 9;
		}

		

		// it appears that the X component of the stick data isn't just nibble reversed,
		// specifically, the second nibble of second byte is combined with the first nibble of the first byte
		// to get the correct X stick value:
		uint8_t stick_horizontal = ((stick_data[1] & 0x0F) << 4) | ((stick_data[0] & 0xF0) >> 4);// horizontal axis is reversed / combined with byte 0
		
		uint8_t stick_vertical = stick_data[2];

		jc->stick.horizontal2 = stick_horizontal;
		jc->stick.vertical2 = stick_vertical;

		jc->stick.horizontal = -128 + (int)(unsigned int)stick_horizontal;
		jc->stick.vertical = -128 + (int)(unsigned int)stick_vertical;

		jc->battery = (stick_data[1] & 0xF0) >> 4;

		//printf("Joycon battery: %d\n", jc->battery);


		uint8_t *gyro_data = nullptr;
		if (jc->left_right == 1) {
			gyro_data = packet + 13;// 13
		} else if (jc->left_right == 2) {
			gyro_data = packet + 13;// 13
		}

		//gyro_data[7] + gyro_data[8]	= relative roll
		//gyro_data[9] + gyro_data[10]	= relative pitch
		//gyro_data[11] + gyro_data[12] = relative yaw

		//gyro_data[7] + gyro_data[8]	= relative roll
		//gyro_data[9] + gyro_data[10]	= relative pitch
		//gyro_data[11] + gyro_data[12] = relative yaw
		//

		// second nibble from first byte + first nibble from 0th byte
		//jc->gyro.pitch = (int)(unsigned int) ((gyro_data[1] & 0x0F) << 4) | ((gyro_data[4] & 0xF0) >> 4);
		//jc->gyro.pitch = (int)(unsigned int)((gyro_data[1] & 0x0F) << 4) | ((gyro_data[0] & 0xF0) >> 4);

		// second nibble from second byte + first nibble from first byte
		//jc->gyro.roll = (int)(unsigned int)((gyro_data[3] & 0x0F) << 4) | ((gyro_data[2] & 0xF0) >> 4);

		




		// get relative roll:
		uint16_t relrollA = ((uint16_t)gyro_data[7] << 8) | gyro_data[8];
		uint16_t relrollB = 0xFFFF - relrollA;
		if (relrollA < relrollB) {
			jc->gyro.relroll = relrollA;
		} else {
			jc->gyro.relroll = -1 * relrollB;
		}
		jc->gyro.relroll /= 257;

		// get relative pitch:
		uint16_t relpitchA = ((uint16_t)gyro_data[9] << 8) | gyro_data[10];
		uint16_t relpitchB = 0xFFFF - relpitchA;
		if (relpitchA < relpitchB) {
			jc->gyro.relpitch = relpitchA;
		} else {
			jc->gyro.relpitch = -1 * relpitchB;
		}
		jc->gyro.relpitch /= 257;


		// get relative yaw:
		uint16_t relyawA = ((uint16_t)gyro_data[11] << 8) | gyro_data[12];
		uint16_t relyawB = 0xFFFF - relyawA;
		if (relyawA < relyawB) {
			jc->gyro.relyaw = relyawA;
		} else {
			jc->gyro.relyaw = -1 * relyawB;
		}
		jc->gyro.relyaw /= 257;




		// get accelerometer X:
		uint16_t accelXA = ((uint16_t)gyro_data[1] << 8) | gyro_data[2];
		uint16_t accelXB = 0xFFFF - accelXA;
		if (accelXA < accelXB) {
			jc->accel.x = accelXA;
		} else {
			jc->accel.x = -1 * accelXB;
		}
		//jc->accel.x /= 257;
		

		// get accelerometer Y:
		uint16_t accelYA = ((uint16_t)gyro_data[3] << 8) | gyro_data[4];
		uint16_t accelYB = 0xFFFF - accelYA;
		if (accelYA < accelYB) {
			jc->accel.y = accelYA;
		} else {
			jc->accel.y = -1 * accelYB;
		}
		//jc->accel.y /= 257;


		// get accelerometer Z:
		uint16_t accelZA = ((uint16_t)gyro_data[5] << 8) | gyro_data[6];
		uint16_t accelZB = 0xFFFF - accelZA;
		if (accelZA < accelZB) {
			jc->accel.z = accelZA;
		} else {
			jc->accel.z = -1 * accelZB;
		}
		//jc->accel.z /= 257;



		if (jc->left_right == 2) {
			//hex_dump(gyro_data, 20);
			//printf("%d\n", jc->gyro.relroll);
			//printf("%02x\n", jc->gyro.relroll);
			//printf("%04x\n", jc->gyro.relroll);

			//printf("%04x\n", absRollA);
			//printf("%02x %02x\n", rollA, rollB);
		}
		
	}



}







void joycon_send_command(Joycon *jc, int command, uint8_t *data, int len) {
	unsigned char buf[0x400];
	memset(buf, 0, 0x400);

	if (!settings.usingBluetooth) {
		buf[0x00] = 0x80;
		buf[0x01] = 0x92;
		buf[0x03] = 0x31;
	}

	buf[settings.usingBluetooth ? 0x0 : 0x8] = command;
	if (data != nullptr && len != 0) {
		memcpy(buf + (settings.usingBluetooth ? 0x1 : 0x9), data, len);
	}

	hid_exchange(jc->handle, buf, len + (settings.usingBluetooth ? 0x1 : 0x9));

	if (data) {
		memcpy(data, buf, 0x40);
	}
}

void joycon_send_subcommand(Joycon *jc, int command, int subcommand, uint8_t *data, int len) {
	unsigned char buf[0x400];
	memset(buf, 0, 0x400);

	uint8_t rumble_base[9] = { (++global_count) & 0xF, 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40 };
	memcpy(buf, rumble_base, 9);

	// set neutral rumble base only if the command is vibrate (0x01)
	// if set when other commands are set, might cause the command to be misread and not executed
	//if (subcommand == 0x01) {
	//	uint8_t rumble_base[9] = { (++global_count) & 0xF, 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40 };
	//	memcpy(buf + 10, rumble_base, 9);
	//}

	buf[9] = subcommand;
	if (data && len != 0) {
		memcpy(buf + 10, data, len);
	}

	joycon_send_command(jc, command, buf, 10 + len);

	if (data) {
		memcpy(data, buf, 0x40); //TODO
	}
}


void joycon_rumble(Joycon *jc, int frequency, int intensity) {
	unsigned char buf[0x400];
	memset(buf, 0, 0x40);

	// intensity: (0, 8)
	// frequency: (0, 255)

	//	 X	AA	BB	 Y	CC	DD
	//[0 1 x40 x40 0 1 x40 x40] is neutral.


	//for (int j = 0; j <= 8; j++) {
	//	buf[1 + intensity] = 0x1;//(i + j) & 0xFF;
	//}

	buf[1 + 0 + intensity] = 0x1;
	buf[1 + 4 + intensity] = 0x1;

	// Set frequency to increase
	if (jc->left_right == 1) {
		buf[1 + 0] = frequency;// (0, 255)
	} else {
		buf[1 + 4] = frequency;// (0, 255)
	}

	// set non-blocking:
	hid_set_nonblocking(jc->handle, 1);

	joycon_send_command(jc, 0x10, (uint8_t*)buf, 0x9);
}


void joycon_rumble2(Joycon *jc, int HF, int HFA, int LF, int LFA) {
	unsigned char buf[0x400];
	memset(buf, 0, 0x40);


	int hf		= HF;
	int hf_amp	= HFA;
	int lf		= LF;
	int lf_amp	= LFA;
	// maybe:
	//int hf_band = hf + hf_amp;

	int off = 0;// offset
	if (jc->left_right == 2) {
		off = 4;
	}

	// Left/Right linear actuator
	//hf = 0x01a8; //Set H.Frequency
	//hf_amp = 0x88; //Set H.Frequency amplitude

	//lf = 0x63; //Set L.Frequency
	//lf_amp = 0x804d; //Set L.Frequency amplitude


	
	//Byte swapping
	//buf[0] = hf_band & 0xFF;
	//buf[1] = (hf_band >> 8) & 0xFF;

	//buf[2] = 0x1;
	
	// Byte swapping
	buf[0 + off] = hf & 0xFF;
	buf[1 + off] = hf_amp + ((hf >> 8) & 0xFF); //Add amp + 1st byte of frequency to amplitude byte
	
	// Byte swapping
	buf[2 + off] = lf + ((lf_amp >> 8) & 0xFF); //Add freq + 1st byte of LF amplitude to the frequency byte
	buf[3 + off] = lf_amp & 0xFF;


	// set non-blocking:
	hid_set_nonblocking(jc->handle, 1);

	joycon_send_command(jc, 0x10, (uint8_t*)buf, 0x9);
}


void joycon_rumble3(Joycon *jc, int frequency, int HFA, int LFA) {

	//Float frequency to hex conversion
	uint16_t encoded_hex_freq = (uint16_t)floor(-32 * (0.693147f - log(frequency / 5)) / 0.693147f + 0.5f);

	//Convert to Joy-Con HF range. Range in big-endian: 0x0004-0x01FC with +0x0004 steps.
	uint16_t hf = (encoded_hex_freq - 0x60) * 4;
	//Convert to Joy-Con LF range. Range: 0x0100-0x7F00.
	uint16_t lf = encoded_hex_freq - 0x40;

	joycon_rumble2(jc, hf, HFA, lf, LFA);
}

void joycon_rumble4(Joycon *jc, int frequency, int HFA, int LFA) {

	//Float frequency to hex conversion
	uint16_t encoded_hex_freq = (uint16_t)floor(-32 * (0.693147f - log(frequency / 5)) / 0.693147f + 0.5f);

	//Convert to Joy-Con HF range. Range in big-endian: 0x0004-0x01FC with +0x0004 steps.
	uint16_t hf = (encoded_hex_freq - 0x60) * 4;
	//Convert to Joy-Con LF range. Range: 0x0100-0x7F00.
	uint16_t lf = encoded_hex_freq - 0x40;

	joycon_rumble2(jc, hf, HFA, lf, LFA);
}




void joycon_init_usb(Joycon *jc) {
	unsigned char buf[0x400];
	memset(buf, 0, 0x400);

	// set blocking:
	// this insures we get the MAC Address
	hid_set_nonblocking(jc->handle, 0);

	//Get MAC Left
	printf("Getting MAC...\n");
	memset(buf, 0x00, 0x40);
	buf[0] = 0x80;
	buf[1] = 0x01;
	hid_exchange(jc->handle, buf, 0x2);

	if (buf[2] == 0x3) {
		printf("%s disconnected!\n", jc->name.c_str());
	} else {
		printf("Found %s, MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", jc->name.c_str(), buf[9], buf[8], buf[7], buf[6], buf[5], buf[4]);
	}

	// set non-blocking:
	//hid_set_nonblocking(jc->handle, 1);

	// Do handshaking
	printf("Doing handshake...\n");
	memset(buf, 0x00, 0x40);
	buf[0] = 0x80;
	buf[1] = 0x02;
	hid_exchange(jc->handle, buf, 0x2);

	// Switch baudrate to 3Mbit
	printf("Switching baudrate...\n");
	memset(buf, 0x00, 0x40);
	buf[0] = 0x80;
	buf[1] = 0x03;
	hid_exchange(jc->handle, buf, 0x2);

	//Do handshaking again at new baudrate so the firmware pulls pin 3 low?
	printf("Doing handshake...\n");
	memset(buf, 0x00, 0x40);
	buf[0] = 0x80;
	buf[1] = 0x02;
	hid_exchange(jc->handle, buf, 0x2);

	//Only talk HID from now on
	printf("Only talk HID...\n");
	memset(buf, 0x00, 0x40);
	buf[0] = 0x80;
	buf[1] = 0x04;
	hid_exchange(jc->handle, buf, 0x2);

	// Enable vibration
	printf("Enabling vibration...\n");
	memset(buf, 0x00, 0x400);
	buf[0] = 0x01; // Enabled
	joycon_send_subcommand(jc, 0x1, 0x48, buf, 1);

	// Enable IMU data
	printf("Enabling IMU data...\n");
	memset(buf, 0x00, 0x400);
	buf[0] = 0x01; // Enabled
	joycon_send_subcommand(jc, 0x1, 0x40, buf, 1);

	printf("Successfully initialized %s!\n", jc->name.c_str());
}



int joycon_init_bt(Joycon *jc) {

	unsigned char buf[0x400];
	memset(buf, 0, 0x400);

	// set non-blocking:
	hid_set_nonblocking(jc->handle, 1);

	// Enable vibration
	printf("Enabling vibration...\n");
	memset(buf, 0x00, 0x400);
	buf[0] = 0x01; // Enabled
	joycon_send_subcommand(jc, 0x1, 0x48, buf, 1);

	// Enable IMU data
	printf("Enabling IMU data...\n");
	memset(buf, 0x00, 0x400);
	buf[0] = 0x01; // Enabled
	joycon_send_subcommand(jc, 0x01, 0x40, buf, 1);


	// Set input report mode (to push at 60hz)
	// x00	Active polling mode for IR camera data. Answers with more than 300 bytes ID 31 packet
	// x01	Active polling mode
	// x02	Active polling mode for IR camera data.Special IR mode or before configuring it ?
	// x21	Unknown.An input report with this ID has pairing or mcu data or serial flash data or device info
	// x23	MCU update input report ?
	// 30	NPad standard mode. Pushes current state @60Hz. Default in SDK if arg is not in the list
	// 31	NFC mode. Pushes large packets @60Hz
	printf("Increase data rate for Bluetooth...\n");
	memset(buf, 0x00, 0x400);
	buf[0] = 0x31;
	joycon_send_subcommand(jc, 0x01, 0x03, buf, 1);


	printf("Pairing1?...\n");
	memset(buf, 0x00, 0x400);
	buf[0] = 0x01;
	joycon_send_subcommand(jc, 0x01, 0x01, buf, 1);

	printf("Pairing2?...\n");
	memset(buf, 0x00, 0x400);
	buf[0] = 0x02;
	joycon_send_subcommand(jc, 0x01, 0x01, buf, 1);

	printf("Pairing3?...\n");
	memset(buf, 0x00, 0x400);
	buf[0] = 0x03;
	joycon_send_subcommand(jc, 0x01, 0x01, buf, 1);



	printf("Successfully initialized %s!\n", jc->name.c_str());

	return 0;
}

void joycon_deinit_usb(Joycon *jc) {
	unsigned char buf[0x40];
	memset(buf, 0x00, 0x40);

	//Let the Joy-Con talk BT again    
	buf[0] = 0x80;
	buf[1] = 0x05;
	hid_exchange(jc->handle, buf, 0x2);
	printf("Deinitialized %s\n", jc->name.c_str());
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
		//wprintf(L"Vendor: %s\nProduct :%s\nVersion Number:%s\n", static_cast<TCHAR *> (GetvJoyManufacturerString()), static_cast<TCHAR *>(GetvJoyProductString()), static_cast<TCHAR *>(GetvJoySerialNumberString()));
		//wprintf(L"Product :%s\n", static_cast<TCHAR *>(GetvJoyProductString()));
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

void updatevJoyDevice(Joycon *jc) {

	// todo: calibration of some kind
	int leftJoyConXOffset = settings.leftJoyConXOffset;
	int leftJoyConYOffset = settings.leftJoyConYOffset;

	int rightJoyConXOffset = settings.rightJoyConXOffset;
	int rightJoyConYOffset = settings.rightJoyConYOffset;

	// multipliers, these shouldn't really be different from one another
	// but are there anyways
	// todo: add a logarithmic multiplier? it's already doable in x360ce though
	int leftJoyConXMultiplier = settings.leftJoyConXMultiplier;
	int leftJoyConYMultiplier = settings.leftJoyConYMultiplier;
	int rightJoyConXMultiplier = settings.rightJoyConXMultiplier;
	int rightJoyConYMultiplier = settings.rightJoyConYMultiplier;

	bool combineJoyCons = settings.combineJoyCons;

	bool reverseX = settings.reverseX;
	bool reverseY = settings.reverseY;





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


	// Set Stick data
	
	int x, y, z;
	int rx = 16384;
	int ry = 16384;
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

	if (reverseX) {
		x = 32768 - x;
	}
	if (reverseY) {
		y = 32768 - y;
	}

	// both left and right joycons
	if (!combineJoyCons) {
		iReport.wAxisX = x;
		iReport.wAxisY = y;
		iReport.wAxisXRot = rx;
		iReport.wAxisYRot = ry;
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



	// gyro data:
	if ((joycons.size() > 1 && jc->left_right == 2) || (joycons.size() == 1 && jc->left_right == 1)) {
		//rz = jc->gyro.roll*240;
		//iReport.wAxisZRot = jc->gyro.roll * 120;
		//iReport.wSlider = jc->gyro.pitch * 120;

		int multiplier;


		// Gyroscope (roll, pitch, yaw):
		multiplier = 1000;
		
		//iReport.wAxisZRot = 16384 + (jc->gyro.relroll * multiplier);
		//iReport.wSlider = 16384 + (jc->gyro.relpitch * multiplier);
		//iReport.wDial = 16384 + (jc->gyro.relyaw * multiplier);

		//iReport.wAxisZ = 16384 + (jc->gyro.relyaw * multiplier);






		// Accelerometer (x, y, z):

		multiplier = 10;
		iReport.wAxisZRot = 16384 + (jc->accel.x * multiplier);
		iReport.wSlider = 16384 + (jc->accel.y * multiplier);
		iReport.wDial = 16384 + (jc->accel.z * multiplier);



		//multiplier = 200;

		//iReport.wAxisZRot = (jc->gyro.roll * multiplier);
		//iReport.wSlider = (jc->gyro.pitch * multiplier);
		//iReport.wDial = (jc->gyro.yaw * multiplier);

	}
	



	// Set button data

	long btns = 0;
	if (!combineJoyCons) {
		btns = jc->buttons;
	} else {

		if (jc->left_right == 2) {

			unsigned r = createMask(0, 15);// 15
			btns = ((jc->buttons) << 16) | (r & iReport.lButtons);

		} else if(jc->left_right == 1) {
			btns = ((iReport.lButtons >> 16) << 16) | (jc->buttons);
		}
	}

	iReport.lButtons = btns;

	// Send data to vJoy device
	pPositionMessage = (PVOID)(&iReport);
	if (!UpdateVJD(DevID, pPositionMessage)) {
		printf("Feeding vJoy device number %d failed - try to enable device then press enter\n", DevID);
		getchar();
		AcquireVJD(DevID);
	}
}


void parseSettings(int length, char *args[]) {
	for (int i = 0; i < length; ++i) {
		if (std::string(args[i]) == "--combine") {
			settings.combineJoyCons = true;
			printf("JoyCon combining enabled.\n");
		}
		if (std::string(args[i]) == "--LXO") {
			settings.leftJoyConXOffset = std::stoi(args[i + 1]);
		}
		if (std::string(args[i]) == "--LYO") {
			settings.leftJoyConYOffset = std::stoi(args[i + 1]);
		}
		if (std::string(args[i]) == "--RXO") {
			settings.rightJoyConXOffset = std::stoi(args[i + 1]);
		}
		if (std::string(args[i]) == "--RYO") {
			settings.rightJoyConYOffset = std::stoi(args[i + 1]);
		}

		if (std::string(args[i]) == "--REVX") {
			settings.reverseX = true;
		}
		if (std::string(args[i]) == "--REVY") {
			settings.reverseY = true;
		}
		if (std::string(args[i]) == "--auto-center") {
			settings.autoCenterSticks = true;
		}
		if (std::string(args[i]) == "--mario-theme") {
			settings.marioTheme = true;
		}
		if (std::string(args[i]) == "--enable-gyro") {
			settings.enableGyro = true;
		}
	}
}









int main(int argc, char *argv[]) {

	// get vJoy Device 1
	acquirevJoyDevice(1);
	// get vJoy Device 2
	acquirevJoyDevice(2);

	int missedPollCount = 0;
	int res;
	int i;
	unsigned char buf[65];
	unsigned char buf2[65];

	int read;	// number of bytes read
	int written;// number of bytes written


	const char *device_name;
	

	// Enumerate and print the HID devices on the system
	struct hid_device_info *devs, *cur_dev;

	res = hid_init();

	





init_start:

	devs = hid_enumerate(JOYCON_VENDOR, 0x0);
	cur_dev = devs;
	while (cur_dev) {
		// identify by vendor:
		if (cur_dev->vendor_id == JOYCON_VENDOR) {

			//device_print(cur_dev);
			Joycon jc;

			// bluetooth, left / right joycon:
			if (cur_dev->product_id == JOYCON_L_BT || cur_dev->product_id == JOYCON_R_BT) {
				found_joycon(cur_dev);
				settings.usingBluetooth = true;
			}

			// pro controller:
			// (probably won't work right, I'm just putting this here so it detects it,
			// I don't have a pro controller to test with.)
			if (cur_dev->product_id == PRO_CONTROLLER) {
				found_joycon(cur_dev);
			}

			// charging grip:
			if (cur_dev->product_id == JOYCON_CHARGING_GRIP) {
				settings.usingGrip = true;
				settings.usingBluetooth = false;
				settings.combineJoyCons = true;
				found_joycon(cur_dev);
			}
		}


		cur_dev = cur_dev->next;
	}
	hid_free_enumeration(devs);



	// init joycons:
	if (settings.usingGrip) {
		for (int i = 0; i < joycons.size(); ++i) {
			joycon_init_usb(&joycons[i]);
		}
	} else {
		for (int i = 0; i < joycons.size(); ++i) {
			joycon_init_bt(&joycons[i]);
		}
	}


	parseSettings(argc, argv);

	

	// use stick data to calibrate:
	if (settings.autoCenterSticks) {
		printf("Auto centering sticks...\n");
		// do a few polls to get stick data:
		for (int j = 0; j < 10; ++j) {
			for (int i = 0; i < joycons.size(); ++i) {
				Joycon *jc = &joycons[i];
				if (!jc->handle) {
					continue;
				}
				// set to be blocking:
				hid_set_nonblocking(jc->handle, 0);

				joycon_send_command(jc, 0x01, buf, 0);

				handle_input(jc, buf, res);

				updatevJoyDevice(jc);
			}
		}




		for (int i = 0; i < joycons.size(); ++i) {
			Joycon *jc = &joycons[i];

			// left joycon:
			if (jc->left_right == 1) {
				int x = (settings.leftJoyConXMultiplier * (jc->stick.horizontal));
				settings.leftJoyConXOffset = -x + (16384);

				int y = (settings.leftJoyConYMultiplier * (jc->stick.vertical));
				settings.leftJoyConYOffset = -y + (16384);

			// right joycon:
			} else if (jc->left_right == 2) {

				int x = (settings.rightJoyConXMultiplier * (jc->stick.horizontal));
				settings.rightJoyConXOffset = -x + (16384);

				int y = (settings.rightJoyConYMultiplier * (jc->stick.vertical));
				settings.rightJoyConYOffset = -y + (16384);
			}


		}


		printf("Done centering sticks.\n");
	}











	// set lights:
	printf("setting LEDs...\n");
	for (int r = 0; r < 5; ++r) {
		for (int i = 0; i < joycons.size(); ++i) {

			Joycon *jc = &joycons[i];

			// Player LED Enable
			memset(buf, 0x00, 0x40);
			if (i == 0) {
				buf[0] = 0x0 | 0x0 | 0x0 | 0x1;		// solid 1
			}
			if (i == 1) {
				if (settings.combineJoyCons) {
					buf[0] = 0x0 | 0x0 | 0x0 | 0x1; // solid 1
				} else if (!settings.combineJoyCons) {
					buf[0] = 0x0 | 0x0 | 0x2 | 0x0; // solid 2
				}
			}
			//buf[0] = 0x80 | 0x40 | 0x2 | 0x1; // Flash top two, solid bottom two
			//buf[0] = 0x8 | 0x4 | 0x2 | 0x1; // All solid
			//buf[0] = 0x80 | 0x40 | 0x20 | 0x10; // All flashing
			//buf[0] = 0x80 | 0x00 | 0x20 | 0x10; // All flashing except 3rd light (off)

			joycon_send_subcommand(jc, 0x1, 0x30, buf, 1);
		}
	}
	printf("LEDs should be set.\n");


	printf("vibrating JoyCon(s).\n");
	// give a small rumble to all joycons:
	for (int k = 0; k < 1; ++k) {
		for (int i = 0; i < joycons.size(); ++i) {
			joycon_rumble(&joycons[i], 100, 1);
			Sleep(20);
			joycon_rumble(&joycons[i], 10, 3);
			//Sleep(100);
		}
	}

	//for (int k = 0; k < 10000; ++k) {

	//	int lower = 40;
	//	int higher = 1000;
	//	float frequency = (sin(0.01*k)*(higher - lower)) + lower;
	//	joycon_rumble3(&joycons[0], frequency, 0x8a, 0x8062);
	//	printf("frequency: %f\n", frequency);

	//	//joycon_rumble2(&joycons[0], 0x0001, 0x88, 0x60, 0x804d);
	//	//joycon_rumble3(&joycons[0], 320, 0x88, 0x804d);

	//	//joycon_rumble3(&joycons[0], 40, 0x88, 0x804d);

	//	//joycon_rumble(&joycons[0], mk_odd((int)(sin(0.01*k)*(200 - 1)) + 1), 1);
	//	
	//	Sleep(100);
	//}

	// Plays the Mario theme on the JoyCons:
	// I'm bad with music I just did this by
	// using a video of a piano version of the mario theme.
	// maybe eventually I'll be able to play something like sound files.

	// notes arbitrarily defined:
	#define C3 110
	#define D3 120
	#define E3 130
	#define F3 140
	#define G3 150
	#define G3A4 155
	#define A4 160
	#define A4B4 165
	#define B4 170
	#define C4 180
	#define D4 190
	#define D4E4 195
	#define E4 200
	#define F4 210
	#define F4G4 215
	#define G4 220
	#define A5 230
	#define B5 240
	#define C5 250



	if (settings.marioTheme) {
		for (int i = 0; i < 1; ++i) {

			printf("Playing mario theme...\n");

			float spd = 1;
			float spd2 = 1;

			//goto N1;

			Sleep(1000);

			joycon_rumble(&joycons[0], mk_odd(E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2
			Sleep(100 / spd2);
			joycon_rumble(&joycons[0], mk_odd(E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2
			Sleep(100 / spd2);
			joycon_rumble(&joycons[0], mk_odd(E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2
			Sleep(100 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2
			Sleep(100 / spd2);
			joycon_rumble(&joycons[0], mk_odd(G4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G2
			Sleep(400 / spd2);

			joycon_rumble(&joycons[0], mk_odd(A4), 1); Sleep(400 / spd); joycon_rumble(&joycons[0], 1, 3);	// too low for joycon
			Sleep(50 / spd2);

			joycon_rumble(&joycons[0], mk_odd(C4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C2
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(G3), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G1
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(E3), 2); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E1
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(A4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A2

			Sleep(100 / spd2);
			joycon_rumble(&joycons[0], mk_odd(B4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// B2

			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(A4B4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// A2-B2?
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(A4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A2
			Sleep(100 / spd2);
			joycon_rumble(&joycons[0], mk_odd(G3), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G1


			Sleep(100 / spd2);
			joycon_rumble(&joycons[0], mk_odd(E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2
			Sleep(100 / spd2);
			joycon_rumble(&joycons[0], mk_odd(G4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G2
			Sleep(100 / spd2);
			joycon_rumble(&joycons[0], mk_odd(A5), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A3



			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(F4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// F2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(G4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G2

			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2

			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(D4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// D2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(B4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// B2


			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C2
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(G3), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G1
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(E3), 2); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E1
			
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(A4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A2
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(B4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// B2
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(A4B4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// A2-B2?
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(A4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A2
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(G4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G2


			Sleep(100 / spd2);
			joycon_rumble(&joycons[0], mk_odd(E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2
			Sleep(100 / spd2);
			joycon_rumble(&joycons[0], mk_odd(G4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G2
			Sleep(100 / spd2);
			joycon_rumble(&joycons[0], mk_odd(A5), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A3
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(F4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// F2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(G4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G2
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2


			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(D4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// D2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(B4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// B2

			// new:

			Sleep(500 / spd2);

			joycon_rumble(&joycons[0], mk_odd(G4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(F4G4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// F2-G2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(F4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// F2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(D4E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// D2-E2
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2

			Sleep(200 / spd2);

			joycon_rumble(&joycons[0], mk_odd(G3A4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// G1-A2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(A4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C2

			
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(A4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(D4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// D2


			Sleep(300 / spd2);

			joycon_rumble(&joycons[0], mk_odd(G4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(F4G4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// F2-G2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(F4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// F2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(D4E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// D2-E2
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2


			// three notes:
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C5), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C3
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C3), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C3
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C3), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C3


			N1:


			Sleep(500 / spd2);
			joycon_rumble(&joycons[0], mk_odd(G4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(F4G4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// F2G2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(F4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// F2

			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(D4E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// D2E2

			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2



			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(G3A4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// G1A2

			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(A4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C2
			Sleep(100 / spd2);
			joycon_rumble(&joycons[0], mk_odd(A4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(D4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// D2


			Sleep(300 / spd2);
			joycon_rumble(&joycons[0], mk_odd(D4E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// D2E2
			Sleep(300 / spd2);
			joycon_rumble(&joycons[0], mk_odd(D4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// D2
			Sleep(300 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C2


			Sleep(800 / spd2);


			joycon_rumble(&joycons[0], mk_odd(G4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(F4G4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// F2G2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(F4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// F2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(D4E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// D2E2
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2

			Sleep(200 / spd2);
			

			joycon_rumble(&joycons[0], mk_odd(G3A4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// G1A2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(A4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C2

			Sleep(200 / spd2);

			joycon_rumble(&joycons[0], mk_odd(A4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(D4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// D2

			Sleep(300 / spd2);


			joycon_rumble(&joycons[0], mk_odd(G4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(F4G4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// F2G2
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(F4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// F2


			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(D4E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// D2E2
			Sleep(100 / spd2);
			joycon_rumble(&joycons[0], mk_odd(E4), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2

			// 30 second mark
			
			// three notes:

			Sleep(300 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C5), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C3
			Sleep(200 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C5), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C3
			Sleep(50 / spd2);
			joycon_rumble(&joycons[0], mk_odd(C5), 1); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C3


			Sleep(1000);
		}
	}







	//#define MusicOffset 300

	// notes in hertz:
	#define C3 131
	#define D3 146
	#define E3 165
	#define F3 175
	#define G3 196
	#define G3A4 208
	#define A4 440
	#define A4B4 466
	#define B4 494
	#define C4 262
	#define D4 294
	#define D4E4 311
	#define E4 329
	#define F4 349
	#define F4G4 215
	#define G4 392
	#define A5 880
	#define B5 988
	#define C5 523

	#define hfa 0xb0	// 8a
	#define lfa 0x006c	// 8062


	if (false) {
		for (int i = 0; i < 1; ++i) {

			printf("Playing mario theme...\n");

			float spd = 1;
			float spd2 = 1;

			Sleep(1000);

			joycon_rumble3(&joycons[0], E4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2
			Sleep(100 / spd2);
			joycon_rumble3(&joycons[0], E4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2
			Sleep(100 / spd2);
			joycon_rumble3(&joycons[0], E4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2
			Sleep(100 / spd2);
			joycon_rumble3(&joycons[0], C4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C2
			Sleep(50 / spd2);
			joycon_rumble3(&joycons[0], E4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2
			Sleep(100 / spd2);
			joycon_rumble3(&joycons[0], G4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G2
			Sleep(400 / spd2);

			joycon_rumble3(&joycons[0], A4, hfa, lfa); Sleep(400 / spd); joycon_rumble(&joycons[0], 1, 3);	// too low for joycon
			Sleep(50 / spd2);

			joycon_rumble3(&joycons[0], C4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C2
			Sleep(200 / spd2);
			joycon_rumble3(&joycons[0], G3, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G1
			Sleep(200 / spd2);
			joycon_rumble3(&joycons[0], E3, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E1
			Sleep(200 / spd2);
			joycon_rumble3(&joycons[0], A4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A2

			Sleep(100 / spd2);
			joycon_rumble3(&joycons[0], B4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// B2

			Sleep(50 / spd2);
			joycon_rumble3(&joycons[0], A4B4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// A2-B2?
			Sleep(50 / spd2);
			joycon_rumble3(&joycons[0], A4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A2
			Sleep(100 / spd2);
			joycon_rumble3(&joycons[0], G3, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G1


			Sleep(100 / spd2);
			joycon_rumble3(&joycons[0], E4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2
			Sleep(100 / spd2);
			joycon_rumble3(&joycons[0], G4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G2
			Sleep(100 / spd2);
			joycon_rumble3(&joycons[0], A5, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A3



			Sleep(200 / spd2);
			joycon_rumble3(&joycons[0], F4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// F2
			Sleep(50 / spd2);
			joycon_rumble3(&joycons[0], G4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G2

			Sleep(200 / spd2);
			joycon_rumble3(&joycons[0], E4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E2

			Sleep(50 / spd2);
			joycon_rumble3(&joycons[0], C4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C2
			Sleep(50 / spd2);
			joycon_rumble3(&joycons[0], D4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// D2
			Sleep(50 / spd2);
			joycon_rumble3(&joycons[0], B4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// B2


			Sleep(200 / spd2);
			joycon_rumble3(&joycons[0], C4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// C2
			Sleep(200 / spd2);
			joycon_rumble3(&joycons[0], G3, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G1
			Sleep(200 / spd2);
			joycon_rumble3(&joycons[0], E3, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// E1

			Sleep(200 / spd2);
			joycon_rumble3(&joycons[0], A4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A2
			Sleep(200 / spd2);
			joycon_rumble3(&joycons[0], B4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// B2
			Sleep(200 / spd2);
			joycon_rumble3(&joycons[0], A4B4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);// A2-B2?
			Sleep(50 / spd2);
			joycon_rumble3(&joycons[0], A4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// A2
			Sleep(200 / spd2);
			joycon_rumble3(&joycons[0], G4, hfa, lfa); Sleep(200 / spd); joycon_rumble(&joycons[0], 1, 3);	// G2


			Sleep(1000);
		}
	}



	printf("Done.\n");




	
	int counter = 0;

	while(true) {

		//auto start = std::chrono::steady_clock::now();
		//std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
		

		counter++;
		

		// input poll loop:

		for (int i = 0; i < joycons.size(); ++i) {

			Joycon *jc = &joycons[i];

			if (!jc->handle) {
				continue;
			}

			// set to be non-blocking:
			hid_set_nonblocking(jc->handle, 1);
			

			// get input:
			memset(buf, 0, 65);

			//if (counter % 2 == 0) {
			//	//joycon_send_command(jc, 0x01, buf, 0);
			//} else {
			//	joycon_send_command(jc, 0x1F, buf, 0);
			//}

			if (settings.enableGyro) {
				// seems to have slower response time:
				joycon_send_command(jc, 0x1F, buf, 0);
			} else {
				// may reset MCU data, not sure:
				joycon_send_command(jc, 0x01, buf, 0);
			}

			//joycon_send_command(jc, 0x1F, buf, 0);
			//joycon_send_command(jc, 0x01, buf, 0);

			handle_input(jc, buf, 0x40);


			//if (GetKeyState('A') & 0x8000/*check if high-order bit is set (1 << 15)*/){
				//joycon_send_subcommand(jc, 0x01, 0x20, 0x0, 0);// reset MCU
			//}


			updatevJoyDevice(jc);

		}


		// sleep:
		//Sleep(5);
		Sleep(1);


		//auto end = std::chrono::steady_clock::now();
		//std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
		//auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

		//printf("time: %d\n", duration);

		if (settings.restart) {
			settings.restart = false;
			goto init_start;
		}


	}

	RelinquishVJD(1);
	RelinquishVJD(2);

	if (settings.usingGrip) {
		for (int i = 0; i < joycons.size(); ++i) {
			joycon_deinit_usb(&joycons[i]);
		}
	}

	// Finalize the hidapi library
	res = hid_exit();

	return 0;
}
