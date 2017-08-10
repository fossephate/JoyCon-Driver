#pragma once
#include <chrono>
#include <thread>
#include <map>
#include <string>


double threshold(double a, double thresh) {
	if (abs(a) > thresh) {
		return a;
	} else {
		return 0;
	}
}

// sleeps accurately:
void accurateSleep(double durationMS) {

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

	float sleepThreshold = 1.8;//1.4

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