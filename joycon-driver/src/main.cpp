
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
#include <wx/wx.h>
#include <wx/glcanvas.h>
#include <cube.h>
#include <MyApp.h>

// glm:
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

// curl:
#include <curl/curl.h>

#if defined(_WIN32)
#include <Windows.h>
#include <Lmcons.h>
#include <shlobj.h>
#endif

// sio:
#include "sio/sio_client.h"



#pragma warning(disable:4996)

#define JOYCON_VENDOR 0x057e
#define JOYCON_L_BT 0x2006
#define JOYCON_R_BT 0x2007
#define PRO_CONTROLLER 0x2009
#define JOYCON_CHARGING_GRIP 0x200e
#define SERIAL_LEN 18
#define PI 3.14159265359
#define L_OR_R(lr) (lr == 1 ? 'L' : (lr == 2 ? 'R' : '?'))


std::vector<Joycon> joycons;
MouseController MC;
JOYSTICK_POSITION_V2 iReport; // The structure that holds the full position data
unsigned char buf[65];
int res = 0;

// sio:
sio::client myClient;


// this is awful, don't do this:
wxStaticText* gyroComboCodeText;
void setGyroComboCodeText(int code);

wxCheckBox* gyroCheckBox;


struct Settings {

	// Enabling this combines both JoyCons to a single vJoy Device(#1)
	// when combineJoyCons == false:
	// JoyCon(L) is mapped to vJoy Device #1
	// JoyCon(R) is mapped to vJoy Device #2
	// when combineJoyCons == true:
	// JoyCon(L) and JoyCon(R) are mapped to vJoy Device #1
	bool combineJoyCons = false;

	bool reverseX = false;// reverses joystick x (both sticks)
	bool reverseY = false;// reverses joystick y (both sticks)

	bool usingGrip = false;
	bool usingBluetooth = true;
	bool disconnect = false;

	// enables motion controls
	bool enableGyro = false;

	// gyroscope (mouse) sensitivity:
	float gyroSensitivityX = 150.0f;
	float gyroSensitivityY = 150.0f;


	// prefer the left joycon for gyro controls
	bool preferLeftJoyCon = false;

	// combo code to set key combination to disable gyroscope for quick turning in games. -1 to disable.
	int gyroscopeComboCode = 4;

	// toggles between two different toggle types
	// disabled = traditional toggle
	// enabled = while button(s) are held gyro is enabled
	bool quickToggleGyro = false;

	// inverts the above function
	bool invertQuickToggle = false;

	// for dolphin, mostly:
	bool dolphinPointerMode = false;

	// so that you don't rapidly toggle the gyro controls every frame:
	bool canToggleGyro = true;


	// enables 3D gyroscope visualizer
	bool gyroWindow = false;

	// plays a version of the mario theme by vibrating
	// the first JoyCon connected.
	bool marioTheme = false;

	// bool to restart the program
	bool restart = false;

	// debug mode
	bool debugMode = false;

	// write debug to file:
	bool writeDebugToFile = false;

	// debug file:
	FILE* outputFile;

	// broadcast mode:
	bool broadcastMode = false;
	// where to connect:
	std::string host = "";
	// string to send:
	std::string controllerState = "";
	// write cast to file:
	bool writeCastToFile = false;

	// poll options:

	// force joycon to update when polled:
	bool forcePollUpdate = false;

	// times to poll per second per joycon:
	float pollsPerSec = 30.0f;

	// time to sleep (in ms) between polls:
	float timeToSleepMS = 2.0f;

