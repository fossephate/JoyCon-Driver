
#include <Windows.h>
#pragma comment(lib, "user32.lib")

#include <bitset>
#include <random>
#include <stdafx.h>
#include <string.h>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <fstream>

#include <hidapi.h>

#include "public.h"
#include "vjoyinterface.h"

#include "packet.h"
#include "joycon.hpp"
#include "MouseController.hpp"
#include "tools.hpp"

// wxWidgets:
//#include <wx/wx.h>
//#include <wx/glcanvas.h>
//#include <cube.h>
//#include <MyApp.h>

// glm:
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

// curl:
#include <curl/curl.h>


#define JOYCON_VENDOR 0x057e
#define JOYCON_L_BT 0x2006
#define JOYCON_R_BT 0x2007
#define PRO_CONTROLLER 0x2009
#define JOYCON_CHARGING_GRIP 0x200e
#define SERIAL_LEN 18
#define PI 3.14159265359
#define L_OR_R(lr) (lr == 1 ? 'L' : (lr == 2 ? 'R' : '?'))

std::vector<Joycon> joycons;
unsigned char buf[65];
int res = 0;


struct Tracker {

	int var1 = 0;
	int var2 = 0;
	int counter1 = 0;

	float low_freq = 200.0f;
	float high_freq = 500.0f;

	float relX = 0;
	float relY = 0;

	float anglex = 0;
	float angley = 0;
	float anglez = 0;

	glm::fquat quat = glm::angleAxis(0.0f, glm::vec3(1.0, 0.0, 0.0));

	std::vector<std::chrono::steady_clock::time_point> tPolls;

	float previousPitch = 0;

} tracker;




