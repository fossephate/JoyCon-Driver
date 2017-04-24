#ifdef WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
//#include <unistd.h>
#include <chrono>
#include <ctime>
#include <cstring>
#include <hidapi.h>



// Uncomment for spam or SPI dumping
//#define DEBUG_PRINT
//#define DUMP_SPI
//#define REPLAY
//#define WEIRD_VIBRATION_TEST
#define INPUT_LOOP
#pragma warning(disable:4996)

void hex_dump(unsigned char *buf, int len) {
	for (int i = 0; i < len; i++)
		printf("%02x ", buf[i]);
	printf("\n");
}

void hid_exchange(hid_device *handle, unsigned char *buf, int len)
{
	if (!handle) return; //TODO: idk I just don't like this to be honest

	hid_write(handle, buf, len);

	int res = hid_read(handle, buf, 0x41);
#ifdef DEBUG_PRINT
	hex_dump(buf, 0x40);
#endif
}

void hid_dual_exchange(hid_device *handle_l, hid_device *handle_r, unsigned char *buf_l, unsigned char *buf_r, int len)
{
	if (handle_l && buf_l)
	{
		hid_set_nonblocking(handle_l, 1);
		hid_write(handle_l, buf_l, len);
		hid_read(handle_l, buf_l, 65);
#ifdef DEBUG_PRINT
		hex_dump(buf_l, 0x40);
#endif
		hid_set_nonblocking(handle_l, 0);
	}

	if (handle_r && buf_r)
	{
		hid_set_nonblocking(handle_r, 1);
		hid_write(handle_r, buf_r, len);
		hid_read(handle_r, buf_r, 65);
#ifdef DEBUG_PRINT
		hex_dump(buf_r, 0x40);
#endif
		hid_set_nonblocking(handle_r, 0);
	}
}

