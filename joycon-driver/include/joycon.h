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
} Joycon;