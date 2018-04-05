#pragma once
#include <chrono>
#include <thread>
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

#include <curl/curl.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;


double lowpassFilter(double a, double thresh) {
	if (abs(a) > thresh) {
		return a;
	} else {
		return 0;
	}
}

int rand_range(int min, int max) {
	return rand() % (max + 1 - min) + min;
}

// sleeps accurately:
void accurateSleep(double durationMS, double sleepThreshold = 1.8) {

	// get current time
	auto tNow = std::chrono::high_resolution_clock::now();

	auto tSleepStart = std::chrono::high_resolution_clock::now();

	auto tSleepDuration = std::chrono::duration_cast<std::chrono::microseconds>(tNow - tSleepStart);

	// get the application's runtime duration in ms
	//runningTimeMS = std::chrono::duration_cast<std::chrono::milliseconds>(tNow - tApplicationStart).count();
	//auto tFrameDuration = std::chrono::duration_cast<std::chrono::microseconds>(tNow - tFrameStart);
	//double tFrameDurationMS = tFrameDuration.count() / 1000.0;


	// time spent sleeping (0):
	double tSleepTimeMS = tSleepDuration.count() / 1000.0;

	//float lowerThres = 0.2;
	//float sleepThreshold = 1.8;//1.4

	// run cpu in circles
	while (tSleepTimeMS < durationMS) {
		// allow cpu to sleep if there is lots of time to kill
		if (tSleepTimeMS < durationMS - sleepThreshold) {
			std::this_thread::sleep_for(std::chrono::microseconds(100));
			//Sleep(1);
		}
		tNow = std::chrono::high_resolution_clock::now();
		tSleepDuration = std::chrono::duration_cast<std::chrono::microseconds>(tNow - tSleepStart);
		tSleepTimeMS = tSleepDuration.count() / 1000.0;
	}

	// done sleeping

}



/* The LoadConfig function loads the configuration file given by filename
It returns a map of key-value pairs stored in the conifuration file */
std::map<std::string, std::string> LoadConfig(std::string filename)
{
	std::ifstream input(filename); //The input stream
	std::map<std::string, std::string> ans; //A map of key-value pairs in the file
	while (input) //Keep on going as long as the file stream is good
	{
		std::string key; //The key
		std::string value; //The value
		std::getline(input, key, ':'); //Read up to the : delimiter into key
		std::getline(input, value, '\n'); //Read up to the newline into value
		std::string::size_type pos1 = value.find_first_of("\""); //Find the first quote in the value
		std::string::size_type pos2 = value.find_last_of("\""); //Find the last quote in the value
		if (pos1 != std::string::npos && pos2 != std::string::npos && pos2 > pos1) //Check if the found positions are all valid
		{
			value = value.substr(pos1 + 1, pos2 - pos1 - 1); //Take a substring of the part between the quotes
			ans[key] = value; //Store the result in the map
		}
	}
	input.close(); //Close the file stream
	return ans; //And return the result
}


void setupConsole(std::string title) {
	// setup console
	AllocConsole();
	freopen("conin$", "r", stdin);
	freopen("conout$", "w", stdout);
	freopen("conout$", "w", stderr);
	printf("Debugging Window:\n");
}


int16_t unsignedToSigned16(uint16_t n) {
	uint16_t A = n;
	uint16_t B = 0xFFFF - A;
	if (A < B) {
		return (int16_t)A;
	} else {
		return (int16_t)(-1 * B);
	}
}

int16_t uint16_to_int16(uint16_t a) {
	int16_t b;
	char* aPointer = (char*)&a, *bPointer = (char*)&b;
	memcpy(bPointer, aPointer, sizeof(a));
	return b;
}

uint16_t combine_uint8_t(uint8_t a, uint8_t b) {
	uint16_t c = ((uint16_t)a << 8) | b;
	return c;
}

int16_t combine_gyro_data(uint8_t a, uint8_t b) {
	uint16_t c = combine_uint8_t(a, b);
	int16_t d = uint16_to_int16(c);
	return d;
}


