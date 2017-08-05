#pragma once
#include <chrono>
#include <thread>


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