void handle_input(Joycon *jc, uint8_t *packet, int len) {

	// bluetooth button pressed packet:
	if (packet[0] == 0x3F) {

		uint16_t old_buttons = jc->buttons;
		int8_t old_dstick = jc->dstick;

		jc->dstick = packet[3];
		// todo: get button states here aswell:
	}

	// input update packet:
	// 0x21 is just buttons, 0x30 includes gyro, 0x31 includes NFC (large packet size)
	if (packet[0] == 0x21 || packet[0] == 0x30 || packet[0] == 0x31) {

		// offset for usb or bluetooth data:
		/*int offset = settings.usingBluetooth ? 0 : 10;*/
		int offset = jc->bluetooth ? 0 : 10;

		uint8_t *btn_data = packet + offset + 3;

		// get button states:
		{
			uint16_t states = 0;
			uint16_t states2 = 0;

			// Left JoyCon:
			if (jc->left_right == 1) {
				states = (btn_data[1] << 8) | (btn_data[2] & 0xFF);
				// Right JoyCon:
			} else if (jc->left_right == 2) {
				states = (btn_data[1] << 8) | (btn_data[0] & 0xFF);
				// Pro Controller:
			} else if (jc->left_right == 3) {
				states = (btn_data[1] << 8) | (btn_data[2] & 0xFF);
				states2 = (btn_data[1] << 8) | (btn_data[0] & 0xFF);
			}

			jc->buttons = states;
			// Pro Controller:
			if (jc->left_right == 3) {
				jc->buttons2 = states2;

				// fix some non-sense the Pro Controller does
				// clear nth bit
				//num &= ~(1UL << n);
				jc->buttons &= ~(1UL << 9);
				jc->buttons &= ~(1UL << 10);
				jc->buttons &= ~(1UL << 12);
				jc->buttons &= ~(1UL << 14);

				jc->buttons2 &= ~(1UL << 8);
				jc->buttons2 &= ~(1UL << 11);
				jc->buttons2 &= ~(1UL << 13);
			}
		}

		// get stick data:
		uint8_t *stick_data = packet + offset;
		if (jc->left_right == 1) {
			stick_data += 6;
		} else if (jc->left_right == 2) {
			stick_data += 9;
		}

		uint16_t stick_x = stick_data[0] | ((stick_data[1] & 0xF) << 8);
		uint16_t stick_y = (stick_data[1] >> 4) | (stick_data[2] << 4);
		jc->stick.x = stick_x;
		jc->stick.y = stick_y;

		// use calibration data:
		jc->CalcAnalogStick();

		// pro controller:
		if (jc->left_right == 3) {
			stick_data += 6;
			uint16_t stick_x = stick_data[0] | ((stick_data[1] & 0xF) << 8);
			uint16_t stick_y = (stick_data[1] >> 4) | (stick_data[2] << 4);
			jc->stick.x = (int)(unsigned int)stick_x;
			jc->stick.y = (int)(unsigned int)stick_y;
			stick_data += 3;
			uint16_t stick_x2 = stick_data[0] | ((stick_data[1] & 0xF) << 8);
			uint16_t stick_y2 = (stick_data[1] >> 4) | (stick_data[2] << 4);
			jc->stick2.x = (int)(unsigned int)stick_x2;
			jc->stick2.y = (int)(unsigned int)stick_y2;

			// calibration data:
			jc->CalcAnalogStick();
		}

		jc->battery = (stick_data[1] & 0xF0) >> 4;
		//printf("JoyCon battery: %d\n", jc->battery);

		// Accelerometer:
		// Accelerometer data is absolute (m/s^2)
		{

			// get accelerometer X:
			jc->accel.x = (float)(uint16_to_int16(packet[13] | (packet[14] << 8) & 0xFF00)) * jc->acc_cal_coeff[0];

			// get accelerometer Y:
			jc->accel.y = (float)(uint16_to_int16(packet[15] | (packet[16] << 8) & 0xFF00)) * jc->acc_cal_coeff[1];

			// get accelerometer Z:
			jc->accel.z = (float)(uint16_to_int16(packet[17] | (packet[18] << 8) & 0xFF00)) * jc->acc_cal_coeff[2];
		}



		// Gyroscope:
		// Gyroscope data is relative (rads/s)
		{

			// get roll:
			jc->gyro.roll = (float)((uint16_to_int16(packet[19] | (packet[20] << 8) & 0xFF00)) - jc->sensor_cal[1][0]) * jc->gyro_cal_coeff[0];

			// get pitch:
			jc->gyro.pitch = (float)((uint16_to_int16(packet[21] | (packet[22] << 8) & 0xFF00)) - jc->sensor_cal[1][1]) * jc->gyro_cal_coeff[1];

			// get yaw:
			jc->gyro.yaw = (float)((uint16_to_int16(packet[23] | (packet[24] << 8) & 0xFF00)) - jc->sensor_cal[1][2]) * jc->gyro_cal_coeff[2];
		}

		// offsets:
		{
			jc->setGyroOffsets();

			jc->gyro.roll -= jc->gyro.offset.roll;
			jc->gyro.pitch -= jc->gyro.offset.pitch;
			jc->gyro.yaw -= jc->gyro.offset.yaw;

			//tracker.counter1++;
			//if (tracker.counter1 > 10) {
			//	tracker.counter1 = 0;
			//	printf("%.3f %.3f %.3f\n", abs(jc->gyro.roll), abs(jc->gyro.pitch), abs(jc->gyro.yaw));
			//}
		}


		//hex_dump(gyro_data, 20);

		if (jc->left_right == 1) {
			//hex_dump(gyro_data, 20);
			//hex_dump(packet+12, 20);
			//printf("x: %f, y: %f, z: %f\n", tracker.anglex, tracker.angley, tracker.anglez);
			//printf("%04x\n", jc->stick.x);
			//printf("%f\n", jc->stick.CalX);
			//printf("%d\n", jc->gyro.yaw);
			//printf("%02x\n", jc->gyro.roll);
			//printf("%04x\n", jc->gyro.yaw);
			//printf("%04x\n", jc->gyro.roll);
			//printf("%f\n", jc->gyro.roll);
			//printf("%d\n", accelXA);
			//printf("%d\n", jc->buttons);
			//printf("%.4f\n", jc->gyro.pitch);
			//printf("%04x\n", accelX);
			//printf("%02x %02x\n", rollA, rollB);
		}

	}






	// handle button combos:
	{

		// press up, down, left, right, L, ZL to restart:
		if (jc->left_right == 1) {
			//if (jc->buttons == 207) {
			//	settings.restart = true;
			//}

			// remove this, it's just for rumble testing
			//uint8_t hfa2 = 0x88;
			//uint16_t lfa2 = 0x804d;

			//tracker.low_freq = clamp(tracker.low_freq, 41.0f, 626.0f);
			//tracker.high_freq = clamp(tracker.high_freq, 82.0f, 1252.0f);
			//
			//// down:
			//if (jc->buttons == 1) {
			//	tracker.high_freq -= 1;
			//	jc->rumble4(tracker.low_freq, tracker.high_freq, hfa2, lfa2);
			//}
			//// down:
			//if (jc->buttons == 2) {
			//	tracker.high_freq += 1;
			//	jc->rumble4(tracker.low_freq, tracker.high_freq, hfa2, lfa2);
			//}

			//// left:
			//if (jc->buttons == 8) {
			//	tracker.low_freq -= 1;
			//	jc->rumble4(tracker.low_freq, tracker.high_freq, hfa2, lfa2);
			//}
			//// right:
			//if (jc->buttons == 4) {
			//	tracker.low_freq += 1;
			//	jc->rumble4(tracker.low_freq, tracker.high_freq, hfa2, lfa2);
			//}

			////printf("%i\n", jc->buttons);
			////printf("%f\n", tracker.frequency);
			//printf("%f %f\n", tracker.low_freq, tracker.high_freq);
		}


		// left:
		if (jc->left_right == 1) {
			jc->btns.down = (jc->buttons & (1 << 0)) ? 1 : 0;
			jc->btns.up = (jc->buttons & (1 << 1)) ? 1 : 0;
			jc->btns.right = (jc->buttons & (1 << 2)) ? 1 : 0;
			jc->btns.left = (jc->buttons & (1 << 3)) ? 1 : 0;
			jc->btns.sr = (jc->buttons & (1 << 4)) ? 1 : 0;
			jc->btns.sl = (jc->buttons & (1 << 5)) ? 1 : 0;
			jc->btns.l = (jc->buttons & (1 << 6)) ? 1 : 0;
			jc->btns.zl = (jc->buttons & (1 << 7)) ? 1 : 0;
			jc->btns.minus = (jc->buttons & (1 << 8)) ? 1 : 0;
			jc->btns.stick_button = (jc->buttons & (1 << 11)) ? 1 : 0;
			jc->btns.capture = (jc->buttons & (1 << 13)) ? 1 : 0;


			if (/*settings.debugMode*/false) {
				printf("U: %d D: %d L: %d R: %d LL: %d ZL: %d SB: %d SL: %d SR: %d M: %d C: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
					jc->btns.up, jc->btns.down, jc->btns.left, jc->btns.right, jc->btns.l, jc->btns.zl, jc->btns.stick_button, jc->btns.sl, jc->btns.sr, \
					jc->btns.minus, jc->btns.capture, (jc->stick.CalX + 1), (jc->stick.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);
			}
			//if (settings.writeDebugToFile) {
			//	fprintf(settings.outputFile, "U: %d D: %d L: %d R: %d LL: %d ZL: %d SB: %d SL: %d SR: %d M: %d C: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
			//		jc->btns.up, jc->btns.down, jc->btns.left, jc->btns.right, jc->btns.l, jc->btns.zl, jc->btns.stick_button, jc->btns.sl, jc->btns.sr, \
			//		jc->btns.minus, jc->btns.capture, (jc->stick.CalX + 1), (jc->stick.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);
			//}
		}

		// right:
		if (jc->left_right == 2) {
			jc->btns.y = (jc->buttons & (1 << 0)) ? 1 : 0;
			jc->btns.x = (jc->buttons & (1 << 1)) ? 1 : 0;
			jc->btns.b = (jc->buttons & (1 << 2)) ? 1 : 0;
			jc->btns.a = (jc->buttons & (1 << 3)) ? 1 : 0;
			jc->btns.sr = (jc->buttons & (1 << 4)) ? 1 : 0;
			jc->btns.sl = (jc->buttons & (1 << 5)) ? 1 : 0;
			jc->btns.r = (jc->buttons & (1 << 6)) ? 1 : 0;
			jc->btns.zr = (jc->buttons & (1 << 7)) ? 1 : 0;
			jc->btns.plus = (jc->buttons & (1 << 9)) ? 1 : 0;
			jc->btns.stick_button = (jc->buttons & (1 << 10)) ? 1 : 0;
			jc->btns.home = (jc->buttons & (1 << 12)) ? 1 : 0;


			if (/*settings.debugMode*/false) {
				printf("A: %d B: %d X: %d Y: %d RR: %d ZR: %d SB: %d SL: %d SR: %d P: %d H: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
					jc->btns.a, jc->btns.b, jc->btns.x, jc->btns.y, jc->btns.r, jc->btns.zr, jc->btns.stick_button, jc->btns.sl, jc->btns.sr, \
					jc->btns.plus, jc->btns.home, (jc->stick.CalX + 1), (jc->stick.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);
			}
			//if (settings.writeDebugToFile) {
			//	fprintf(settings.outputFile, "A: %d B: %d X: %d Y: %d RR: %d ZR: %d SB: %d SL: %d SR: %d P: %d H: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
			//		jc->btns.a, jc->btns.b, jc->btns.x, jc->btns.y, jc->btns.r, jc->btns.zr, jc->btns.stick_button, jc->btns.sl, jc->btns.sr, \
			//		jc->btns.plus, jc->btns.home, (jc->stick.CalX + 1), (jc->stick.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);
			//}
		}

		// pro controller:
		if (jc->left_right == 3) {

			// left:
			jc->btns.down = (jc->buttons & (1 << 0)) ? 1 : 0;
			jc->btns.up = (jc->buttons & (1 << 1)) ? 1 : 0;
			jc->btns.right = (jc->buttons & (1 << 2)) ? 1 : 0;
			jc->btns.left = (jc->buttons & (1 << 3)) ? 1 : 0;
			jc->btns.sr = (jc->buttons & (1 << 4)) ? 1 : 0;
			jc->btns.sl = (jc->buttons & (1 << 5)) ? 1 : 0;
			jc->btns.l = (jc->buttons & (1 << 6)) ? 1 : 0;
			jc->btns.zl = (jc->buttons & (1 << 7)) ? 1 : 0;
			jc->btns.minus = (jc->buttons & (1 << 8)) ? 1 : 0;
			jc->btns.stick_button = (jc->buttons & (1 << 11)) ? 1 : 0;
			jc->btns.capture = (jc->buttons & (1 << 13)) ? 1 : 0;

			// right:
			jc->btns.y = (jc->buttons2 & (1 << 0)) ? 1 : 0;
			jc->btns.x = (jc->buttons2 & (1 << 1)) ? 1 : 0;
			jc->btns.b = (jc->buttons2 & (1 << 2)) ? 1 : 0;
			jc->btns.a = (jc->buttons2 & (1 << 3)) ? 1 : 0;
			jc->btns.sr = (jc->buttons2 & (1 << 4)) ? 1 : 0;
			jc->btns.sl = (jc->buttons2 & (1 << 5)) ? 1 : 0;
			jc->btns.r = (jc->buttons2 & (1 << 6)) ? 1 : 0;
			jc->btns.zr = (jc->buttons2 & (1 << 7)) ? 1 : 0;
			jc->btns.plus = (jc->buttons2 & (1 << 9)) ? 1 : 0;
			jc->btns.stick_button2 = (jc->buttons2 & (1 << 10)) ? 1 : 0;
			jc->btns.home = (jc->buttons2 & (1 << 12)) ? 1 : 0;


			if (/*settings.debugMode*/false) {

				printf("U: %d D: %d L: %d R: %d LL: %d ZL: %d SB: %d SL: %d SR: %d M: %d C: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
					jc->btns.up, jc->btns.down, jc->btns.left, jc->btns.right, jc->btns.l, jc->btns.zl, jc->btns.stick_button, jc->btns.sl, jc->btns.sr, \
					jc->btns.minus, jc->btns.capture, (jc->stick.CalX + 1), (jc->stick.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);

				printf("A: %d B: %d X: %d Y: %d RR: %d ZR: %d SB: %d SL: %d SR: %d P: %d H: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
					jc->btns.a, jc->btns.b, jc->btns.x, jc->btns.y, jc->btns.r, jc->btns.zr, jc->btns.stick_button2, jc->btns.sl, jc->btns.sr, \
					jc->btns.plus, jc->btns.home, (jc->stick2.CalX + 1), (jc->stick2.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);
			}

			//if (settings.writeDebugToFile) {
			//	fprintf(settings.outputFile, "U: %d D: %d L: %d R: %d LL: %d ZL: %d SB: %d SL: %d SR: %d M: %d C: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
			//		jc->btns.up, jc->btns.down, jc->btns.left, jc->btns.right, jc->btns.l, jc->btns.zl, jc->btns.stick_button, jc->btns.sl, jc->btns.sr, \
			//		jc->btns.minus, jc->btns.capture, (jc->stick.CalX + 1), (jc->stick.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);

			//	fprintf(settings.outputFile, "A: %d B: %d X: %d Y: %d RR: %d ZR: %d SB: %d SL: %d SR: %d P: %d H: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
			//		jc->btns.a, jc->btns.b, jc->btns.x, jc->btns.y, jc->btns.r, jc->btns.zr, jc->btns.stick_button2, jc->btns.sl, jc->btns.sr, \
			//		jc->btns.plus, jc->btns.home, (jc->stick2.CalX + 1), (jc->stick2.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);
			//}

		}

	}
}




void pollLoop() {

	// poll joycons:
	for (int i = 0; i < joycons.size(); ++i) {

		Joycon *jc = &joycons[i];

		if (!jc->handle) { continue; }

		if (/*settings.forcePollUpdate*/false) {
			// set to be blocking:
			hid_set_nonblocking(jc->handle, 0);
		} else {
			// set to be non-blocking:
			hid_set_nonblocking(jc->handle, 1);
		}

		// get input:
		memset(buf, 0, 65);

		// get current time
		std::chrono::steady_clock::time_point tNow = std::chrono::high_resolution_clock::now();

		auto timeSincePoll = std::chrono::duration_cast<std::chrono::microseconds>(tNow - tracker.tPolls[i]);

		// time spent sleeping (0):
		double timeSincePollMS = timeSincePoll.count() / 1000.0;

		if (timeSincePollMS > (1000.0 / /*settings.pollsPerSec*/60.0f)) {
			jc->send_command(0x1E, buf, 0);
			tracker.tPolls[i] = std::chrono::high_resolution_clock::now();
		}


		hid_read(jc->handle, buf, 0x40);

		// get rid of queue:
		// if we force the poll to wait then the queue will never clear and will just freeze:
		if (/*!settings.forcePollUpdate*/true) {
			while (hid_read(jc->handle, buf, 0x40) > 0) {};
		}

		handle_input(jc, buf, 0x40);
	}

	// DO STUFF WITH JOYCONS HERE:

	// get first connected joycon:
	Joycon *jc = &joycons[0];

	// left joycon:
					printf("U: %d D: %d L: %d R: %d LL: %d ZL: %d SB: %d SL: %d SR: %d M: %d C: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
					jc->btns.up, jc->btns.down, jc->btns.left, jc->btns.right, jc->btns.l, jc->btns.zl, jc->btns.stick_button, jc->btns.sl, jc->btns.sr, \
					jc->btns.minus, jc->btns.capture, (jc->stick.CalX + 1), (jc->stick.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);

	// right joycon:
	//				printf("A: %d B: %d X: %d Y: %d RR: %d ZR: %d SB: %d SL: %d SR: %d P: %d H: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
					jc->btns.a, jc->btns.b, jc->btns.x, jc->btns.y, jc->btns.r, jc->btns.zr, jc->btns.stick_button, jc->btns.sl, jc->btns.sr, \
					jc->btns.plus, jc->btns.home, (jc->stick.CalX + 1), (jc->stick.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);

	// sleep:
	accurateSleep(2.0);// 8.00
}

void start() {

	int read;	// number of bytes read
	int written;// number of bytes written
	const char *device_name;

	// Enumerate and print the HID devices on the system
	struct hid_device_info *devs, *cur_dev;

	res = hid_init();

	// hack:
	for (int i = 0; i < 100; ++i) {
		tracker.tPolls.push_back(std::chrono::high_resolution_clock::now());
	}


	if (/*settings.writeDebugToFile*/false) {

		// find a debug file to output to:
		int fileNumber = 0;
		std::string name = std::string("output-") + std::to_string(fileNumber) + std::string(".txt");
		while (exists_test0(name)) {
			fileNumber += 1;
			name = std::string("output-") + std::to_string(fileNumber) + std::string(".txt");
		}

		//settings.outputFile = fopen(name.c_str(), "w");
	}


init_start:

	devs = hid_enumerate(JOYCON_VENDOR, 0x0);
	cur_dev = devs;
	while (cur_dev) {

		// identify by vendor:
		if (cur_dev->vendor_id == JOYCON_VENDOR) {

			// bluetooth, left / right joycon:
			if (cur_dev->product_id == JOYCON_L_BT || cur_dev->product_id == JOYCON_R_BT) {
				Joycon jc = Joycon(cur_dev);
				joycons.push_back(jc);
			}

			// pro controller:
			if (cur_dev->product_id == PRO_CONTROLLER) {
				Joycon jc = Joycon(cur_dev);
				joycons.push_back(jc);
			}

			// charging grip:
			//if (cur_dev->product_id == JOYCON_CHARGING_GRIP) {
			//	Joycon jc = Joycon(cur_dev);
			//	settings.usingBluetooth = false;
			//	settings.combineJoyCons = true;
			//	joycons.push_back(jc);
			//}

		}


		cur_dev = cur_dev->next;
	}
	hid_free_enumeration(devs);



	// init joycons:
	if (/*settings.usingGrip*/false) {
		for (int i = 0; i < joycons.size(); ++i) {
			joycons[i].init_usb();
		}
	} else {
		for (int i = 0; i < joycons.size(); ++i) {
			joycons[i].init_bt();
		}
	}

	// initial poll to get battery data:
	pollLoop();
	for (int i = 0; i < joycons.size(); ++i) {
		printf("battery level: %u\n", joycons[i].battery);
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
				if (/*settings.combineJoyCons*/true) {
					buf[0] = 0x0 | 0x0 | 0x0 | 0x1; // solid 1
				} else if (/*!settings.combineJoyCons*/false) {
					buf[0] = 0x0 | 0x0 | 0x2 | 0x0; // solid 2
				}
			}
			//buf[0] = 0x80 | 0x40 | 0x2 | 0x1; // Flash top two, solid bottom two
			//buf[0] = 0x8 | 0x4 | 0x2 | 0x1; // All solid
			//buf[0] = 0x80 | 0x40 | 0x20 | 0x10; // All flashing
			//buf[0] = 0x80 | 0x00 | 0x20 | 0x10; // All flashing except 3rd light (off)
			jc->send_subcommand(0x01, 0x30, buf, 1);
		}
	}


	// give a small rumble to all joycons:
	printf("vibrating JoyCon(s).\n");
	for (int k = 0; k < 1; ++k) {
		for (int i = 0; i < joycons.size(); ++i) {
			joycons[i].rumble(100, 1);
			Sleep(20);
			joycons[i].rumble(10, 3);
		}
	}

	printf("Done.\n");
}




//int main(int argc, char *argv[]) {
int wWinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPWSTR cmdLine, int cmdShow) {

	setupConsole("Debug");

	start();

	while (true) {
		pollLoop();
	}

	return 0;
}