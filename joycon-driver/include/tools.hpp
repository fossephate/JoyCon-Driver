#pragma once



double threshold(double a, double thresh) {
	if (abs(a) > thresh) {
		return a;
	} else {
		return 0;
	}
}