	// version number
	std::string version = "1.05";

} settings;


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

	// get current time
	//std::chrono::high_resolution_clock tNow;
	//std::chrono::steady_clock::time_point tPoll = std::chrono::high_resolution_clock::now();
	std::vector<std::chrono::steady_clock::time_point> tPolls;
	//Tracker(int value) : tPolls(100, std::chrono::high_resolution_clock::now()) {}
	//auto tSleepStart = std::chrono::high_resolution_clock::now();

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
			jc->gyro.roll	= (float)((uint16_to_int16(packet[19] | (packet[20] << 8) & 0xFF00)) - jc->sensor_cal[1][0]) * jc->gyro_cal_coeff[0];

			// get pitch:
			jc->gyro.pitch	= (float)((uint16_to_int16(packet[21] | (packet[22] << 8) & 0xFF00)) - jc->sensor_cal[1][1]) * jc->gyro_cal_coeff[1];

			// get yaw:
			jc->gyro.yaw	= (float)((uint16_to_int16(packet[23] | (packet[24] << 8) & 0xFF00)) - jc->sensor_cal[1][2]) * jc->gyro_cal_coeff[2];
		}

		// offsets:
		{
			jc->setGyroOffsets();

			jc->gyro.roll	-= jc->gyro.offset.roll;
			jc->gyro.pitch	-= jc->gyro.offset.pitch;
			jc->gyro.yaw	-= jc->gyro.offset.yaw;

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

			
			if (settings.debugMode) {
				printf("U: %d D: %d L: %d R: %d LL: %d ZL: %d SB: %d SL: %d SR: %d M: %d C: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
					jc->btns.up, jc->btns.down, jc->btns.left, jc->btns.right, jc->btns.l, jc->btns.zl, jc->btns.stick_button, jc->btns.sl, jc->btns.sr, \
					jc->btns.minus, jc->btns.capture, (jc->stick.CalX + 1), (jc->stick.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);
			}
			if (settings.writeDebugToFile) {
				fprintf(settings.outputFile, "U: %d D: %d L: %d R: %d LL: %d ZL: %d SB: %d SL: %d SR: %d M: %d C: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
					jc->btns.up, jc->btns.down, jc->btns.left, jc->btns.right, jc->btns.l, jc->btns.zl, jc->btns.stick_button, jc->btns.sl, jc->btns.sr, \
					jc->btns.minus, jc->btns.capture, (jc->stick.CalX + 1), (jc->stick.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);
			}
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


			if (settings.debugMode) {
				printf("A: %d B: %d X: %d Y: %d RR: %d ZR: %d SB: %d SL: %d SR: %d P: %d H: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
					jc->btns.a, jc->btns.b, jc->btns.x, jc->btns.y, jc->btns.r, jc->btns.zr, jc->btns.stick_button, jc->btns.sl, jc->btns.sr, \
					jc->btns.plus, jc->btns.home, (jc->stick.CalX + 1), (jc->stick.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);
			}
			if (settings.writeDebugToFile) {
				fprintf(settings.outputFile, "A: %d B: %d X: %d Y: %d RR: %d ZR: %d SB: %d SL: %d SR: %d P: %d H: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
					jc->btns.a, jc->btns.b, jc->btns.x, jc->btns.y, jc->btns.r, jc->btns.zr, jc->btns.stick_button, jc->btns.sl, jc->btns.sr, \
					jc->btns.plus, jc->btns.home, (jc->stick.CalX + 1), (jc->stick.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);
			}
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


			if (settings.debugMode) {

				printf("U: %d D: %d L: %d R: %d LL: %d ZL: %d SB: %d SL: %d SR: %d M: %d C: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
						jc->btns.up, jc->btns.down, jc->btns.left, jc->btns.right, jc->btns.l, jc->btns.zl, jc->btns.stick_button, jc->btns.sl, jc->btns.sr, \
						jc->btns.minus, jc->btns.capture, (jc->stick.CalX + 1), (jc->stick.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);

				printf("A: %d B: %d X: %d Y: %d RR: %d ZR: %d SB: %d SL: %d SR: %d P: %d H: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
						jc->btns.a, jc->btns.b, jc->btns.x, jc->btns.y, jc->btns.r, jc->btns.zr, jc->btns.stick_button2, jc->btns.sl, jc->btns.sr, \
						jc->btns.plus, jc->btns.home, (jc->stick2.CalX + 1), (jc->stick2.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);
			}

			if (settings.writeDebugToFile) {
				fprintf(settings.outputFile, "U: %d D: %d L: %d R: %d LL: %d ZL: %d SB: %d SL: %d SR: %d M: %d C: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
					jc->btns.up, jc->btns.down, jc->btns.left, jc->btns.right, jc->btns.l, jc->btns.zl, jc->btns.stick_button, jc->btns.sl, jc->btns.sr, \
					jc->btns.minus, jc->btns.capture, (jc->stick.CalX + 1), (jc->stick.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);

				fprintf(settings.outputFile, "A: %d B: %d X: %d Y: %d RR: %d ZR: %d SB: %d SL: %d SR: %d P: %d H: %d SX: %.5f SY: %.5f GR: %06d GP: %06d GY: %06d\n", \
					jc->btns.a, jc->btns.b, jc->btns.x, jc->btns.y, jc->btns.r, jc->btns.zr, jc->btns.stick_button2, jc->btns.sl, jc->btns.sr, \
					jc->btns.plus, jc->btns.home, (jc->stick2.CalX + 1), (jc->stick2.CalY + 1), (int)jc->gyro.roll, (int)jc->gyro.pitch, (int)jc->gyro.yaw);
			}

		}

	}
}





int acquirevJoyDevice(int deviceID) {

	int stat;

	// Get the driver attributes (Vendor ID, Product ID, Version Number)
	if (!vJoyEnabled()) {
		printf("Function vJoyEnabled Failed - make sure that vJoy is installed and enabled\n");
		int dummy = getchar();
		stat = -2;
		throw;
	} else {
		//wprintf(L"Vendor: %s\nProduct :%s\nVersion Number:%s\n", static_cast<TCHAR *> (GetvJoyManufacturerString()), static_cast<TCHAR *>(GetvJoyProductString()), static_cast<TCHAR *>(GetvJoySerialNumberString()));
		//wprintf(L"Product :%s\n", static_cast<TCHAR *>(GetvJoyProductString()));
	};

	// Get the status of the vJoy device before trying to acquire it
	VjdStat status = GetVJDStatus(deviceID);

	switch (status) {
		case VJD_STAT_OWN:
			printf("vJoy device %d is already owned by this feeder\n", deviceID);
			break;
		case VJD_STAT_FREE:
			printf("vJoy device %d is free\n", deviceID);
			break;
		case VJD_STAT_BUSY:
			printf("vJoy device %d is already owned by another feeder\nCannot continue\n", deviceID);
			return -3;
		case VJD_STAT_MISS:
			printf("vJoy device %d is not installed or disabled\nCannot continue\n", deviceID);
			return -4;
		default:
			printf("vJoy device %d general error\nCannot continue\n", deviceID);
			return -1;
	};

	// Acquire the vJoy device
	if (!AcquireVJD(deviceID)) {
		printf("Failed to acquire vJoy device number %d.\n", deviceID);
		int dummy = getchar();
		stat = -1;
		throw;
	} else {
		printf("Acquired device number %d - OK\n", deviceID);
	}
}


void updatevJoyDevice2(Joycon *jc) {

	UINT DevID;

	PVOID pPositionMessage;
	UINT	IoCode = LOAD_POSITIONS;
	UINT	IoSize = sizeof(JOYSTICK_POSITION);
	// HID_DEVICE_ATTRIBUTES attrib;
	BYTE id = 1;
	UINT iInterface = 1;

	// Set destination vJoy device
	DevID = jc->vJoyNumber;
	id = (BYTE)DevID;
	iReport.bDevice = id;

	if (DevID == 0 && settings.debugMode) {
		printf("something went very wrong D:\n");
	}

	// Set Stick data

	int x = 0;
	int y = 0;
	//int z = 0;
	int z = 16384;
	int rx = 0;
	int ry = 0;
	int rz = 0;

	if (jc->deviceNumber == 0) {
		x = 16384 * (jc->stick.CalX);
		y = 16384 * (jc->stick.CalY);
	} else if (jc->deviceNumber == 1) {
		rx = 16384 * (jc->stick.CalX);
		ry = 16384 * (jc->stick.CalY);
	}
	// pro controller:
	if (jc->left_right == 3) {
		x = 16384 * (jc->stick.CalX);
		y = 16384 * (jc->stick.CalY);
		rx = 16384 * (jc->stick2.CalX);
		ry = 16384 * (jc->stick2.CalY);
	}


	x += 16384;
	y += 16384;
	rx += 16384;
	ry += 16384;
	//rz += 16384;

	if (settings.reverseX) {
		x = 32768 - x;
		rx = 32768 - rx;
	}
	if (settings.reverseY) {
		y = 32768 - y;
		ry = 32768 - ry;
	}


	if (jc->deviceNumber == 0) {
		iReport.wAxisX = x;
		iReport.wAxisY = y;
	} else if (jc->deviceNumber == 1) {
		iReport.wAxisXRot = rx;
		iReport.wAxisYRot = ry;
	}
	// pro controller:
	if (jc->left_right == 3) {
		// both sticks:
		iReport.wAxisX = x;
		iReport.wAxisY = y;
		iReport.wAxisXRot = rx;
		iReport.wAxisYRot = ry;
	}

	iReport.wAxisZ = z;// update z with 16384


	// prefer left joycon for gyroscope controls:
	int a = -1;
	int b = -1;
	if (settings.preferLeftJoyCon) {
		a = 1;
		b = 2;
	} else {
		a = 2;
		b = 1;
	}

	bool gyroComboCodePressed = false;



	// gyro / accelerometer data:
	if ((((jc->left_right == a) || (joycons.size() == 1 && jc->left_right == b) || (jc->left_right == 3)) && settings.combineJoyCons) || !settings.combineJoyCons) {

		int multiplier;


		// Gyroscope (roll, pitch, yaw):
		multiplier = 1000;




		// complementary filtered tracking
		// uses gyro + accelerometer

		// set to 0:
		tracker.quat = glm::angleAxis(0.0f, glm::vec3(1.0, 0.0, 0.0));

		float gyroCoeff = 0.001;


		// x:
		float pitchDegreesAccel = glm::degrees((atan2(-jc->accel.x, -jc->accel.z) + PI));
		float pitchDegreesGyro = -jc->gyro.pitch * gyroCoeff;
		float pitch = 0;

		tracker.anglex += pitchDegreesGyro;
		if ((pitchDegreesAccel - tracker.anglex) > 180) {
			tracker.anglex += 360;
		} else if ((tracker.anglex - pitchDegreesAccel) > 180) {
			tracker.anglex -= 360;
		}
		tracker.anglex = (tracker.anglex * 0.98) + (pitchDegreesAccel * 0.02);
		pitch = tracker.anglex;

		glm::fquat delx = glm::angleAxis(glm::radians(pitch), glm::vec3(1.0, 0.0, 0.0));
		tracker.quat = tracker.quat*delx;

		// y:
		float rollDegreesAccel = -glm::degrees((atan2(-jc->accel.y, -jc->accel.z) + PI));
		float rollDegreesGyro = -jc->gyro.roll * gyroCoeff;
		float roll = 0;

		tracker.angley += rollDegreesGyro;
		if ((rollDegreesAccel - tracker.angley) > 180) {
			tracker.angley += 360;
		} else if ((tracker.angley - rollDegreesAccel) > 180) {
			tracker.angley -= 360;
		}
		tracker.angley = (tracker.angley * 0.98) + (rollDegreesAccel * 0.02);
		//tracker.angley = -rollInDegreesAccel;
		roll = tracker.angley;


		glm::fquat dely = glm::angleAxis(glm::radians(roll), glm::vec3(0.0, 0.0, 1.0));
		tracker.quat = tracker.quat*dely;

		//printf("%f\n", roll);


		// z:
		float yawDegreesAccel = glm::degrees((atan2(-jc->accel.y, -jc->accel.x) + PI));
		float yawDegreesGyro = -jc->gyro.yaw * gyroCoeff;
		float yaw = 0;

		tracker.anglez += lowpassFilter(yawDegreesGyro, 0.5);
		//if ((yawInDegreesAccel - tracker.anglez) > 180) {
		//	tracker.anglez += 360;
		//} else if ((tracker.anglez - yawDegreesAccel) > 180) {
		//	tracker.anglez -= 360;
		//}
		//tracker.anglez = (tracker.anglez * 0.98) + (yawDegreesAccel * 0.02);
		yaw = tracker.anglez;


		glm::fquat delz = glm::angleAxis(glm::radians(-yaw), glm::vec3(0.0, 1.0, 0.0));
		tracker.quat = tracker.quat*delz;





		float relX2 = -jc->gyro.yaw * settings.gyroSensitivityX;
		float relY2 = jc->gyro.pitch * settings.gyroSensitivityY;

		relX2 /= 10;
		relY2 /= 10;

		//printf("%.3f %.3f %.3f\n", abs(jc->gyro.roll), abs(jc->gyro.pitch), abs(jc->gyro.yaw));
		//printf("%.2f %.2f\n", relX2, relY2);

		// check if combo keys are pressed:
		int comboCodeButtons = -1;
		if (jc->left_right != 3) {
			comboCodeButtons = jc->buttons;
		} else {
			comboCodeButtons = ((uint32_t)jc->buttons2 << 16) | jc->buttons;
		}
		
		setGyroComboCodeText(comboCodeButtons);
		if (comboCodeButtons == settings.gyroscopeComboCode) {
			gyroComboCodePressed = true;
		} else {
			gyroComboCodePressed = false;
		}

		if (!gyroComboCodePressed) {
			settings.canToggleGyro = true;
		}

		if (settings.canToggleGyro && gyroComboCodePressed && !settings.quickToggleGyro) {
			settings.enableGyro = !settings.enableGyro;
			gyroCheckBox->SetValue(settings.enableGyro);
			settings.canToggleGyro = false;
		}

		if (jc->left_right == 2) {
			relX2 *= -1;
			relY2 *= -1;
		}

		bool gyroActuallyOn = false;
		
		if (settings.enableGyro && settings.quickToggleGyro) {
			// check if combo keys are pressed:
			if (settings.invertQuickToggle) {
				if (!gyroComboCodePressed) {
					gyroActuallyOn = true;
				}
			} else {
				if (gyroComboCodePressed) {
					gyroActuallyOn = true;
				}
			}
		}

		if (settings.enableGyro && !settings.quickToggleGyro) {
			gyroActuallyOn = true;
		}

		float mult = settings.gyroSensitivityX * 10.0f;


		if (gyroActuallyOn) {
			MC.moveRel3(relX2, relY2);
		}

		if (settings.dolphinPointerMode) {
			iReport.wAxisZRot += (jc->gyro.roll * mult);
			iReport.wSlider += (jc->gyro.pitch * mult);
			iReport.wDial += (jc->gyro.yaw * mult);

			iReport.wAxisZRot = clamp(iReport.wAxisZRot, 0, 32678);
			iReport.wSlider = clamp(iReport.wSlider, 0, 32678);
			iReport.wDial = clamp(iReport.wDial, 0, 32678);
		} else {
			iReport.wAxisZRot = 16384 + (jc->gyro.roll * mult);
			iReport.wSlider = 16384 + (jc->gyro.pitch * mult);
			iReport.wDial = 16384 + (jc->gyro.yaw * mult);
		}
	}

	// Set button data
	// JoyCon(L) is the first 16 bits
	// JoyCon(R) is the last 16 bits

	long btns = 0;
	if (!settings.combineJoyCons) {
		btns = jc->buttons;
	} else {
		if (jc->left_right == 1) {
			// don't overwrite the other joycon
			btns = ((iReport.lButtons >> 16) << 16) | (jc->buttons);
		} else if (jc->left_right == 2) {
			btns = ((jc->buttons) << 16) | (createMask(0, 15) & iReport.lButtons);
		}
	}

	// Pro Controller:
	if (jc->left_right == 3) {
		uint32_t combined = ((uint32_t)jc->buttons2 << 16) | jc->buttons;
		btns = combined;
		//std::bitset<16> num1(jc->buttons);
		//std::bitset<16> num2(jc->buttons2);
		//std::cout << num1 << " " << num2 << std::endl;
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





void parseSettings2() {

	//setupConsole("Debug");

	std::map<std::string, std::string> cfg = LoadConfig("config.txt");

	settings.combineJoyCons = (bool)stoi(cfg["combineJoyCons"]);
	settings.enableGyro = (bool)stoi(cfg["gyroControls"]);

	settings.gyroSensitivityX = stof(cfg["gyroSensitivityX"]);
	settings.gyroSensitivityY = stof(cfg["gyroSensitivityY"]);

	settings.gyroWindow = (bool)stoi(cfg["gyroWindow"]);
	settings.marioTheme = (bool)stoi(cfg["marioTheme"]);

	settings.reverseX = (bool)stoi(cfg["reverseX"]);
	settings.reverseY = (bool)stoi(cfg["reverseY"]);

	settings.preferLeftJoyCon = (bool)stoi(cfg["preferLeftJoyCon"]);
	settings.quickToggleGyro = (bool)stoi(cfg["quickToggleGyro"]);
	settings.invertQuickToggle = (bool)stoi(cfg["invertQuickToggle"]);

	settings.dolphinPointerMode = (bool)stoi(cfg["dolphinPointerMode"]);

	settings.gyroscopeComboCode = stoi(cfg["gyroscopeComboCode"]);

	settings.debugMode = (bool)stoi(cfg["debugMode"]);
	settings.writeDebugToFile = (bool)stoi(cfg["writeDebugToFile"]);

	settings.forcePollUpdate = (bool)stoi(cfg["forcePollUpdate"]);

	settings.broadcastMode = (bool)stoi(cfg["broadcastMode"]);
	settings.host = cfg["host"];
	settings.writeCastToFile = (bool)stoi(cfg["writeCastToFile"]);

}

void start();

void pollLoop() {

	// poll joycons:
	for (int i = 0; i < joycons.size(); ++i) {
		
		Joycon *jc = &joycons[i];

		// choose a random joycon to reduce bias / figure out the problem w/input lag:
		//Joycon *jc = &joycons[rand_range(0, joycons.size()-1)];
		
		if (!jc->handle) { continue; }


		if (settings.forcePollUpdate) {
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
		
		if (timeSincePollMS > (1000.0/settings.pollsPerSec)) {
			jc->send_command(0x1E, buf, 0);
			tracker.tPolls[i] = std::chrono::high_resolution_clock::now();
		}


		//hid_read(jc->handle, buf, 0x40);
		hid_read_timeout(jc->handle, buf, 0x40, 20);

		// get rid of queue:
		// if we force the poll to wait then the queue will never clear and will just freeze:
		//if (!settings.forcePollUpdate) {
		//	while (hid_read(jc->handle, buf, 0x40) > 0) {};
		//}

		//for (int i = 0; i < 100; ++i) {
		//	hid_read(jc->handle, buf, 0x40);
		//}

		handle_input(jc, buf, 0x40);
	}

	// update vjoy:
	for (int i = 0; i < joycons.size(); ++i) {
		updatevJoyDevice2(&joycons[i]);
	}

	// sleep:
	accurateSleep(settings.timeToSleepMS);


	if (settings.broadcastMode && joycons.size() > 0) {
		Joycon *jc = &joycons[0];
		std::string newControllerState = "";
		
		
		if (jc->btns.up == 1 && jc->btns.left == 1) {
			newControllerState += "7";
		} else if (jc->btns.up && jc->btns.right == 1) {
			newControllerState += "1";
		} else if (jc->btns.down == 1 && jc->btns.left == 1) {
			newControllerState += "5";
		} else if (jc->btns.down == 1 && jc->btns.right == 1) {
			newControllerState += "3";
		} else if (jc->btns.up == 1) {
			newControllerState += "0";
		} else if (jc->btns.down == 1) {
			newControllerState += "4";
		} else if (jc->btns.left == 1) {
			newControllerState += "6";
		} else if (jc->btns.right == 1) {
			newControllerState += "2";
		} else {
			newControllerState += "8";
		}

		newControllerState += jc->btns.stick_button == 1 ? "1" : "0";
		newControllerState += jc->btns.l == 1 ? "1" : "0";
		newControllerState += jc->btns.zl == 1 ? "1" : "0";
		newControllerState += jc->btns.minus == 1 ? "1" : "0";
		newControllerState += jc->btns.capture == 1 ? "1" : "0";

		newControllerState += jc->btns.a == 1 ? "1" : "0";
		newControllerState += jc->btns.b == 1 ? "1" : "0";
		newControllerState += jc->btns.x == 1 ? "1" : "0";
		newControllerState += jc->btns.y == 1 ? "1" : "0";
		newControllerState += jc->btns.stick_button2 == 1 ? "1" : "0";
		newControllerState += jc->btns.r == 1 ? "1" : "0";
		newControllerState += jc->btns.zr == 1 ? "1" : "0";
		newControllerState += jc->btns.plus == 1 ? "1" : "0";
		newControllerState += jc->btns.home == 1 ? "1" : "0";

		int LX = ((jc->stick.CalX - 1.0) * 128) + 255;
		int LY = ((jc->stick.CalY - 1.0) * 128) + 255;
		int RX = ((jc->stick2.CalX - 1.0) * 128) + 255;
		int RY = ((jc->stick2.CalY - 1.0) * 128) + 255;

		newControllerState += " " + std::to_string(LX) + " " + std::to_string(LY) + " " + std::to_string(RX) + " " + std::to_string(RY);

		if (newControllerState != settings.controllerState) {
			settings.controllerState = newControllerState;
			printf("%s\n", newControllerState);
			myClient.socket()->emit("sendControllerState", newControllerState);
		}
		if (settings.writeCastToFile) {
			fprintf(settings.outputFile, "%s\n", newControllerState);
		}
	}

	if (settings.restart) {
		settings.restart = false;
		start();
	}
}



void start() {




	// set infinite reconnect attempts
	myClient.set_reconnect_attempts(999999999999);
	if (settings.broadcastMode) {
		myClient.connect(settings.host);
	}



	// get vJoy Device 1-8
	for (int i = 1; i < 9; ++i) {
		acquirevJoyDevice(i);
	}

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


	if (settings.writeDebugToFile || settings.writeCastToFile) {

		// find a debug file to output to:
		int fileNumber = 0;
		std::string name = std::string("output-") + std::to_string(fileNumber) + std::string(".txt");
		while (exists_test0(name)) {
			fileNumber += 1;
			name = std::string("output-") + std::to_string(fileNumber) + std::string(".txt");
		}

		settings.outputFile = fopen(name.c_str(), "w");
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
	if (settings.usingGrip) {
		for (int i = 0; i < joycons.size(); ++i) {
			joycons[i].init_usb();
		}
	} else {
		for (int i = 0; i < joycons.size(); ++i) {
			joycons[i].init_bt();
		}
	}

	if (settings.combineJoyCons) {
		int counter = 0;
		for (int i = 0; i < joycons.size(); ++i) {
			joycons[i].vJoyNumber = (counter/2)+1;
			joycons[i].deviceNumber = (counter % 2 ? 1 : 0);
			counter++;
		}
	} else {
		for (int i = 0; i < joycons.size(); ++i) {
			joycons[i].vJoyNumber = i+1;
			joycons[i].deviceNumber = 0;// left
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

			Joycon joycon = joycons[0];

			Sleep(1000);

			joycon.rumble(mk_odd(E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// E2
			Sleep(100 / spd2);
			joycon.rumble(mk_odd(E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// E2
			Sleep(100 / spd2);
			joycon.rumble(mk_odd(E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// E2
			Sleep(100 / spd2);
			joycon.rumble(mk_odd(C4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// E2
			Sleep(100 / spd2);
			joycon.rumble(mk_odd(G4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// G2
			Sleep(400 / spd2);

			joycon.rumble(mk_odd(A4), 1); Sleep(400 / spd); joycon.rumble(1, 3);	// too low for joycon
			Sleep(50 / spd2);

			joycon.rumble(mk_odd(C4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C2
			Sleep(200 / spd2);
			joycon.rumble(mk_odd(G3), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// G1
			Sleep(200 / spd2);
			joycon.rumble(mk_odd(E3), 2); Sleep(200 / spd); joycon.rumble(1, 3);	// E1
			Sleep(200 / spd2);
			joycon.rumble(mk_odd(A4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// A2

			Sleep(100 / spd2);
			joycon.rumble(mk_odd(B4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// B2

			Sleep(50 / spd2);
			joycon.rumble(mk_odd(A4B4), 1); Sleep(200 / spd); joycon.rumble(1, 3);// A2-B2?
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(A4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// A2
			Sleep(100 / spd2);
			joycon.rumble(mk_odd(G3), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// G1


			Sleep(100 / spd2);
			joycon.rumble(mk_odd(E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// E2
			Sleep(100 / spd2);
			joycon.rumble(mk_odd(G4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// G2
			Sleep(100 / spd2);
			joycon.rumble(mk_odd(A5), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// A3



			Sleep(200 / spd2);
			joycon.rumble(mk_odd(F4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// F2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(G4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// G2

			Sleep(200 / spd2);
			joycon.rumble(mk_odd(E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// E2

			Sleep(50 / spd2);
			joycon.rumble(mk_odd(C4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(D4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// D2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(B4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// B2


			Sleep(200 / spd2);
			joycon.rumble(mk_odd(C4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C2
			Sleep(200 / spd2);
			joycon.rumble(mk_odd(G3), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// G1
			Sleep(200 / spd2);
			joycon.rumble(mk_odd(E3), 2); Sleep(200 / spd); joycon.rumble(1, 3);	// E1

			Sleep(200 / spd2);
			joycon.rumble(mk_odd(A4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// A2
			Sleep(200 / spd2);
			joycon.rumble(mk_odd(B4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// B2
			Sleep(200 / spd2);
			joycon.rumble(mk_odd(A4B4), 1); Sleep(200 / spd); joycon.rumble(1, 3);// A2-B2?
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(A4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// A2
			Sleep(200 / spd2);
			joycon.rumble(mk_odd(G4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// G2


			Sleep(100 / spd2);
			joycon.rumble(mk_odd(E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// E2
			Sleep(100 / spd2);
			joycon.rumble(mk_odd(G4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// G2
			Sleep(100 / spd2);
			joycon.rumble(mk_odd(A5), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// A3
			Sleep(200 / spd2);
			joycon.rumble(mk_odd(F4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// F2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(G4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// G2
			Sleep(200 / spd2);
			joycon.rumble(mk_odd(E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// E2


			Sleep(200 / spd2);
			joycon.rumble(mk_odd(C4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(D4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// D2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(B4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// B2

																					// new:

			Sleep(500 / spd2);

			joycon.rumble(mk_odd(G4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// G2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(F4G4), 1); Sleep(200 / spd); joycon.rumble(1, 3);// F2-G2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(F4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// F2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(D4E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);// D2-E2
			Sleep(200 / spd2);
			joycon.rumble(mk_odd(E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// E2

			Sleep(200 / spd2);

			joycon.rumble(mk_odd(G3A4), 1); Sleep(200 / spd); joycon.rumble(1, 3);// G1-A2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(A4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// A2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(C4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C2


			Sleep(200 / spd2);
			joycon.rumble(mk_odd(A4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// A2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(C4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(D4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// D2


			Sleep(300 / spd2);

			joycon.rumble(mk_odd(G4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// G2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(F4G4), 1); Sleep(200 / spd); joycon.rumble(1, 3);// F2-G2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(F4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// F2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(D4E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);// D2-E2
			Sleep(200 / spd2);
			joycon.rumble(mk_odd(E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// E2


																					// three notes:
			Sleep(200 / spd2);
			joycon.rumble(mk_odd(C5), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C3
			Sleep(200 / spd2);
			joycon.rumble(mk_odd(C3), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C3
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(C3), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C3


		N1:


			Sleep(500 / spd2);
			joycon.rumble(mk_odd(G4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// G2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(F4G4), 1); Sleep(200 / spd); joycon.rumble(1, 3);// F2G2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(F4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// F2

			Sleep(50 / spd2);
			joycon.rumble(mk_odd(D4E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);// D2E2

			Sleep(200 / spd2);
			joycon.rumble(mk_odd(E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// E2



			Sleep(200 / spd2);
			joycon.rumble(mk_odd(G3A4), 1); Sleep(200 / spd); joycon.rumble(1, 3);// G1A2

			Sleep(50 / spd2);
			joycon.rumble(mk_odd(A4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// A2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(C4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C2
			Sleep(100 / spd2);
			joycon.rumble(mk_odd(A4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// A2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(C4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(D4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// D2


			Sleep(300 / spd2);
			joycon.rumble(mk_odd(D4E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);// D2E2
			Sleep(300 / spd2);
			joycon.rumble(mk_odd(D4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// D2
			Sleep(300 / spd2);
			joycon.rumble(mk_odd(C4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C2


			Sleep(800 / spd2);


			joycon.rumble(mk_odd(G4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// G2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(F4G4), 1); Sleep(200 / spd); joycon.rumble(1, 3);// F2G2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(F4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// F2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(D4E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);// D2E2
			Sleep(200 / spd2);
			joycon.rumble(mk_odd(E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// E2

			Sleep(200 / spd2);


			joycon.rumble(mk_odd(G3A4), 1); Sleep(200 / spd); joycon.rumble(1, 3);// G1A2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(A4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// A2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(C4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C2

			Sleep(200 / spd2);

			joycon.rumble(mk_odd(A4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// A2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(C4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(D4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// D2

			Sleep(300 / spd2);


			joycon.rumble(mk_odd(G4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// G2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(F4G4), 1); Sleep(200 / spd); joycon.rumble(1, 3);// F2G2
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(F4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// F2


			Sleep(50 / spd2);
			joycon.rumble(mk_odd(D4E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);// D2E2
			Sleep(100 / spd2);
			joycon.rumble(mk_odd(E4), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// E2

																					// 30 second mark

																					// three notes:

			Sleep(300 / spd2);
			joycon.rumble(mk_odd(C5), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C3
			Sleep(200 / spd2);
			joycon.rumble(mk_odd(C5), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C3
			Sleep(50 / spd2);
			joycon.rumble(mk_odd(C5), 1); Sleep(200 / spd); joycon.rumble(1, 3);	// C3


			Sleep(1000);
		}
	}


	#define MusicOffset 600

	// notes in hertz:
	#define C3 131 + MusicOffset
	#define D3 146 + MusicOffset
	#define E3 165 + MusicOffset
	#define F3 175 + MusicOffset
	#define G3 196 + MusicOffset
	#define G3A4 208 + MusicOffset
	#define A4 440 + MusicOffset
	#define A4B4 466 + MusicOffset
	#define B4 494 + MusicOffset
	#define C4 262 + MusicOffset
	#define D4 294 + MusicOffset
	#define D4E4 311 + MusicOffset
	#define E4 329 + MusicOffset
	#define F4 349 + MusicOffset
	#define F4G4 215 + MusicOffset
	#define G4 392 + MusicOffset
	#define A5 880 + MusicOffset
	#define B5 988 + MusicOffset
	#define C5 523 + MusicOffset

	#define hfa 0xb0	// 8a
	#define lfa 0x006c	// 8062


	if (false) {
		for (int i = 0; i < 1; ++i) {

			printf("Playing mario theme...\n");

			float spd = 1;
			float spd2 = 1;

			Joycon joycon = joycons[0];

			Sleep(1000);

			joycon.rumble3(E4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// E2
			Sleep(100 / spd2);
			joycon.rumble3(E4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// E2
			Sleep(100 / spd2);
			joycon.rumble3(E4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// E2
			Sleep(100 / spd2);
			joycon.rumble3(C4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// C2
			Sleep(50 / spd2);
			joycon.rumble3(E4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// E2
			Sleep(100 / spd2);
			joycon.rumble3(G4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// G2
			Sleep(400 / spd2);

			joycon.rumble3(A4, hfa, lfa); Sleep(400 / spd); joycon.rumble(1, 3);	// too low for joycon
			Sleep(50 / spd2);

			joycon.rumble3(C4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// C2
			Sleep(200 / spd2);
			joycon.rumble3(G3, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// G1
			Sleep(200 / spd2);
			joycon.rumble3(E3, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// E1
			Sleep(200 / spd2);
			joycon.rumble3(A4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// A2

			Sleep(100 / spd2);
			joycon.rumble3(B4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// B2

			Sleep(50 / spd2);
			joycon.rumble3(A4B4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);// A2-B2?
			Sleep(50 / spd2);
			joycon.rumble3(A4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// A2
			Sleep(100 / spd2);
			joycon.rumble3(G3, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// G1


			Sleep(100 / spd2);
			joycon.rumble3(E4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// E2
			Sleep(100 / spd2);
			joycon.rumble3(G4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// G2
			Sleep(100 / spd2);
			joycon.rumble3(A5, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// A3



			Sleep(200 / spd2);
			joycon.rumble3(F4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// F2
			Sleep(50 / spd2);
			joycon.rumble3(G4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// G2

			Sleep(200 / spd2);
			joycon.rumble3(E4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// E2

			Sleep(50 / spd2);
			joycon.rumble3(C4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// C2
			Sleep(50 / spd2);
			joycon.rumble3(D4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// D2
			Sleep(50 / spd2);
			joycon.rumble3(B4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// B2


			Sleep(200 / spd2);
			joycon.rumble3(C4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// C2
			Sleep(200 / spd2);
			joycon.rumble3(G3, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// G1
			Sleep(200 / spd2);
			joycon.rumble3(E3, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// E1

			Sleep(200 / spd2);
			joycon.rumble3(A4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// A2
			Sleep(200 / spd2);
			joycon.rumble3(B4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// B2
			Sleep(200 / spd2);
			joycon.rumble3(A4B4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);// A2-B2?
			Sleep(50 / spd2);
			joycon.rumble3(A4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// A2
			Sleep(200 / spd2);
			joycon.rumble3(G4, hfa, lfa); Sleep(200 / spd); joycon.rumble(1, 3);	// G2


			Sleep(1000);
		}
	}

	printf("Done.\n");




}



void actuallyQuit() {

	for (int i = 1; i < 9; ++i) {
		RelinquishVJD(i);
	}

	for (int i = 0; i < joycons.size(); ++i) {
		buf[0] = 0x0; // disconnect
		joycons[i].send_subcommand(0x01, 0x06, buf, 1);
	}

	if (settings.usingGrip) {
		for (int i = 0; i < joycons.size(); ++i) {
			joycons[i].deinit_usb();
		}
	}
	// Finalize the hidapi library
	res = hid_exit();
}


// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

// control ids
enum {
	SpinTimer = wxID_HIGHEST + 1
};

// ----------------------------------------------------------------------------
// helper functions
// ----------------------------------------------------------------------------

static void CheckGLError() {
	GLenum errLast = GL_NO_ERROR;

	for (;; ) {
		GLenum err = glGetError();
		if (err == GL_NO_ERROR)
			return;

		// normally the error is reset by the call to glGetError() but if
		// glGetError() itself returns an error, we risk looping forever here
		// so check that we get a different error than the last time
		if (err == errLast) {
			wxLogError(wxT("OpenGL error state couldn't be reset."));
			return;
		}

		errLast = err;

		wxLogError(wxT("OpenGL error %d"), err);
	}
}

// function to draw the texture for cube faces
static wxImage DrawDice(int size, unsigned num) {
	wxASSERT_MSG(num >= 1 && num <= 6, wxT("invalid dice index"));

	const int dot = size / 16;        // radius of a single dot
	const int gap = 5 * size / 32;      // gap between dots

	wxBitmap bmp(size, size);
	wxMemoryDC dc;
	dc.SelectObject(bmp);
	dc.SetBackground(*wxWHITE_BRUSH);
	dc.Clear();
	dc.SetBrush(*wxBLACK_BRUSH);

	// the upper left and lower right points
	if (num != 1) {
		dc.DrawCircle(gap + dot, gap + dot, dot);
		dc.DrawCircle(size - gap - dot, size - gap - dot, dot);
	}

	// draw the central point for odd dices
	if (num % 2) {
		dc.DrawCircle(size / 2, size / 2, dot);
	}

	// the upper right and lower left points
	if (num > 3) {
		dc.DrawCircle(size - gap - dot, gap + dot, dot);
		dc.DrawCircle(gap + dot, size - gap - dot, dot);
	}

	// finally those 2 are only for the last dice
	if (num == 6) {
		dc.DrawCircle(gap + dot, size / 2, dot);
		dc.DrawCircle(size - gap - dot, size / 2, dot);
	}

	dc.SelectObject(wxNullBitmap);

	return bmp.ConvertToImage();
}

// ============================================================================
// implementation
// ============================================================================

// ----------------------------------------------------------------------------
// TestGLContext
// ----------------------------------------------------------------------------

TestGLContext::TestGLContext(wxGLCanvas *canvas) : wxGLContext(canvas) {
	SetCurrent(*canvas);

	// set up the parameters we want to use
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_TEXTURE_2D);

	// add slightly more light, the default lighting is rather dark
	GLfloat ambient[] = { 0.5, 0.5, 0.5, 0.5 };
	glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);

	// set viewing projection
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustum(-0.5f, 0.5f, -0.5f, 0.5f, 1.0f, 3.0f);

	// create the textures to use for cube sides: they will be reused by all
	// canvases (which is probably not critical in the case of simple textures
	// we use here but could be really important for a real application where
	// each texture could take many megabytes)
	glGenTextures(WXSIZEOF(m_textures), m_textures);

	for (unsigned i = 0; i < WXSIZEOF(m_textures); i++) {
		glBindTexture(GL_TEXTURE_2D, m_textures[i]);

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

		const wxImage img(DrawDice(256, i + 1));

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.GetWidth(), img.GetHeight(),
			0, GL_RGB, GL_UNSIGNED_BYTE, img.GetData());
	}

	CheckGLError();
}

void TestGLContext::DrawRotatedCube(glm::fquat q) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();


	//glm::mat4 m = glm::toMat4(q);
	glm::mat4 m = glm::mat4(1.0);
	m = glm::translate(m, glm::vec3(0.0f, 0.0f, -2.0f));
	m = m * glm::toMat4(q);
	glLoadMatrixf(&m[0][0]);

	// draw six faces of a cube of size 1 centered at (0, 0, 0)
	glBindTexture(GL_TEXTURE_2D, m_textures[0]);
	glBegin(GL_QUADS);
	glNormal3f(0.0f, 0.0f, 1.0f);
	glTexCoord2f(0, 0); glVertex3f(0.5f, 0.5f, 0.5f);
	glTexCoord2f(1, 0); glVertex3f(-0.5f, 0.5f, 0.5f);
	glTexCoord2f(1, 1); glVertex3f(-0.5f, -0.5f, 0.5f);
	glTexCoord2f(0, 1); glVertex3f(0.5f, -0.5f, 0.5f);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, m_textures[1]);
	glBegin(GL_QUADS);
	glNormal3f(0.0f, 0.0f, -1.0f);
	glTexCoord2f(0, 0); glVertex3f(-0.5f, -0.5f, -0.5f);
	glTexCoord2f(1, 0); glVertex3f(-0.5f, 0.5f, -0.5f);
	glTexCoord2f(1, 1); glVertex3f(0.5f, 0.5f, -0.5f);
	glTexCoord2f(0, 1); glVertex3f(0.5f, -0.5f, -0.5f);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, m_textures[2]);
	glBegin(GL_QUADS);
	glNormal3f(0.0f, 1.0f, 0.0f);
	glTexCoord2f(0, 0); glVertex3f(0.5f, 0.5f, 0.5f);
	glTexCoord2f(1, 0); glVertex3f(0.5f, 0.5f, -0.5f);
	glTexCoord2f(1, 1); glVertex3f(-0.5f, 0.5f, -0.5f);
	glTexCoord2f(0, 1); glVertex3f(-0.5f, 0.5f, 0.5f);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, m_textures[3]);
	glBegin(GL_QUADS);
	glNormal3f(0.0f, -1.0f, 0.0f);
	glTexCoord2f(0, 0); glVertex3f(-0.5f, -0.5f, -0.5f);
	glTexCoord2f(1, 0); glVertex3f(0.5f, -0.5f, -0.5f);
	glTexCoord2f(1, 1); glVertex3f(0.5f, -0.5f, 0.5f);
	glTexCoord2f(0, 1); glVertex3f(-0.5f, -0.5f, 0.5f);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, m_textures[4]);
	glBegin(GL_QUADS);
	glNormal3f(1.0f, 0.0f, 0.0f);
	glTexCoord2f(0, 0); glVertex3f(0.5f, 0.5f, 0.5f);
	glTexCoord2f(1, 0); glVertex3f(0.5f, -0.5f, 0.5f);
	glTexCoord2f(1, 1); glVertex3f(0.5f, -0.5f, -0.5f);
	glTexCoord2f(0, 1); glVertex3f(0.5f, 0.5f, -0.5f);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, m_textures[5]);
	glBegin(GL_QUADS);
	glNormal3f(-1.0f, 0.0f, 0.0f);
	glTexCoord2f(0, 0); glVertex3f(-0.5f, -0.5f, -0.5f);
	glTexCoord2f(1, 0); glVertex3f(-0.5f, -0.5f, 0.5f);
	glTexCoord2f(1, 1); glVertex3f(-0.5f, 0.5f, 0.5f);
	glTexCoord2f(0, 1); glVertex3f(-0.5f, 0.5f, -0.5f);
	glEnd();

	glFlush();

	CheckGLError();
}

void TestGLContext::DrawRotatedCube(float xangle, float yangle, float zangle) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0.0f, 0.0f, -2.0f);
	glRotatef(xangle, 1.0f, 0.0f, 0.0f);
	glRotatef(yangle, 0.0f, 1.0f, 0.0f);
	glRotatef(zangle, 0.0f, 0.0f, 1.0f);

	// draw six faces of a cube of size 1 centered at (0, 0, 0)
	glBindTexture(GL_TEXTURE_2D, m_textures[0]);
	glBegin(GL_QUADS);
	glNormal3f(0.0f, 0.0f, 1.0f);
	glTexCoord2f(0, 0); glVertex3f(0.5f, 0.5f, 0.5f);
	glTexCoord2f(1, 0); glVertex3f(-0.5f, 0.5f, 0.5f);
	glTexCoord2f(1, 1); glVertex3f(-0.5f, -0.5f, 0.5f);
	glTexCoord2f(0, 1); glVertex3f(0.5f, -0.5f, 0.5f);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, m_textures[1]);
	glBegin(GL_QUADS);
	glNormal3f(0.0f, 0.0f, -1.0f);
	glTexCoord2f(0, 0); glVertex3f(-0.5f, -0.5f, -0.5f);
	glTexCoord2f(1, 0); glVertex3f(-0.5f, 0.5f, -0.5f);
	glTexCoord2f(1, 1); glVertex3f(0.5f, 0.5f, -0.5f);
	glTexCoord2f(0, 1); glVertex3f(0.5f, -0.5f, -0.5f);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, m_textures[2]);
	glBegin(GL_QUADS);
	glNormal3f(0.0f, 1.0f, 0.0f);
	glTexCoord2f(0, 0); glVertex3f(0.5f, 0.5f, 0.5f);
	glTexCoord2f(1, 0); glVertex3f(0.5f, 0.5f, -0.5f);
	glTexCoord2f(1, 1); glVertex3f(-0.5f, 0.5f, -0.5f);
	glTexCoord2f(0, 1); glVertex3f(-0.5f, 0.5f, 0.5f);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, m_textures[3]);
	glBegin(GL_QUADS);
	glNormal3f(0.0f, -1.0f, 0.0f);
	glTexCoord2f(0, 0); glVertex3f(-0.5f, -0.5f, -0.5f);
	glTexCoord2f(1, 0); glVertex3f(0.5f, -0.5f, -0.5f);
	glTexCoord2f(1, 1); glVertex3f(0.5f, -0.5f, 0.5f);
	glTexCoord2f(0, 1); glVertex3f(-0.5f, -0.5f, 0.5f);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, m_textures[4]);
	glBegin(GL_QUADS);
	glNormal3f(1.0f, 0.0f, 0.0f);
	glTexCoord2f(0, 0); glVertex3f(0.5f, 0.5f, 0.5f);
	glTexCoord2f(1, 0); glVertex3f(0.5f, -0.5f, 0.5f);
	glTexCoord2f(1, 1); glVertex3f(0.5f, -0.5f, -0.5f);
	glTexCoord2f(0, 1); glVertex3f(0.5f, 0.5f, -0.5f);
	glEnd();

	glBindTexture(GL_TEXTURE_2D, m_textures[5]);
	glBegin(GL_QUADS);
	glNormal3f(-1.0f, 0.0f, 0.0f);
	glTexCoord2f(0, 0); glVertex3f(-0.5f, -0.5f, -0.5f);
	glTexCoord2f(1, 0); glVertex3f(-0.5f, -0.5f, 0.5f);
	glTexCoord2f(1, 1); glVertex3f(-0.5f, 0.5f, 0.5f);
	glTexCoord2f(0, 1); glVertex3f(-0.5f, 0.5f, -0.5f);
	glEnd();

	glFlush();

	CheckGLError();
}

// ----------------------------------------------------------------------------
// MyApp: the application object
// ----------------------------------------------------------------------------

//IMPLEMENT_APP(app);
wxIMPLEMENT_APP_NO_MAIN(MyApp);

bool MyApp::OnInit() {
	if (!wxApp::OnInit()) {
		return false;
	}

	Connect(wxID_ANY, wxEVT_IDLE, wxIdleEventHandler(MyApp::onIdle));
	new MainFrame();

	//new MyFrame();
	//m_myTimer.Start(0);
	return true;
}

int MyApp::OnExit() {
	delete m_glContext;
	delete m_glStereoContext;

	actuallyQuit();

	return wxApp::OnExit();
}

void MyApp::onIdle(wxIdleEvent& evt) {
	pollLoop();
}

TestGLContext& MyApp::GetContext(wxGLCanvas *canvas, bool useStereo) {
	TestGLContext *glContext;
	if (useStereo) {
		if (!m_glStereoContext) {
			// Create the OpenGL context for the first stereo window which needs it:
			// subsequently created windows will all share the same context.
			m_glStereoContext = new TestGLContext(canvas);
		}
		glContext = m_glStereoContext;
	} else {
		if (!m_glContext) {
			// Create the OpenGL context for the first mono window which needs it:
			// subsequently created windows will all share the same context.
			m_glContext = new TestGLContext(canvas);
		}
		glContext = m_glContext;
	}

	glContext->SetCurrent(*canvas);

	return *glContext;
}



MainFrame::MainFrame() : wxFrame(NULL, wxID_ANY, wxT("JoyCon-Driver by fosse ©2018")) {

	wxPanel *panel = new wxPanel(this, wxID_ANY);

	//this->Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(MainFrame::onQuit), NULL, this);
	//Connect(this->GetId(), wxEVT_CLOSE_WINDOW, wxCloseEventHandler(wxCloseEventFunction, MainFrame::onQuit));
	//this->Bind(wxEVT_CLOSE_WINDOW, &MainFrame::onQuit, this);
	Connect(wxEVT_CLOSE_WINDOW, wxCloseEventHandler(MainFrame::onQuit2));

	CB1 = new wxCheckBox(panel, wxID_ANY, wxT("Combine JoyCons"), wxPoint(20, 20));
	CB1->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MainFrame::toggleCombine, this);
	CB1->SetValue(settings.combineJoyCons);

	CB6 = new wxCheckBox(panel, wxID_ANY, wxT("Reverse Stick X"), wxPoint(20, 40));
	CB6->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MainFrame::toggleReverseX, this);
	CB6->SetValue(settings.reverseX);
	CB7 = new wxCheckBox(panel, wxID_ANY, wxT("Reverse Stick Y"), wxPoint(120, 40));
	CB7->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MainFrame::toggleReverseY, this);
	CB7->SetValue(settings.reverseY);

	gyroCheckBox = new wxCheckBox(panel, wxID_ANY, wxT("Gyro Controls"), wxPoint(20, 60));
	gyroCheckBox->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MainFrame::toggleGyro, this);
	gyroCheckBox->SetValue(settings.enableGyro);

	CB4 = new wxCheckBox(panel, wxID_ANY, wxT("Gyro Window"), wxPoint(120, 60));
	CB4->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MainFrame::toggleGyroWindow, this);
	CB4->SetValue(settings.gyroWindow);
	CB8 = new wxCheckBox(panel, wxID_ANY, wxT("Prefer Left JoyCon for Gyro Controls"), wxPoint(20, 80));
	CB8->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MainFrame::togglePreferLeftJoyCon, this);
	CB8->SetValue(settings.preferLeftJoyCon);
	CB12 = new wxCheckBox(panel, wxID_ANY, wxT("Quick Toggle Gyro Controls"), wxPoint(20, 100));
	CB12->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MainFrame::toggleQuickToggleGyro, this);
	CB12->SetValue(settings.quickToggleGyro);
	CB13 = new wxCheckBox(panel, wxID_ANY, wxT("Invert Quick Toggle"), wxPoint(190, 100));
	CB13->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MainFrame::toggleInvertQuickToggle, this);
	CB13->SetValue(settings.invertQuickToggle);

	CB5 = new wxCheckBox(panel, wxID_ANY, wxT("Mario Theme"), wxPoint(20, 120));
	CB5->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MainFrame::toggleMario, this);
	CB5->SetValue(settings.marioTheme);
	CB14 = new wxCheckBox(panel, wxID_ANY, wxT("Dolphin Mode"), wxPoint(120, 120));
	CB14->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MainFrame::toggleDolphinPointerMode, this);
	CB14->SetValue(settings.dolphinPointerMode);

	CB9 = new wxCheckBox(panel, wxID_ANY, wxT("Debug Mode"), wxPoint(20, 140));
	CB9->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MainFrame::toggleDebugMode, this);
	CB9->SetValue(settings.debugMode);

	CB10 = new wxCheckBox(panel, wxID_ANY, wxT("Write Debug To File"), wxPoint(120, 140));
	CB10->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MainFrame::toggleWriteDebug, this);
	CB10->SetValue(settings.debugMode);

	CB11 = new wxCheckBox(panel, wxID_ANY, wxT("Force Poll Update"), wxPoint(20, 160));
	CB11->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MainFrame::toggleForcePoll, this);
	CB11->SetValue(settings.forcePollUpdate);


	slider1Text = new wxStaticText(panel, wxID_ANY, wxT("Gyro Controls Sensitivity X"), wxPoint(20, 200));
	st1 = new wxStaticText(panel, wxID_ANY, wxT("(Also the sensitivity for Rz/sl0/sl1)"), wxPoint(40, 220));
	slider1 = new wxSlider(panel, wxID_ANY, settings.gyroSensitivityX, -1000, 1000, wxPoint(180, 180), wxSize(150, 20), wxSL_LABELS);
	slider1->Bind(wxEVT_SLIDER, &MainFrame::setGyroSensitivityX, this);


	slider2Text = new wxStaticText(panel, wxID_ANY, wxT("Gyro Controls Sensitivity Y"), wxPoint(20, 240));
	slider2 = new wxSlider(panel, wxID_ANY, settings.gyroSensitivityY, -1000, 1000, wxPoint(180, 220), wxSize(150, 20), wxSL_LABELS);
	slider2->Bind(wxEVT_SLIDER, &MainFrame::setGyroSensitivityY, this);

	CB15 = new wxCheckBox(panel, wxID_ANY, wxT("Broadcast Mode"), wxPoint(140, 160));
	CB15->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MainFrame::toggleBroadcastMode, this);
	CB15->SetValue(settings.broadcastMode);

	CB16 = new wxCheckBox(panel, wxID_ANY, wxT("Write Cast to File"), wxPoint(20, 180));
	CB16->Bind(wxEVT_COMMAND_CHECKBOX_CLICKED, &MainFrame::toggleWriteCastToFile, this);
	CB16->SetValue(settings.broadcastMode);


	gyroComboCodeText = new wxStaticText(panel, wxID_ANY, wxT("Gyro Combo Code: "), wxPoint(20, 270));

	st1 = new wxStaticText(panel, wxID_ANY, wxT("Change the default settings and more in the config file!"), wxPoint(20, 300));

	wxString version;
	version.Printf("JoyCon-Driver version %s\n", settings.version);
	st2 = new wxStaticText(panel, wxID_ANY, version, wxPoint(20, 330));

	startButton = new wxButton(panel, wxID_EXIT, wxT("Start"), wxPoint(150, 360));
	startButton->Bind(wxEVT_BUTTON, &MainFrame::onStart, this);

	quitButton = new wxButton(panel, wxID_EXIT, wxT("Quit"), wxPoint(250, 360));
	quitButton->Bind(wxEVT_BUTTON, &MainFrame::onQuit, this);

	updateButton = new wxButton(panel, wxID_EXIT, wxT("Check for update"), wxPoint(18, 360));
	updateButton->Bind(wxEVT_BUTTON, &MainFrame::onUpdate, this);

	donateButton = new wxButton(panel, wxID_EXIT, wxT("Donate"), wxPoint(250, 325));
	donateButton->Bind(wxEVT_BUTTON, &MainFrame::onDonate, this);

	SetClientSize(350, 400);
	Show();

	checkForUpdate();
}


void MainFrame::onStart(wxCommandEvent&) {
	setupConsole("Debug");
	if (settings.gyroWindow) {
		new MyFrame();
	}


	start();
	if (!settings.gyroWindow) {
		while (true) {
			pollLoop();
			wxYield();// so that the main window doesn't freeze
		}
	}
}

void MainFrame::onQuit(wxCommandEvent&) {
	actuallyQuit();
	exit(0);
}

void MainFrame::onQuit2(wxCloseEvent&) {
	actuallyQuit();
	exit(0);
}


void MainFrame::checkForUpdate() {

	download("version.txt", "https://fosse.co/version.txt");
	std::ifstream ifs("version.txt");
	std::string content;
	content.assign((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

	wxString alert;

	bool upToDate = (content == settings.version) ? true : false;

	if (!upToDate) {
		alert.Printf("An update is available!\nCurrent version: %s\nLatest version: %s\n", settings.version, content);
		wxMessageBox(alert);
		alert.Printf("To update to the latest version, just click check for update, and the latest version will be installed automatically.\n");
		wxMessageBox(alert);
	}

	//if (!upToDate) {
	//	wxString text;
	//	text.Printf("Updating! the program will now close, it may fail so check the version when the script finishes.\n");
	//	wxMessageBox(text);
	//	download("latest.zip", "https://fosse.co/latest.zip");
	//	download("update.bat", "https://fosse.co/update.bat");
	//	system("start update.bat");
	//	exit(0);
	//}
}


void MainFrame::onUpdate(wxCommandEvent&) {

	//download("version.txt", "https://raw.githubusercontent.com/mfosse/JoyCon-Driver/master/joycon-driver/build/Win32/Release/version.txt");
	download("version.txt", "https://fosse.co/version.txt");
	std::ifstream ifs("version.txt");
	std::string content;
	content.assign((std::istreambuf_iterator<char>(ifs)), (std::istreambuf_iterator<char>()));

	wxString alert;

	bool upToDate = (content == settings.version) ? true : false;

	if (!upToDate) {
		alert.Printf("An update is available!\nCurrent version: %s\nLatest version: %s\n", settings.version, content);
	} else {
		alert.Printf("You are running the latest version!\n");
	}
	wxMessageBox(alert);

	if (!upToDate) {
		//wxAboutDialogInfo info;
		//info.SetWebSite("https://github.com/mfosse/JoyCon-Driver");
		//wxAboutBox(info);

		wxString text;
		text.Printf("Updating! the program will now close, it may fail so check the version when the script finishes.\n");
		wxMessageBox(text);
		download("latest.zip", "https://fosse.co/latest.zip");
		download("update.bat", "https://fosse.co/update.bat");
		system("start update.bat");

		exit(0);
	}
}

void MainFrame::onDonate(wxCommandEvent&) {
	wxString alert;
	alert.Printf("Thank you very much!\n\nI have a paypal at matt.cfosse@gmail.com\nBTC Address: 17hDC2X7a1SWjsqBJRt9mJb9fJjqLCwgzG\nETH Address: 0xFdcA914e1213af24fD20fB6855E89141DF8caF96\n");
	wxMessageBox(alert);
}

void MainFrame::toggleCombine(wxCommandEvent&) {
	settings.combineJoyCons = !settings.combineJoyCons;
}

void MainFrame::toggleGyro(wxCommandEvent&) {
	settings.enableGyro = !settings.enableGyro;
}

void MainFrame::toggleGyroWindow(wxCommandEvent&) {
	settings.gyroWindow = !settings.gyroWindow;
}

void MainFrame::toggleMario(wxCommandEvent&) {
	settings.marioTheme = !settings.marioTheme;
}

void MainFrame::toggleReverseX(wxCommandEvent&) {
	settings.reverseX = !settings.reverseX;
}

void MainFrame::toggleReverseY(wxCommandEvent&) {
	settings.reverseY = !settings.reverseY;
}

void MainFrame::togglePreferLeftJoyCon(wxCommandEvent&) {
	settings.preferLeftJoyCon = !settings.preferLeftJoyCon;
}

void MainFrame::toggleQuickToggleGyro(wxCommandEvent&) {
	settings.quickToggleGyro = !settings.quickToggleGyro;
}

void MainFrame::toggleInvertQuickToggle(wxCommandEvent&) {
	settings.invertQuickToggle = !settings.invertQuickToggle;
}

void MainFrame::toggleDolphinPointerMode(wxCommandEvent &) {
	settings.dolphinPointerMode = !settings.dolphinPointerMode;
}

void MainFrame::toggleDebugMode(wxCommandEvent&) {
	settings.debugMode = !settings.debugMode;
}

void MainFrame::toggleWriteDebug(wxCommandEvent&) {
	settings.writeDebugToFile = !settings.writeDebugToFile;
	// find a debug file to output to:

	if (settings.writeDebugToFile) {
		int fileNumber = 0;
		std::string name = std::string("output-") + std::to_string(fileNumber) + std::string(".txt");
		while (exists_test0(name)) {
			fileNumber += 1;
			name = std::string("output-") + std::to_string(fileNumber) + std::string(".txt");
		}
		settings.outputFile = fopen(name.c_str(), "w");
	}
}

void MainFrame::toggleForcePoll(wxCommandEvent&) {
	settings.forcePollUpdate = !settings.forcePollUpdate;
}

void MainFrame::setGyroSensitivityX(wxCommandEvent&) {
	settings.gyroSensitivityX = slider1->GetValue();
}

void MainFrame::setGyroSensitivityY(wxCommandEvent&) {
	settings.gyroSensitivityY = slider2->GetValue();
}

void MainFrame::toggleBroadcastMode(wxCommandEvent &) {
	settings.broadcastMode = !settings.broadcastMode;
}

void MainFrame::toggleWriteCastToFile(wxCommandEvent &) {
	settings.writeCastToFile = !settings.writeCastToFile;
}

void setGyroComboCodeText(int code) {
	wxString text;
	text.Printf("Gyro Combo Code: %d\n", code);
	gyroComboCodeText->SetLabel(text);
}


// ----------------------------------------------------------------------------
// TestGLCanvas
// ----------------------------------------------------------------------------

wxBEGIN_EVENT_TABLE(TestGLCanvas, wxGLCanvas)
EVT_PAINT(TestGLCanvas::OnPaint)
EVT_KEY_DOWN(TestGLCanvas::OnKeyDown)
EVT_TIMER(SpinTimer, TestGLCanvas::OnSpinTimer)
wxEND_EVENT_TABLE()

// With perspective OpenGL graphics, the wxFULL_REPAINT_ON_RESIZE style
// flag should always be set, because even making the canvas smaller should
// be followed by a paint event that updates the entire canvas with new
// viewport settings.
TestGLCanvas::TestGLCanvas(wxWindow *parent, int *attribList)
	: wxGLCanvas(parent, wxID_ANY, attribList,
		wxDefaultPosition, wxDefaultSize,
		wxFULL_REPAINT_ON_RESIZE),
	m_xangle(0.0),
	m_yangle(0.0),
	m_spinTimer(this, SpinTimer),
	m_useStereo(false),
	m_stereoWarningAlreadyDisplayed(false)
{
	if (attribList) {
		int i = 0;
		while (attribList[i] != 0) {
			if (attribList[i] == WX_GL_STEREO) {
				m_useStereo = true;
			}
			++i;
		}
	}
	m_spinTimer.Start(0);
}

void TestGLCanvas::OnPaint(wxPaintEvent& WXUNUSED(event)) {
	// This is required even though dc is not used otherwise.
	wxPaintDC dc(this);

	// Set the OpenGL viewport according to the client size of this canvas.
	// This is done here rather than in a wxSizeEvent handler because our
	// OpenGL rendering context (and thus viewport setting) is used with
	// multiple canvases: If we updated the viewport in the wxSizeEvent
	// handler, changing the size of one canvas causes a viewport setting that
	// is wrong when next another canvas is repainted.
	const wxSize ClientSize = GetClientSize();

	TestGLContext& canvas = wxGetApp().GetContext(this, m_useStereo);
	glViewport(0, 0, ClientSize.x, ClientSize.y);

	// Render the graphics and swap the buffers.
	GLboolean quadStereoSupported;
	glGetBooleanv(GL_STEREO, &quadStereoSupported);

	canvas.DrawRotatedCube(tracker.quat);
	//canvas.DrawRotatedCube(tracker.anglex, tracker.angley, tracker.anglez);
	if (m_useStereo && !m_stereoWarningAlreadyDisplayed) {
		m_stereoWarningAlreadyDisplayed = true;
		wxLogError("Stereo not supported by the graphics card.");
	}

	SwapBuffers();
}

void TestGLCanvas::OnKeyDown(wxKeyEvent& event) {

	glm::fquat del;
	float angle = 0.25f;

	switch (event.GetKeyCode()) {
	case WXK_RIGHT:
		del = glm::angleAxis(-angle, glm::vec3(0.0, 0.0, 1.0));
		tracker.quat = tracker.quat*del;
		break;

	case WXK_LEFT:
		del = glm::angleAxis(angle, glm::vec3(0.0, 0.0, 1.0));
		tracker.quat = tracker.quat*del;
		break;

	case WXK_DOWN:
		del = glm::angleAxis(angle, glm::vec3(1.0, 0.0, 0.0));
		tracker.quat = tracker.quat*del;
		break;

	case WXK_UP:
		del = glm::angleAxis(-angle, glm::vec3(1.0, 0.0, 0.0));
		tracker.quat = tracker.quat*del;
		break;

	case WXK_SPACE:
		tracker.anglex = 0;
		tracker.angley = 0;
		tracker.anglez = 0;
		tracker.quat = glm::angleAxis(0.0f, glm::vec3(1.0, 0.0, 0.0));
		break;

	default:
		event.Skip();
		return;
	}
}

void TestGLCanvas::OnSpinTimer(wxTimerEvent& WXUNUSED(event)) {
	//Spin(tracker.relX, tracker.relY);
	Refresh(false);
	pollLoop();
}

wxString glGetwxString(GLenum name) {
	const GLubyte *v = glGetString(name);
	if (v == 0) {
		// The error is not important. It is GL_INVALID_ENUM.
		// We just want to clear the error stack.
		glGetError();

		return wxString();
	}

	return wxString((const char*)v);
}


// ----------------------------------------------------------------------------
// MyFrame: main application window
// ----------------------------------------------------------------------------

wxBEGIN_EVENT_TABLE(MyFrame, wxFrame)
//EVT_MENU(wxID_NEW, MyFrame::OnNewWindow)
//EVT_MENU(wxID_CLOSE, MyFrame::OnClose)
wxEND_EVENT_TABLE()

MyFrame::MyFrame(bool stereoWindow) : wxFrame(NULL, wxID_ANY, wxT("3D JoyCon gyroscope visualizer")) {
	int stereoAttribList[] = { WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_STEREO, 0 };

	new TestGLCanvas(this, stereoWindow ? stereoAttribList : NULL);

	SetIcon(wxICON(sample));

	SetClientSize(400, 400);
	Show();

	// test IsDisplaySupported() function:
	static const int attribs[] = { WX_GL_RGBA, WX_GL_DOUBLEBUFFER, 0 };
	wxLogStatus("Double-buffered display %s supported", wxGLCanvas::IsDisplaySupported(attribs) ? "is" : "not");

	if (stereoWindow) {
		const wxString vendor = glGetwxString(GL_VENDOR).Lower();
		const wxString renderer = glGetwxString(GL_RENDERER).Lower();
		if (vendor.find("nvidia") != wxString::npos && renderer.find("quadro") == wxString::npos) {
			ShowFullScreen(true);
		}
	}
}


//int main(int argc, char *argv[]) {
int wWinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPWSTR cmdLine, int cmdShow) {




	parseSettings2();
	wxEntry(hInstance);
	return 0;
}
