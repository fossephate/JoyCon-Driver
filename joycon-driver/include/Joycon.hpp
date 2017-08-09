#include <bitset>
#include <hidapi.h>

class Joycon {

public:

	hid_device *handle;
	wchar_t *serial;

	std::string name;

	unsigned char r_buf[65];// read buffer
	unsigned char w_buf[65];// write buffer, what to hid_write to the device

	bool bluetooth = true;

	int left_right = 0;// 1: left joycon, 2: right joycon

	uint16_t buttons;
	bool buttons2[32];
	int8_t dstick;
	uint8_t battery;

	int global_count = 0;

	struct Stick {
		int horizontal;
		int vertical;

		uint8_t horizontal2;
		uint8_t vertical2;
	} stick;

	struct Gyroscope {
		// absolute:
		float pitch		= 0;
		float yaw		= 0;
		float roll		= 0;

		// relative:
		float relpitch	= 0;
		float relyaw	= 0;
		float relroll	= 0;
	} gyro;

	struct Accelerometer {
		float x = 0;
		float y = 0;
		float z = 0;
	} accel;


	uint8_t newButtons[3];


public:

	void hid_exchange(hid_device *handle, unsigned char *buf, int len) {
		if (!handle) return;

		int res;

		res = hid_write(handle, buf, len);

		//if (res < 0) {
		//	printf("Number of bytes written was < 0!\n");
		//} else {
		//	printf("%d bytes written.\n", res);
		//}

		//// set non-blocking:
		//hid_set_nonblocking(handle, 1);

		res = hid_read(handle, buf, 0x40);

		//if (res < 1) {
		//	printf("Number of bytes read was < 1!\n");
		//} else {
		//	printf("%d bytes read.\n", res);
		//}

		#ifdef DEBUG_PRINT
		hex_dump(buf, 0x40);
		#endif
	}


	void send_command(int command, uint8_t *data, int len) {
		unsigned char buf[0x400];
		memset(buf, 0, 0x400);

		if (!bluetooth) {
			buf[0x00] = 0x80;
			buf[0x01] = 0x92;
			buf[0x03] = 0x31;
		}

		buf[bluetooth ? 0x0 : 0x8] = command;
		if (data != nullptr && len != 0) {
			memcpy(buf + (bluetooth ? 0x1 : 0x9), data, len);
		}

		hid_exchange(this->handle, buf, len + (bluetooth ? 0x1 : 0x9));

		if (data) {
			memcpy(data, buf, 0x40);
		}
	}

	void send_subcommand(int command, int subcommand, uint8_t *data, int len) {
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

		send_command(command, buf, 10 + len);

		if (data) {
			memcpy(data, buf, 0x40); //TODO
		}
	}

	void rumble(int frequency, int intensity) {

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
		if (this->left_right == 1) {
			buf[1 + 0] = frequency;// (0, 255)
		} else {
			buf[1 + 4] = frequency;// (0, 255)
		}

		// set non-blocking:
		hid_set_nonblocking(this->handle, 1);

		send_command(0x10, (uint8_t*)buf, 0x9);
	}

