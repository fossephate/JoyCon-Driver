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

#pragma warning(disable:4996)

// Uncomment for spam or SPI dumping
//#define DEBUG_PRINT
//#define DUMP_SPI
//#define REPLAY
#define INPUT_LOOP

void hex_dump(unsigned char *buf, int len) {
	for (int i = 0; i < len; i++) {
		printf("%02x ", buf[i]);
	}
    printf("\n");
}

void hid_exchange(hid_device *handle, unsigned char *buf, int len) {
    if(!handle) return; //TODO: idk I just don't like this to be honest
    
    hid_write(handle, buf, len);

	int res = hid_read(handle, buf, 0x41);
#ifdef DEBUG_PRINT
	hex_dump(buf, 0x40);
#endif
}

void hid_dual_exchange(hid_device *handle_l, hid_device *handle_r, unsigned char *buf_l, unsigned char *buf_r, int len) {
    hid_set_nonblocking(handle_l, 1);
    hid_set_nonblocking(handle_r, 1);

    hid_write(handle_l, buf_l, len);
    hid_write(handle_r, buf_r, len);

	hid_read(handle_l, buf_l, 65);
	hid_read(handle_r, buf_r, 65);
#ifdef DEBUG_PRINT
	hex_dump(buf_l, 0x40);
	hex_dump(buf_r, 0x40);
#endif
    
    hid_set_nonblocking(handle_l, 0);
    hid_set_nonblocking(handle_r, 0);
}