void spi_flash_dump(hid_device *handle, char *out_path)
{
	unsigned char buf[0x40];
	unsigned char spi_read[0x39] = { 0x80, 0x92, 0x0, 0x31, 0x0, 0x0, 0xd4, 0xe6, 0x1, 0xc, 0x0, 0x1, 0x40, 0x40, 0x0, 0x1, 0x40, 0x40, 0x10, 0x00, 0x0, 0x0, 0x0, 0x1C, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

	FILE *dump = fopen(out_path, "wb");
	for (*(uint32_t*)(&spi_read[0x13]) = 0; *(uint32_t*)(&spi_read[0x13]) < 0x80000; *(uint32_t*)(&spi_read[0x13]) += 0x1C)
	{
		memcpy(buf, spi_read, 0x39);
		hid_exchange(handle, buf, 0x39);

		fwrite(buf + 0x1E * sizeof(char), 0x1C, 1, dump);
	}
	fclose(dump);
}

int joycon_init(hid_device *handle, const char *name)
{
	unsigned char buf[0x40];
	memset(buf, 0, 0x40);

	//Get MAC Left
	memset(buf, 0x00, 0x40);
	buf[0] = 0x80;
	buf[1] = 0x01;
	hid_exchange(handle, buf, 0x2);

	if (buf[2] == 0x3)
	{
		printf("%s disconnected!\n", name);
		return -1;
	} else
	{
		printf("Found %s, MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", name, buf[9], buf[8], buf[7], buf[6], buf[5], buf[4]);
	}

	//Do handshaking
	memset(buf, 0x00, 0x40);
	buf[0] = 0x80;
	buf[1] = 0x02;
	hid_exchange(handle, buf, 0x2);

	

	// Switch baudrate to 3Mbit
#ifndef WEIRD_VIBRATION_TEST
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

void joycon_deinit(hid_device *handle, char *name)
{
	unsigned char buf[0x40];
	memset(buf, 0x00, 0x40);

	//Let the Joy-Con talk BT again	
	buf[0] = 0x80;
	buf[1] = 0x05;
	hid_exchange(handle, buf, 0x2);

	printf("Deinitialized %s\n", name);
}

int main(int argc, char* argv[])
{
	int res;
	unsigned char* buf[2] = { 0 };
	hid_device *handle_l = 0, *handle_r = 0;
	struct hid_device_info *devs, *dev_iter;
	bool pro_controller = false;

	res = hid_init();

	devs = hid_enumerate(0x057e, 0x200e); //TODO: Pro Controller? Will probably need changes to init...
	dev_iter = devs;
	while (dev_iter)
	{
		if (dev_iter->interface_number == 0)
		{
			handle_r = hid_open_path(dev_iter->path);
			buf[0] = (unsigned char*)malloc(0x40);
			memset(buf[0], 0, 0x40);

			if (joycon_init(handle_r, dev_iter->product_id == 0x200e ? "Joy-Con (R)" : "Pro Controller"))
				handle_r = NULL;

			pro_controller = (dev_iter->product_id != 0x200e); //TODO idk about this
		} else if (dev_iter->interface_number == 1)
		{
			handle_l = hid_open_path(dev_iter->path);
			buf[1] = (unsigned char*)malloc(0x40);
			memset(buf[1], 0, 0x40);

			if (joycon_init(handle_l, "Joy-Con (L)"))
				handle_l = NULL;
		}

		dev_iter = dev_iter->next;
	}
	hid_free_enumeration(devs);

	if (!handle_r)
	{
		printf("Failed to get handle for interface 0 (right Joy-Con or Pro Controller), exiting...\n");
		//return -1;
	}

	// Only missing one half by this point
	if (!handle_l && !pro_controller)
	{
		printf("Could not get handles for both Joy-Con! Exiting...");
	}

#ifdef DUMP_SPI
	printf("Dumping Joy-Con SPI flashes...");
	if (handle_l)
		spi_flash_dump(handle_l, "left_joycon_dump.bin");
	spi_flash_dump(handle_r, "right_joycon_dump.bin");
#endif

	// Replays a string of hex values (ie 80 92 .. ..) separated by newlines
#ifdef REPLAY
	ssize_t read;
	char *line;
	size_t len = 0;
	FILE *replay = fopen("replay.txt", "rb");
	while ((read = getline(&line, &len, replay)) > 0) {
		int i = 0;

		memset(buf[0], 0, 0x40);

		char *line_temp = line;
		while (i < 0x40)
		{
			buf[0][i++] = strtol(line_temp, &line_temp, 16);
		}
		if (buf[0][8] == 0x1f) continue; //Cull out input packets

		printf("Sent: ");
		hex_dump(buf[0], 0x40);

		if (buf[1])
			memcpy(buf[1], buf[0], 0x40);

		hid_dual_exchange(handle_l, handle_r, buf[1], buf[0], 0x40);
		//hid_exchange(handle_r, buf_r, 0x40);
		printf("Got:  ");
		hex_dump(buf[0], 0x40);
		printf("\n");
	}
#endif

#ifdef WEIRD_VIBRATION_TEST
	for (int l = 0x10; l < 0x20; l++)
	{
		for (int i = 0; i < 8; i++)
		{
			for (int k = 0; k < 256; k++)
			{
				memset(buf[0], 0, 0x40);
				buf[0][0] = 0x80;
				buf[0][1] = 0x92;
				buf[0][2] = 0x0;
				buf[0][3] = 0xa;
				buf[0][4] = 0x0;
				buf[0][5] = 0x0;
				buf[0][8] = 0x10;
				for (int j = 0; j <= 8; j++) {
					buf[0][10 + i] = 0x1;//(i + j) & 0xFF;
				}

				// Set frequency to increase
				buf[0][10 + 0] = k;
				buf[0][10 + 4] = k;

				if (buf[1])
					memcpy(buf[1], buf[0], 0x40);

				hid_dual_exchange(handle_l, handle_r, buf[1], buf[0], 0x40);
				printf("Sent %x %x %u\n", i & 0xFF, l, k);
			}
		}
	}
#endif

#ifdef INPUT_LOOP
	printf("Start input poll loop\n");

	unsigned long last = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
	while (1) {
		printf("%02llums delay,  ", (std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1)) - last);
		last = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);

		buf[0][0] = 0x80; // 80     Do custom command
		buf[0][1] = 0x92; // 92     Post-handshake type command
		buf[0][2] = 0x00; // 0001   u16 second part size
		buf[0][3] = 0x01;
		buf[0][8] = 0x1F; // 1F     Get input command

		if (buf[1])
			memcpy(buf[1], buf[0], 0x9);
		hid_dual_exchange(handle_l, handle_r, buf[1], buf[0], 0x9);

		if (buf[1])
		{
			printf("left ");
			hex_dump(buf[1], 0x3D);
			printf("            ");
		}

		printf("right ");
		hex_dump(buf[0], 0x3D);
	}
#endif

	if (handle_l)
	{
		joycon_deinit(handle_l, "Joy-Con (L)");
		hid_close(handle_l);
	}

	if (handle_r)
	{
		joycon_deinit(handle_r, "Joy-Con (R)");
		hid_close(handle_r);
	}

	// Finalize the hidapi library
	res = hid_exit();

	return 0;
}