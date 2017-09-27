# JoyCon-Driver
A vJoy feeder for the Nintendo Switch JoyCons with analog stick support and motion controls


## How to use
1. Install vJoy, found here: http://vjoystick.sourceforge.net/site/

2. Setup your vJoy Devices to look like this:
    * http://i.imgur.com/nXQDFPK.png
    * http://i.imgur.com/bF1re0X.png

3. Pair the JoyCon(s) to your PC

4. Run the Application, if it doesn't detect your JoyCon(s), make sure they are fully paired/connected and restart the program.
	* There's a precompiled version of the program in the release folder with all of the dlls it needs, just download the zip and extract it.

5. Once the program is running vJoy should register the input from the JoyCons.
    * To verify it's working you can use the vJoy monitor that comes with vJoy, it looks like this: http://i.imgur.com/x4Fn7Cq.png
    * To re-pair the JoyCons go into Settings and remove the JoyCon(s) and then pair them again.
    * You'll likely want to use this with something like x360ce (http://www.x360ce.com), which will let you map the vJoy device to a virtual xbox controller for games that support them.


## Important Notes
* The JoyCons need to be re-paired anytime after they've reconnected to the switch.

* There is now a config file, and a GUI


These are the default settings:
```
// there appears to be a good amount of variance between JoyCons,
// but they work well once you find the right offsets
// these are the values that worked well for my JoyCons:
int leftJoyConXOffset = 16000;
int leftJoyConYOffset = 13000;

int rightJoyConXOffset = 15000;
int rightJoyConYOffset = 19000;

// multipliers to go from the range (-128,128) to (-32768, 32768)
// These shouldn't need to be changed too much, but the option is there
// I found that 250 works for me
int leftJoyConXMultiplier = 10;
int leftJoyConYMultiplier = 10;
int rightJoyConXMultiplier = 10;
int rightJoyConYMultiplier = 10;

// Enabling this combines both JoyCons to a single vJoy Device(#1)
// when combineJoyCons == false:
// JoyCon(L) is mapped to vJoy Device #1
// JoyCon(R) is mapped to vJoy Device #2
// when combineJoyCons == true:
// JoyCon(L) and JoyCon(R) are mapped to vJoy Device #1
bool combineJoyCons = false;

bool reverseX = false;// reverses joystick x (both sticks)
bool reverseY = false;// reverses joystick y (both sticks)

// Automatically center sticks
// works by getting joystick position at start
// and assumes that to be (0,0), and uses that to calculate the offsets
bool autoCenterSticks = false;


// enables motion controls
bool enableGyro = true;

// enables 3D gyroscope visualizer
bool gyroWindow = true;

// plays a version of the mario theme by vibrating
// the first JoyCon connected.
bool marioTheme = false;

```



## Donate
  * If you like the project and would like to donate I have a paypal at matt.cfosse@gmail.com
  * BTC Address: 17hDC2X7a1SWjsqBJRt9mJb9fJjqLCwgzG
  * ETH Address: 0xFdcA914e1213af24fD20fB6855E89141DF8caF96




## Thanks
  * Thanks to everyone at: https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/
