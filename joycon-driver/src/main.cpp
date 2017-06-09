


#include <bitset>
#include <random>
#include <stdafx.h>
#include <string.h>
#include <chrono>

#include <hidapi.h>



#include "public.h"
#include "vjoyinterface.h"


#pragma warning(disable:4996)

#define JOYCON_VENDOR 0x057e

#define JOYCON_L_BT 0x2006
#define JOYCON_R_BT 0x2007

#define PRO_CONTROLLER 0x2009

#define JOYCON_CHARGING_GRIP 0x200e

#define SERIAL_LEN 18

//#define VIBRATION_TEST
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

struct Joycon {

	hid_device *handle;
	wchar_t *serial;

	std::string name;

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
};



std::vector<Joycon> joycons;

JOYSTICK_POSITION_V2 iReport; // The structure that holds the full position data
//bool disconnect = false;
//bool bluetooth = true;
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
	// I found that 240 works for me
	int leftJoyConXMultiplier = 240;
	int leftJoyConYMultiplier = 240;
	int rightJoyConXMultiplier = 240;
	int rightJoyConYMultiplier = 240;

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
	bool usingBluetooth = false;
	bool disconnect = false;

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

	// bluetooth polled update packet:
	if (packet[0] == 0x21/*33*/) {

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


		uint8_t *pckt = packet + 3;
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


		// get button states:
		{
			uint16_t states = 0;

			// re-order bits:

			// Left JoyCon:
			if (jc->left_right == 1) {
				states = (pckt[1] << 8) | (pckt[2] & 0xff);
			// Right JoyCon:
			} else if (jc->left_right == 2) {
				states = (pckt[1] << 8) | (pckt[0] & 0xff);
			}

			jc->buttons = states;
		}

		// stick data:

		uint8_t *stick_data = nullptr;
		if (jc->left_right == 1) {
			stick_data = packet + 6;
		} else if(jc->left_right == 2) {
			stick_data = packet + 9;
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


	//if (jc->left_right == 1) {
	//	printf("L: ");
	//} else if (jc->left_right == 2) {
	//	printf("R: ");
	//}

	if (packet[0] != 0x3F && packet[0] != 0x21) {
		//printf("Unknown packet: ");
		//hex_dump2(packet, len);

		//hex_dump(packet, len);
	}

	if (jc->left_right == 2) {
		//hex_dump(packet, len-20);
	}

	if (packet[5] == 0x31/*49*/) {
		if (jc->left_right == 2) {
			//hex_dump(packet, len - 20);
		}
	}

	//if (jc->stick.horizontal < -50) {
	//	printf("TEST%d\n", rand0t1()*100);
	//}

	//printf("\n");

	//hex_dump_0(packet, len);
	//printf("\n");

	// response packet to?:
	// buf[0] = 0x01
	// buf[10] = 0x03
	//if (packet[0] == 0x30/*48*/) {
		//hex_dump(packet, len);
	//}




	//uint8_t *pckt = packet + 10;

	//hex_dump(pckt, 12);


	// USB polled info:
	if (packet[5] == 0x31/*49*/) {

		uint8_t *pckt = packet + 13;

		//if (jc->left_right == 1) {
		//	hex_dump(pckt, 20);
		//}

		// get button states:
		{

			uint16_t states = 0;

			// re-order bits:

			// Left JoyCon:
			if (jc->left_right == 1) {
				states = (pckt[1] << 8) | (pckt[2] & 0xff);
			// Right JoyCon:
			} else if (jc->left_right == 2) {
				states = (pckt[1] << 8) | (pckt[0] & 0xff);
			}

			jc->buttons = states;
		}

		// stick data:

		uint8_t *stick_data = nullptr;
		if (jc->left_right == 1) {
			stick_data = packet + 16;
		} else if (jc->left_right == 2) {
			stick_data = packet + 19;
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


	//print_buttons(jc);
	//print_buttons2(jc);
	//print_stick2(jc);
}











void joycon_send_command(hid_device *handle, int command, uint8_t *data, int len) {
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

	hid_exchange(handle, buf, len + (settings.usingBluetooth ? 0x1 : 0x9));

	if (data) {
		memcpy(data, buf, 0x40);
	}
}

void joycon_send_subcommand(hid_device *handle, int command, int subcommand, uint8_t *data, int len) {
	unsigned char buf[0x400];
	memset(buf, 0, 0x400);

	uint8_t rumble_base[9] = { (++global_count) & 0xF, 0x00, 0x01, 0x40, 0x40, 0x00, 0x01, 0x40, 0x40 };
	memcpy(buf, rumble_base, 9);

	buf[9] = subcommand;
	if (data && len != 0) {
		memcpy(buf + 10, data, len);
	}

	joycon_send_command(handle, command, buf, 10 + len);

	if (data) {
		memcpy(data, buf, 0x40); //TODO
	}
}


void joycon_init_usb(Joycon *jc) {
	unsigned char buf[0x400];
	memset(buf, 0, 0x400);


	// USB:
	//	80 01 (Handled elsewhere ? Returns MAC packet)
	//	80 02 (Do two handshakes 19 01 03 07 00 91 10 00, 19 01 03 0B 00 91 12 04)
	//	80 03 (Do baud switch)
	//	80 04 (Set something to 1)
	//	80 05 (Set something to 0)
	//	80 06 (Something with sending post - handshake command 01 00 00 00 00 00 00 00 01 06 00 00, maybe baud related as well ? )
	//	80 91 ... (Send pre - handshake command ? Has some weird lookup table stuff)
	//	80 92 ... (Something with pre - handshake commands ? )
	//	01 ... (Send post - handshake command ? Sends 0x31 - large UART starting with 19 01 03 07 00 92 00 00 with checksum edited in and the rest of the HID request pinned on)
	//	10 ... (Same as above)

	// set blocking:
	// this insures we get the MAC Address
	hid_set_nonblocking(jc->handle, 0);

	// USB:

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
	joycon_send_subcommand(jc->handle, 0x1, 0x48, buf, 1);

	// Enable IMU data
	printf("Enabling IMU data...\n");
	memset(buf, 0x00, 0x400);
	buf[0] = 0x01; // Enabled
	joycon_send_subcommand(jc->handle, 0x1, 0x40, buf, 1);

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
	joycon_send_subcommand(jc->handle, 0x1, 0x48, buf, 1);

	// Enable IMU data
	printf("Enabling IMU data...\n");
	memset(buf, 0x00, 0x400);
	buf[0] = 0x01; // Enabled
	joycon_send_subcommand(jc->handle, 0x01, 0x40, buf, 1);


	// start polling at 60hz?
	printf("Set to poll at 60hz?\n");
	memset(buf, 0x00, 0x400);
	buf[0] = 0x01; // Enabled
	//									cmd subcmd
	joycon_send_subcommand(jc->handle, 0x01, 0x03, buf, 1);


	//for (int i = 0; i < 100; ++i) {
	//	// set lights:
	//	memset(buf, 0x00, 0x400);
	//	buf[0] = i;
	//	buf[1] = i;
	//	//									cmd subcmd
	//	joycon_send_subcommand(jc->handle, 0x10, 0x30, buf, 1);
	//}

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


	//if (reverseX) {
	//	leftJoyConXMultiplier *= -1;
	//	rightJoyConXMultiplier *= -1;
	//}
	//if (reverseY) {
	//	leftJoyConYMultiplier *= -1;
	//	rightJoyConYMultiplier *= -1;
	//}


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

	if (reverseX) {
		x *= -1;
	}
	if (reverseY) {
		y *= -1;
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

	long btns = 0;
	if (!combineJoyCons) {
		btns = jc->buttons;
	} else {

		if (jc->left_right == 2) {
			//btns = iReport.lButtons | jc->buttons << 16;
			//unsigned low8bits = iReport.lButtons & 0xFF;
			//btns = (jc->buttons << 16) | (iReport.lButtons & 0xFF);
			//btns = ((jc->buttons) << 16) | (iReport.lButtons & 0xFF);

			unsigned r = createMask(0, 15);// 15
			btns = ((jc->buttons) << 16) | (r & iReport.lButtons);
			//std::bitset<32> x(btns);
			//std::cout << x;
			//printf("\n");
		} else if(jc->left_right == 1) {
			//unsigned high8bits = iReport.lButtons;
			btns = ((iReport.lButtons >> 16) << 16) | (jc->buttons);

			//std::bitset<32> x(btns);
			//std::cout << x;
			//printf("\n");
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
	}
}








int main(int argc, char *argv[]) {

	// get vJoy Device 1
	acquirevJoyDevice(1);
	// get vJoy Device 2
	acquirevJoyDevice(2);


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
				settings.usingBluetooth = true;
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
		for (int j = 0; j < 5; ++j) {
			for (int i = 0; i < joycons.size(); ++i) {

				Joycon *jc = &joycons[i];
				if (!jc->handle) {
					continue;
				}

				if (settings.usingGrip) {
					memset(buf, 0, 65);
					buf[0] = 0x80; // 80     Do custom command
					buf[1] = 0x92; // 92     Post-handshake type command
					buf[2] = 0x00; // 0001   u16 second part size
					buf[3] = 0x01;
					buf[8] = 0x1F; // 1F     Get input command
				} else {
					buf[0] = 0x1;// HID get input
					buf[1] = 0x0;
					buf[2] = 0x0;
				}



				// set to be blocking:
				hid_set_nonblocking(jc->handle, 0);

				// send data:
				hid_write(jc->handle, buf, 9);

				// read response:
				res = hid_read(jc->handle, buf, 65);// returns length of actual bytes read

				// set to be blocking:
				hid_set_nonblocking(jc->handle, 0);

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

	int missedPollCount = 0;
	

	while(true) {

		//auto start = std::chrono::steady_clock::now();
		//std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();


		//#ifdef VIBRATION_TEST
		//for (int l = 0x10; l < 0x20; l++) {
		//	for (int i = 0; i < 8; i++) {
		//		for (int k = 0; k < 256; k++) {
		//			memset(buf, 0, 0x40);
		//			buf[0] = 0x80;
		//			buf[1] = 0x92;
		//			buf[2] = 0x0;
		//			buf[3] = 0xa;
		//			buf[4] = 0x0;
		//			buf[5] = 0x0;
		//			buf[8] = 0x10;
		//			for (int j = 0; j <= 8; j++) {
		//				buf[10 /*+ i*/] = 0x1;//(i + j) & 0xFF;
		//			}

		//			// Set frequency to increase
		//			buf[10 + 0] = k;
		//			buf[10 + 4] = k;

		//			//if (buf[1]) {
		//			//	memcpy(buf[1], buf[0], 0x40);
		//			//}

		//			hid_dual_exchange(joycons[0].handle, joycons[1].handle, buf, buf, 0x40);
		//			printf("Sent %x %x %u\n", i & 0xFF, l, k);
		//		}
		//	}
		//}

		//Joycon *jc = &joycons[0];

		//for (int l = 0x10; l < 0x20; l++) {
		//	for (int i = 1; i < 8; i++) {
		//		for (int k = 0; k < 256; k++) {
		//			memset(buf, 0, 0x40);
		//			for (int j = 0; j <= 8; j++) {
		//				buf[1 + i] = 0x1;//(i + j) & 0xFF;
		//			}

		//			// Set frequency to increase
		//			buf[1 + 0] = k;
		//			buf[1 + 4] = k;

		//			//if (buf) {
		//			//	memcpy(buf[1], buf[0], 0x400);
		//			//}
		//			if (i > 3) {
		//				continue;
		//			}
		//			if (k > 200) {
		//				continue;
		//			}

		//			// set non-blocking:
		//			hid_set_nonblocking(jc->handle, 1);
		//			
		//			joycon_send_command(jc->handle, 0x10, (uint8_t*)buf, 0x9);

		//			printf("Sent %x %x %u\n", i /*& 0xFF*/, l, k);

		//			Sleep(50);
		//		}
		//	}
		//}

		//#endif

		#ifdef LED_TEST
		for (int r = 0; r < 10; ++r) {
			for (int i = 0; i < joycons.size(); ++i) {

				Joycon *jc = &joycons[i];

				//hid_set_nonblocking(jc->handle, 1);

				printf("Enabling some LEDs, sometimes this can fail and take a few times?\n");

				// Player LED Enable
				memset(buf, 0x00, 0x40);
				buf[0] = 0x80 | 0x40 | 0x2 | 0x1; // Flash top two, solid bottom two
				joycon_send_subcommand(jc->handle, 0x1, 0x30, buf, 1);

				// Home LED Enable
				memset(buf, 0x00, 0x40);
				buf[0] = 0xFF; // Slowest pulse?
				joycon_send_subcommand(jc->handle, 0x1, 0x38, buf, 1);

				Sleep(100);
			}
		}
		Sleep(1000000);
		#endif

		

		// input poll loop:

		for (int i = 0; i < joycons.size(); ++i) {

			Joycon *jc = &joycons[i];

			if (!jc->handle) {
				continue;
			}

			


			memset(buf, 0, 65);

			if (settings.usingGrip) {
				buf[0] = 0x80; // 80     Do custom command
				buf[1] = 0x92; // 92     Post-handshake type command
				buf[2] = 0x00; // 0001   u16 second part size
				buf[3] = 0x01;
				buf[8] = 0x1F; // 1F     Get input command
			} else {
				buf[0] = 0x1;// HID get input
				buf[1] = 0x0;
				buf[2] = 0x0;
			}
			


			// set to be non-blocking:
			hid_set_nonblocking(jc->handle, 1);

			// send data:
			written = hid_write(jc->handle, buf, 9);
			//printf("%d bytes written.\n", written);

			// read response:
			read = hid_read(jc->handle, buf, 65);// returns length of actual bytes read
			//printf("%d bytes read.\n", res);

			// set to be blocking:
			//hid_set_nonblocking(jc->handle, 0);


			if (read == 0) {
				missedPollCount += 1;

			} else if (read > 0) {
				// handle input data
				handle_input(jc, buf, res);	
				//printf("%d\n", res);
			}

			if (missedPollCount > 2000) {
				//printf("Connection not stable, retrying\n", i);
				missedPollCount = 0;
				//goto init_start;
			}


			updatevJoyDevice(jc);

		}


		// sleep:
		//Sleep(5);
		Sleep(1);


		//auto end = std::chrono::steady_clock::now();
		//std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
		//auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

		//printf("time: %d\n", duration);

		//if (disconnect) {
		//	printf("DISCONNECTED\n");
		//	goto init_start;
		//}
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
