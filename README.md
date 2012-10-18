# can-opener

A Ruby application built around the OpenPort J2534 CANBUS library

## Structure

---

* **CAN-Opener** - The main project folder
	* **src** - Source code for the Ruby application
	* **tweakedcl** - Tweaked version of the CANlogger source to output logged data to STDIO instead of a file
* **CANlogger** - Original source code from Tactrix to access the OpenPort adapter