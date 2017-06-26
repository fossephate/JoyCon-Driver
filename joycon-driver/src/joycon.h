#include <bitset>
#include <hidapi.h>

typedef struct {

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
		// absolute:
		int pitch;
		int yaw;
		int roll;

		// relative:
		int relpitch;
		int relyaw;
		int relroll;
	} gyro;

	struct Accelerometer {
		int x;
		int y;
		int z;
	} accel;


	uint8_t newButtons[3];
} Joycon;