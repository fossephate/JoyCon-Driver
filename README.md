# JoyCon-Driver
A vJoy feeder for the Nintendo Switch JoyCons


Credit to:
https://github.com/dekuNukem/ and https://github.com/riking/ for their work on figuring out the joystick data.


To use the this, you'll need to install vJoy, found here: http://vjoystick.sourceforge.net/site/


The offsets in the program are specific to my JoyCons, so you'll likely want to edit them to fit yours.
Theres also a bool to combine the JoyCons into a single vJoy device, or keep them as seperate vJoy devices (vJoy device #1 & #2)

Pair the JoyCon(s), and run the Application, if it doesn't work, make sure your JoyCons are fully paired/connected and restart the program.
Note: the JoyCons seem to need to be re-paired anytime after they've reconnected to the switch.
To re-pair the JoyCons go into Settings and remove the JoyCon(s) and then pair them again.

Once the program is running vJoy should register the input from the JoyCons.

You'll likely want to use this with something like x360ce (http://www.x360ce.com), which will map the vJoy device to an xbox controller.