float clamp(float a, float min, float max) {
	if (a < min) {
		return min;
	} else if (a > max) {
		return max;
	} else {
		return a;
	}
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


float rand0t1() {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(0.0f, 1.0f);
	float rnd = dis(gen);
	return rnd;
}
































//struct s_button_map {
//	int bit;
//	char *name;
//};
//
//
//struct s_button_map button_map[16] = {
//	{ 0, "D" },{ 1, "R" },{ 2, "L" },{ 3, "U" },{ 4, "SL" },{ 5, "SR" },
//	{ 6, "?" },{ 7, "?" },{ 8, "-" },{ 9, "+" },{ 10, "LS" },{ 11, "RS" },
//	{ 12, "Ho" },{ 13, "Sc" },{ 14, "LR" },{ 15, "ZLR" },
//};
//
//void print_buttons(Joycon *jc) {
//
//	for (int i = 0; i < 16; i++) {
//		if (jc->buttons & (1 << button_map[i].bit)) {
//			printf("1");
//		} else {
//			printf("0");
//		}
//	}
//	printf("\n");
//}


//void print_buttons2(Joycon *jc) {
//
//	printf("Joycon %c (Unattached): ", L_OR_R(jc->left_right));
//
//	for (int i = 0; i < 32; i++) {
//		if (jc->buttons2[i]) {
//			printf("1");
//		} else {
//			printf("0");
//		}
//
//	}
//	printf("\n");
//}

//void print_stick2(Joycon *jc) {
//
//	printf("Joycon %c (Unattached): ", L_OR_R(jc->left_right));
//
//	printf("%d %d\n", jc->stick.horizontal, jc->stick.vertical);
//}




const char *const dstick_names[9] = { "Up", "UR", "Ri", "DR", "Do", "DL", "Le", "UL", "Neu" };



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

void int_dump(unsigned char *buf, int len) {
	for (int i = 0; i < len; i++) {
		printf("%i ", buf[i]);
	}
	printf("\n");
}

int _floor(float n) {
	return (int)n;
}


//void device_print(struct hid_device_info *dev) {
//	printf("USB device info:\n  vid: 0x%04hX pid: 0x%04hX\n  path: %s\n  serial_number: %ls\n  interface_number: %d\n",
//		dev->vendor_id, dev->product_id, dev->path, dev->serial_number, dev->interface_number);
//	printf("  Manufacturer: %ls\n", dev->manufacturer_string);
//	printf("  Product:      %ls\n\n", dev->product_string);
//}



//void print_dstick(Joycon *jc) {
//	printf("%s\n", dstick_names[jc->dstick]);
//}

//inline bool exists_test0(const std::string& name) {
//	ifstream f(name.c_str());
//	return f.good();
//}

inline bool exists_test0(const std::string& name) {
	if (FILE *file = fopen(name.c_str(), "r")) {
		fclose(file);
		return true;
	} else {
		return false;
	}
}

template<typename T>
std::string get_time(std::chrono::time_point<T> time) {
	using namespace std;
	using namespace std::chrono;

	time_t curr_time = T::to_time_t(time);
	char sRep[100];
	strftime(sRep, sizeof(sRep), "%Y-%m-%d %H:%M:%S", localtime(&curr_time));

	typename T::duration since_epoch = time.time_since_epoch();
	seconds sec = duration_cast<seconds>(since_epoch);
	since_epoch -= sec;
	milliseconds milli = duration_cast<milliseconds>(since_epoch);


	string s = "";
	stringstream ss;
	ss << "[" << sRep << ":" << milli.count() << "]";
	s = ss.str();

	//s = to_string(sRep);

	return s;

}


void download(char outfilename[FILENAME_MAX], char *url) {

	CURL *curl;
	FILE *fp;
	CURLcode res;

	curl = curl_easy_init();
	if (curl) {
		fp = fopen(outfilename, "wb");
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		fclose(fp);
	}

}