# can-opener

A Ruby application built around the OpenPort J2534 CANBUS library

The project goals (subject to evolution) are as follows in order of priority with those goals achieved struck through;

1. Set up basic repo with CANlogger original source code - **DONE**
2. Provide a visual representation of data (packets) flowing through the CANBUS
	* Rework CANlogger to output its data to a STDIO channel that ruby can utilise
	* Build a basic ruby script around CANLogger to output received data
	* Build a GUI on top of that, moving away from command line
3. Determine if it's possible (and then implement how) to switch between different speed CAN busses
4. Group and categorise that information to permit easy dissemination of the information (i.e. fold-out menus)
5. Label / colourise known source and destination of CANBUS packets to further ease human processing of information
6. If possible, perform one of the following tasks;
	* Compile J2534 C++ libraries as a ruby Gem, or
	* Rewrite J2534 libraries in ruby if possible
7. Build a standalone application
8. Somehow get it working on other platforms

## Structure

* **CAN-Opener** - The main project folder
	* **src** - Source code for the Ruby application
	* **tweakedcl** - Tweaked version of the CANlogger source to output logged data to STDIO instead of a file
* **CANlogger** - Original source code from Tactrix to access the OpenPort adapter