	void rumble2(int HF, int HFA, int LF, int LFA) {
		unsigned char buf[0x400];
		memset(buf, 0, 0x40);


		int hf = HF;
		int hf_amp = HFA;
		int lf = LF;
		int lf_amp = LFA;
		// maybe:
		//int hf_band = hf + hf_amp;

		int off = 0;// offset
		if (this->left_right == 2) {
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
		hid_set_nonblocking(this->handle, 1);

		send_command(0x10, (uint8_t*)buf, 0x9);
	}

	void rumble3(int frequency, int HFA, int LFA) {

		//Float frequency to hex conversion
		uint16_t encoded_hex_freq = (uint16_t)floor(-32 * (0.693147f - log(frequency / 5)) / 0.693147f + 0.5f);

		//Convert to Joy-Con HF range. Range in big-endian: 0x0004-0x01FC with +0x0004 steps.
		uint16_t hf = (encoded_hex_freq - 0x60) * 4;
		//Convert to Joy-Con LF range. Range: 0x0100-0x7F00.
		uint16_t lf = encoded_hex_freq - 0x40;

		rumble2(hf, HFA, lf, LFA);
	}

	void rumble4(int frequency, int HFA, int LFA) {

		//Float frequency to hex conversion
		uint16_t encoded_hex_freq = (uint16_t)floor(-32 * (0.693147f - log(frequency / 5)) / 0.693147f + 0.5f);

		//Convert to Joy-Con HF range. Range in big-endian: 0x0004-0x01FC with +0x0004 steps.
		uint16_t hf = (encoded_hex_freq - 0x60) * 4;
		//Convert to Joy-Con LF range. Range: 0x0100-0x7F00.
		uint16_t lf = encoded_hex_freq - 0x40;

		rumble2(hf, HFA, lf, LFA);
	}


	void init_usb() {
		unsigned char buf[0x400];
		memset(buf, 0, 0x400);

		// set blocking:
		// this insures we get the MAC Address
		hid_set_nonblocking(this->handle, 0);

		//Get MAC Left
		printf("Getting MAC...\n");
		memset(buf, 0x00, 0x40);
		buf[0] = 0x80;
		buf[1] = 0x01;
		hid_exchange(this->handle, buf, 0x2);

		if (buf[2] == 0x3) {
			printf("%s disconnected!\n", this->name.c_str());
		} else {
			printf("Found %s, MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", this->name.c_str(), buf[9], buf[8], buf[7], buf[6], buf[5], buf[4]);
		}

		// set non-blocking:
		//hid_set_nonblocking(jc->handle, 1);

		// Do handshaking
		printf("Doing handshake...\n");
		memset(buf, 0x00, 0x40);
		buf[0] = 0x80;
		buf[1] = 0x02;
		hid_exchange(this->handle, buf, 0x2);

		// Switch baudrate to 3Mbit
		printf("Switching baudrate...\n");
		memset(buf, 0x00, 0x40);
		buf[0] = 0x80;
		buf[1] = 0x03;
		hid_exchange(this->handle, buf, 0x2);

		//Do handshaking again at new baudrate so the firmware pulls pin 3 low?
		printf("Doing handshake...\n");
		memset(buf, 0x00, 0x40);
		buf[0] = 0x80;
		buf[1] = 0x02;
		hid_exchange(this->handle, buf, 0x2);

		//Only talk HID from now on
		printf("Only talk HID...\n");
		memset(buf, 0x00, 0x40);
		buf[0] = 0x80;
		buf[1] = 0x04;
		hid_exchange(this->handle, buf, 0x2);

		// Enable vibration
		printf("Enabling vibration...\n");
		memset(buf, 0x00, 0x400);
		buf[0] = 0x01; // Enabled
		send_subcommand(0x1, 0x48, buf, 1);

		// Enable IMU data
		printf("Enabling IMU data...\n");
		memset(buf, 0x00, 0x400);
		buf[0] = 0x01; // Enabled
		send_subcommand(0x1, 0x40, buf, 1);

		printf("Successfully initialized %s!\n", this->name.c_str());
	}


	int init_bt() {

		unsigned char buf[0x400];
		memset(buf, 0, 0x400);

		// set non-blocking:
		hid_set_nonblocking(jc->handle, 1);

		// Enable vibration
		printf("Enabling vibration...\n");
		memset(buf, 0x00, 0x400);
		buf[0] = 0x01; // Enabled
		send_subcommand(0x1, 0x48, buf, 1);

		// Enable IMU data
		printf("Enabling IMU data...\n");
		memset(buf, 0x00, 0x400);
		buf[0] = 0x01; // Enabled
		send_subcommand(0x01, 0x40, buf, 1);


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
		send_subcommand(0x01, 0x03, buf, 1);


		printf("Pairing1?...\n");
		memset(buf, 0x00, 0x400);
		buf[0] = 0x01;
		send_subcommand(0x01, 0x01, buf, 1);

		printf("Pairing2?...\n");
		memset(buf, 0x00, 0x400);
		buf[0] = 0x02;
		send_subcommand(0x01, 0x01, buf, 1);

		printf("Pairing3?...\n");
		memset(buf, 0x00, 0x400);
		buf[0] = 0x03;
		send_subcommand(0x01, 0x01, buf, 1);



		printf("Successfully initialized %s!\n", jc->name.c_str());

		return 0;
	}

	void deinit_usb() {
		unsigned char buf[0x40];
		memset(buf, 0x00, 0x40);

		//Let the Joy-Con talk BT again    
		buf[0] = 0x80;
		buf[1] = 0x05;
		hid_exchange(this->handle, buf, 0x2);
		printf("Deinitialized %s\n", this->name.c_str());
	}


};