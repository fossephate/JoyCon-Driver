# JoyCon-Driver
A vJoy feeder / Driver for the Nintendo Switch JoyCons on Windows with analog stick support and motion controls

## How to use
1. Install vJoy, found here: http://vjoystick.sourceforge.net/site/

2. Setup your vJoy Devices to look like this:
    * http://i.imgur.com/nXQDFPK.png
    * http://i.imgur.com/bF1re0X.png

3. Pair the JoyCon(s) to your PC

4. Run the Application, if it doesn't detect your JoyCon(s), make sure they are fully paired/connected and restart the program.
	* For the latest features and updates, clone the repositiory / download the zip
	* For the last major release, go here: https://github.com/mfosse/JoyCon-Driver/releases

5. Once the program is running vJoy should register the input from the JoyCons.
    * To verify it's working you can use the vJoy monitor that comes with vJoy, it looks like this: http://i.imgur.com/x4Fn7Cq.png
    * To re-pair the JoyCons go into Settings and remove the JoyCon(s) and then pair them again.
    * You'll likely want to use this with something like x360ce (http://www.x360ce.com), which will let you map the vJoy device to a virtual xbox controller for games that support them.


## Important Notes
* The JoyCons need to be re-paired anytime after they've reconnected to the switch.

* There is now a config file, and a GUI


These are the default settings:
```

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
float gyroSensitivityX = 100.0f;
float gyroSensitivityY = 100.0f;

// enables 3D gyroscope visualizer
bool gyroWindow = false;

// plays a version of the mario theme by vibrating
// the first JoyCon connected.
bool marioTheme = false;

// bool to restart the program
bool restart = false;

```



## Donate
  * If you like the project and would like to donate I have a paypal at matt.cfosse@gmail.com
  * BTC Address: 17hDC2X7a1SWjsqBJRt9mJb9fJjqLCwgzG
  * ETH Address: 0xFdcA914e1213af24fD20fB6855E89141DF8caF96




## Thanks
  * Thanks to everyone at: https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering/
