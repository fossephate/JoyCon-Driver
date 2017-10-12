#include <bitset>
#include <hidapi.h>
#include "tools.hpp"

class Joycon {

public:

	hid_device *handle;
	wchar_t *serial;

	std::string name;

	unsigned char r_buf[65];// read buffer
	unsigned char w_buf[65];// write buffer, what to hid_write to the device

	bool bluetooth = true;

	int left_right = 0;// 1: left joycon, 2: right joycon, 3: pro controller

	uint16_t buttons;
	uint16_t buttons2;// for pro controller

	int8_t dstick;
	uint8_t battery;

	int global_count = 0;

	struct Stick {
		int horizontal;
		int vertical;
	};

	Stick stick;
	Stick stick2;// Pro Controller

	struct Gyroscope {
		// absolute:
		double pitch		= 0;
		double yaw		= 0;
		double roll		= 0;

		// relative:
		double relpitch	= 0;
		double relyaw	= 0;
		double relroll	= 0;

		uint16_t rawrelpitch = 0;
		uint16_t rawrelyaw = 0;
		uint16_t rawrelroll = 0;
	} gyro;

	struct Accelerometer {
		double prevX = 0;
		double prevY = 0;
		double prevZ = 0;
		double x = 0;
		double y = 0;
		double z = 0;
	} accel;


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
	}


	void send_command(int command, uint8_t *data, int len) {
		unsigned char buf[0x40];
		memset(buf, 0, 0x40);

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
		unsigned char buf[0x40];
		memset(buf, 0, 0x40);

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

	void rumble2(uint16_t hf, uint8_t hfa, uint8_t lf, uint16_t lfa) {
		unsigned char buf[0x400];
		memset(buf, 0, 0x40);


		//int hf		= HF;
		//int hf_amp	= HFA;
		//int lf		= LF;
		//int lf_amp	= LFA;
		// maybe:
		//int hf_band = hf + hf_amp;

		int off = 0;// offset
		if (this->left_right == 2) {
			off = 4;
		}


		// Byte swapping
		buf[0 + off] = hf & 0xFF;
		buf[1 + off] = hfa + ((hf >> 8) & 0xFF); //Add amp + 1st byte of frequency to amplitude byte

		// Byte swapping
		buf[2 + off] = lf + ((lfa >> 8) & 0xFF); //Add freq + 1st byte of LF amplitude to the frequency byte
		buf[3 + off] = lfa & 0xFF;


		// set non-blocking:
		hid_set_nonblocking(this->handle, 1);

		send_command(0x10, (uint8_t*)buf, 0x9);
	}

	void rumble3(float frequency, uint8_t hfa, uint16_t lfa) {

		//Float frequency to hex conversion
		if (frequency < 0.0f) {
			frequency = 0.0f;
		} else if (frequency > 1252.0f) {
			frequency = 1252.0f;
		}
		uint8_t encoded_hex_freq = (uint8_t)round(log2((double)frequency / 10.0)*32.0);

		//uint16_t encoded_hex_freq = (uint16_t)floor(-32 * (0.693147f - log(frequency / 5)) / 0.693147f + 0.5f); // old

		//Convert to Joy-Con HF range. Range in big-endian: 0x0004-0x01FC with +0x0004 steps.
		uint16_t hf = (encoded_hex_freq - 0x60) * 4;
		//Convert to Joy-Con LF range. Range: 0x01-0x7F.
		uint8_t lf = encoded_hex_freq - 0x40;

		rumble2(hf, hfa, lf, lfa);
	}



	void rumble4(float real_LF, float real_HF, uint8_t hfa, uint16_t lfa) {

		real_LF = clamp(real_LF, 40.875885f, 626.286133f);
		real_HF = clamp(real_HF, 81.75177, 1252.572266f);

		////Float frequency to hex conversion
		//if (frequency < 0.0f) {
		//	frequency = 0.0f;
		//} else if (frequency > 1252.0f) {
		//	frequency = 1252.0f;
		//}
		//uint8_t encoded_hex_freq = (uint8_t)round(log2((double)frequency / 10.0)*32.0);

		//uint16_t encoded_hex_freq = (uint16_t)floor(-32 * (0.693147f - log(frequency / 5)) / 0.693147f + 0.5f); // old

		////Convert to Joy-Con HF range. Range in big-endian: 0x0004-0x01FC with +0x0004 steps.
		//uint16_t hf = (encoded_hex_freq - 0x60) * 4;
		////Convert to Joy-Con LF range. Range: 0x01-0x7F.
		//uint8_t lf = encoded_hex_freq - 0x40;



		uint16_t hf = ((uint8_t)round(log2((double)real_HF * 0.01)*32.0) - 0x60) * 4;
		uint8_t lf = (uint8_t)round(log2((double)real_LF * 0.01)*32.0) - 0x40;

		rumble2(hf, hfa, lf, lfa);
	}


	void rumble_freq(uint16_t hf, uint8_t hfa, uint8_t lf, uint16_t lfa) {
		unsigned char buf[0x400];
		memset(buf, 0, 0x40);


		//int hf		= HF;
		//int hf_amp	= HFA;
		//int lf		= LF;
		//int lf_amp	= LFA;
		// maybe:
		//int hf_band = hf + hf_amp;

		int off = 0;// offset
		if (this->left_right == 2) {
			off = 4;
		}


		// Byte swapping
		buf[0 + off] = hf & 0xFF;
		buf[1 + off] = hfa + ((hf >> 8) & 0xFF); //Add amp + 1st byte of frequency to amplitude byte

		// Byte swapping
		buf[2 + off] = lf + ((lfa >> 8) & 0xFF); //Add freq + 1st byte of LF amplitude to the frequency byte
		buf[3 + off] = lfa & 0xFF;


		// set non-blocking:
		hid_set_nonblocking(this->handle, 1);

		send_command(0x10, (uint8_t*)buf, 0x9);
	}


	int init_bt() {

		this->bluetooth = true;

		unsigned char buf[0x40];
		memset(buf, 0, 0x40);

		// set blocking to ensure command is recieved:
		hid_set_nonblocking(this->handle, 0);

		// Enable vibration
		printf("Enabling vibration...\n");
		buf[0] = 0x01; // Enabled
		send_subcommand(0x1, 0x48, buf, 1);

		// Enable IMU data
		printf("Enabling IMU data...\n");
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

		// sometimes gets stuck at 0x21 mode until set to 0x31, so do that and then set to 0x30 mode?
		// I don't understand why this happens, but it prevents gyro data from being polled
		printf("Increase data rate for Bluetooth...\n");
		buf[0] = 0x30;
		send_subcommand(0x01, 0x03, buf, 1);
		//buf[0] = 0x30;
		//send_subcommand(0x01, 0x03, buf, 1);


		//printf("Pairing1?...\n");
		//buf[0] = 0x01;
		//send_subcommand(0x01, 0x01, buf, 1);

		//printf("Pairing2?...\n");
		//buf[0] = 0x02;
		//send_subcommand(0x01, 0x01, buf, 1);

		//printf("Pairing3?...\n");
		//buf[0] = 0x03;
		//send_subcommand(0x01, 0x01, buf, 1);



		printf("Successfully initialized %s!\n", this->name.c_str());

		return 0;
	}

	void init_usb() {

		this->bluetooth = false;

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

	void deinit_usb() {
		unsigned char buf[0x40];
		memset(buf, 0x00, 0x40);

		//Let the Joy-Con talk BT again    
		buf[0] = 0x80;
		buf[1] = 0x05;
		hid_exchange(this->handle, buf, 0x2);
		printf("Deinitialized %s\n", this->name.c_str());
	}





	// SPI:

	void spi_write(hid_device *handle, uint32_t offs, uint8_t *data, uint8_t len) {
		unsigned char buf[0x400];
		uint8_t *spi_write = (uint8_t*)calloc(1, 0x26 * sizeof(uint8_t));
		uint32_t* offset = (uint32_t*)(&spi_write[0]);
		uint8_t* length = (uint8_t*)(&spi_write[4]);

		*length = len;
		*offset = offs;
		memcpy(&spi_write[0x5], data, len);

		int max_write_count = 2000;
		int write_count = 0;
		do {
			//usleep(300000);
			write_count += 1;
			memcpy(buf, spi_write, 0x39);
			this->send_subcommand(0x1, 0x11, buf, 0x26);
		} while ((buf[0x10 + (bluetooth ? 0 : 10)] != 0x11 && buf[0] != (bluetooth ? 0x21 : 0x81))
			&& write_count < max_write_count);
		if (write_count > max_write_count)
			printf("ERROR: Write error or timeout\nSkipped writing of %dBytes at address 0x%05X...\n",
				*length, *offset);
	}

	void spi_read(hid_device *handle, uint32_t offs, uint8_t *data, uint8_t len) {
		unsigned char buf[0x400];
		uint8_t *spi_read_cmd = (uint8_t*)calloc(1, 0x26 * sizeof(uint8_t));
		uint32_t* offset = (uint32_t*)(&spi_read_cmd[0]);
		uint8_t* length = (uint8_t*)(&spi_read_cmd[4]);

		*length = len;
		*offset = offs;

		int max_read_count = 2000;
		int read_count = 0;
		do {
			//usleep(300000);
			read_count += 1;
			memcpy(buf, spi_read_cmd, 0x36);
			this->send_subcommand(0x1, 0x10, buf, 0x26);
		} while (*(uint32_t*)&buf[0xF + (bluetooth ? 0 : 10)] != *offset && read_count < max_read_count);
		if (read_count > max_read_count)
			printf("ERROR: Read error or timeout\nSkipped reading of %dBytes at address 0x%05X...\n",
				*length, *offset);


		memcpy(data, &buf[0x14 + (bluetooth ? 0 : 10)], len);
	}

	void spi_flash_dump(hid_device *handle, char *out_path) {
		unsigned char buf[0x400];
		uint8_t *spi_read_cmd = (uint8_t*)calloc(1, 0x26 * sizeof(uint8_t));
		int safe_length = 0x10; // 512KB fits into 32768 * 16B packets
		int fast_rate_length = 0x1D; // Max SPI data that fit into a packet is 29B. Needs removal of last 3 bytes from the dump.

		int length = fast_rate_length;

		FILE *dump = fopen(out_path, "wb");
		if (dump == NULL)
		{
			printf("Failed to open dump file %s, aborting...\n", out_path);
			return;
		}

		uint32_t* offset = (uint32_t*)(&spi_read_cmd[0x0]);
		for (*offset = 0x0; *offset < 0x80000; *offset += length)
		{
			// HACK/TODO: hid_exchange loves to return data from the wrong addr, or 0x30 (NACK?) packets
			// so let's make sure our returned data is okay before writing

			//Set length of requested data
			spi_read_cmd[0x4] = length;

			int max_read_count = 2000;
			int read_count = 0;
			while (1)
			{
				read_count += 1;
				memcpy(buf, spi_read_cmd, 0x26);
				this->send_subcommand(0x1, 0x10, buf, 0x26);

				// sanity-check our data, loop if it's not good
				if ((buf[0] == (bluetooth ? 0x21 : 0x81)
					&& *(uint32_t*)&buf[0xF + (bluetooth ? 0 : 10)] == *offset)
					|| read_count > max_read_count)
					break;
			}

			if (read_count > max_read_count)
			{
				printf("\n\nERROR: Read error or timeout.\nSkipped dumping of %dB at address 0x%05X...\n\n",
					length, *offset);
				return;
			}
			fwrite(buf + (0x14 + (bluetooth ? 0 : 10)) * sizeof(char), length, 1, dump);

			if ((*offset & 0xFF) == 0) // less spam
				printf("\rDumped 0x%05X of 0x80000", *offset);
		}
		printf("\rDumped 0x80000 of 0x80000\n");
		fclose(dump);
	}


};