void spi_flash_dump(hid_device *handle, char *out_path) {
    unsigned char buf[0x40];
    unsigned char spi_read[0x39] = {0x80, 0x92, 0x0, 0x31, 0x0, 0x0, 0xd4, 0xe6, 0x1, 0xc, 0x0, 0x1, 0x40, 0x40, 0x0, 0x1, 0x40, 0x40, 0x10, 0x00, 0x0, 0x0, 0x0, 0x1C, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
	
	FILE *dump = fopen(out_path, "wb");
	for(*(uint32_t*)(&spi_read[0x13]) = 0; *(uint32_t*)(&spi_read[0x13]) < 0x80000; *(uint32_t*)(&spi_read[0x13]) += 0x1C) {
	    memcpy(buf, spi_read, 0x39);
	    hid_exchange(handle, buf, 0x39);
	    
	    fwrite(buf + 0x1E * sizeof(char), 0x1C, 1, dump);
	}
	fclose(dump);
}

int main(int argc, char* argv[]) {
	int res;
	unsigned char buf_l[0x40];
	unsigned char buf_r[0x40];
	hid_device *handle_l = nullptr;
	hid_device *handle_r = nullptr;

	struct hid_device_info *devs, *dev_iter;

	res = hid_init();

	devs = hid_enumerate(0x057e, 0x200e); //TODO: Pro Controller? Will probably need changes to init...
	dev_iter = devs;
	while (dev_iter) {
		if (dev_iter->interface_number == 0) {
			handle_r = hid_open_path(dev_iter->path);
		} else if (dev_iter->interface_number == 1) {
			handle_l = hid_open_path(dev_iter->path);
		}

		dev_iter = dev_iter->next;
	}
	hid_free_enumeration(devs);
	
	if(!handle_l || !handle_r) {
	    fprintf(stderr, "Could not get handle(s) for Joy-Con! Handles L %08x, R %08x\n", handle_l, handle_r);
	    return -1;
	}
	
	//Get MAC Left
	memset(buf_l, 0x00, 0x40);
	buf_l[0] = 0x80;
	buf_l[1] = 0x01;
	hid_exchange(handle_l, buf_l, 0x2);
	
	if(buf_l[2] == 0x3) {
	    fprintf(stderr, "Left Joy-Con disconnected! Cannot initialize without both Joy-Con!\n");
	    return -1;
	} else {
	    printf("Found Joy-Con (L), MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", buf_l[9], buf_l[8], buf_l[7], buf_l[6], buf_l[5], buf_l[4]);
	}
	
	//Get MAC Right
	memset(buf_r, 0x00, 0x40);
	buf_r[0] = 0x80;
	buf_r[1] = 0x01;
	hid_exchange(handle_r, buf_r, 0x2);
	
	if(buf_r[2] == 0x3) {
	    fprintf(stderr, "Right Joy-Con disconnected! Cannot initialize without both Joy-Con!\n");
	    return -1;
	} else {
	    printf("Found Joy-Con (R), MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", buf_l[9], buf_l[8], buf_l[7], buf_l[6], buf_l[5], buf_l[4]);
	}
	
	//Do handshaking
	memset(buf_l, 0x00, 0x40);
	buf_l[0] = 0x80;
	buf_l[1] = 0x02;
	hid_exchange(handle_l, buf_l, 0x2);
	
	memset(buf_r, 0x00, 0x40);
	buf_r[0] = 0x80;
	buf_r[1] = 0x02;
	hid_exchange(handle_r, buf_r, 0x2);
	
	printf("Switching baudrate...\n");
	
	// Switch baudrate to 3Mbit
	memset(buf_l, 0x00, 0x40);
	buf_l[0] = 0x80;
	buf_l[1] = 0x03;
	hid_exchange(handle_l, buf_l, 0x2);

	memset(buf_r, 0x00, 0x40);
	buf_r[0] = 0x80;
	buf_r[1] = 0x03;
	hid_exchange(handle_r, buf_r, 0x2);
	
	//Do handshaking again at new baudrate so the firmware pulls pin 3 low?
	memset(buf_l, 0x00, 0x40);
	buf_l[0] = 0x80;
	buf_l[1] = 0x02;
	hid_exchange(handle_l, buf_l, 0x2);
	
	memset(buf_r, 0x00, 0x40);
	buf_r[0] = 0x80;
	buf_r[1] = 0x02;
	hid_exchange(handle_r, buf_r, 0x2);
	
#ifdef DUMP_SPI
	printf("Dumping Joy-Con SPI flashes...");
	spi_flash_dump(handle_l, "left_joycon_dump.bin");
	spi_flash_dump(handle_r, "right_joycon_dump.bin");
#endif
	
// Replays a string of hex values (ie 80 92 .. ..) separated by newlines
#ifdef REPLAY
	ssize_t read;
	char *line;
	size_t len = 0;
	FILE *replay = fopen("replay.txt", "rb");
	hid_set_nonblocking(handle_l, 1);
	while ((read = getline(&line, &len, replay)) > 0) {
	    int i = 0;
	    
	    memset(buf_l, 0, 0x40);
	    
	    char *line_temp = line;
	    while(i < 0x40) {
	        buf_l[i++] = strtol(line_temp, &line_temp, 16);
	    }
	    if(buf_l[8] == 0x1f) continue; //Cull out input packets

        printf("Sent: ");
        hex_dump(buf_l, 0x40);
        memcpy(buf_r, buf_l, 0x40);
	    hid_exchange(handle_l, buf_l, 0x40);
	    //hid_exchange(handle_r, buf_r, 0x40);
	    printf("Got:  ");
	    hex_dump(buf_l, 0x40);
	    printf("\n");
	}
	hid_set_nonblocking(handle_l, 0);
#endif
	
#ifdef INPUT_LOOP
	printf("Start input poll loop\n");
	
	unsigned long last = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
	while(1) {
	    //printf("%02llums delay,  left ", (std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1)) - last);
	    last = std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);

	    buf_l[0] = 0x80; // 80     Do custom command
	    buf_l[1] = 0x92; // 92     Post-handshake type command
	    buf_l[2] = 0x00; // 0001   u16 second part size
	    buf_l[3] = 0x01;
	    buf_l[8] = 0x1F; // 1F     Get input command
	    
	    memcpy(buf_r, buf_l, 0x9);
	    hid_dual_exchange(handle_l, handle_r, buf_l, buf_r, 0x9);
	    
	    //hex_dump(buf_l, 0x3D);
	    //printf("  right ");
	    hex_dump(buf_r, 0x3D);
    }
#endif
    
    hid_close(handle_l);
    hid_close(handle_r);

	// Finalize the hidapi library
	res = hid_exit();

	return 0;
}
