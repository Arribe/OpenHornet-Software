/**************************************************************************************
 *        ____                   _    _                       _
 *       / __ \                 | |  | |                     | |
 *      | |  | |_ __   ___ _ __ | |__| | ___  _ __ _ __   ___| |_
 *      | |  | | '_ \ / _ \ '_ \|  __  |/ _ \| '__| '_ \ / _ \ __|
 *      | |__| | |_) |  __/ | | | |  | | (_) | |  | | | |  __/ |_
 *       \____/| .__/ \___|_| |_|_|  |_|\___/|_|  |_| |_|\___|\__|
 *             | |
 *             |_|
 *   ----------------------------------------------------------------------------------
 *   Copyright 2016-2024 OpenHornet
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *   ----------------------------------------------------------------------------------
 *   Note: All other portions of OpenHornet not within the 'OpenHornet-Software' 
 *   GitHub repository is released under the Creative Commons Attribution -
 *   Non-Commercial - Share Alike License. (CC BY-NC-SA 4.0)
 *   ----------------------------------------------------------------------------------
 *   This Project uses Doxygen as a documentation generator.
 *   Please use Doxygen capable comments.
 **************************************************************************************/

/**
 * @file 4A9A1-THROTTLE_CONTROLLER.ino
 * @author Arribe
 * @date 04.01.2024
 * @version u.0.0.1
 * @copyright Copyright 2016-2024 OpenHornet. Licensed under the Apache License, Version 2.0.
 * @warning This sketch is based on a wiring diagram, and was not yet tested on hardware. The throttle solenoids weren't working. (Remove this line once tested on hardware and in system.)
 * @brief Controls the THROTTLE QUADRANT.
 *
 * @details
 * 
 *  * **Reference Designator:** 4A9A1
 *  * **Intended Board:** CONTROLLER_Throttle
 *  * **RS485 Bus Address:** NA
 * 
 * ### Wiring diagram:
 * PIN | Function
 * --- | ---
 * 12  | J2 OUTBD_CSx
 * 5   | J3 INBD_CSy
 * 40  | INBD_MAX_LIMIT
 * 38  | INBD_IDLE_LIMIT
 * 36  | OUTBD_MAX_LIMIT
 * 34  | OUTBD_IDLE_LIMIT
 * 21  | EXT_LTS
 * 17  | RAID_FLIR
 * 15  | ATC_ENGAGE
 * 39  | MAX_SOL_SIG
 * 37  | IDLE_SOL_SIG
 * 
*
* ## Hall Sensor Zero via #define SET_THROTTLE_ZERO
* It may be helpful to zero the hall sensors when both throttles are at ground idle position.
* If the Hall sensors are not zeroed out the reads my increase when advancing the throttles forward, and then loop around to a 
* lower value which is confusing for the Window's game controller and its calibration utility.
* 
* ## Disabling the max limit switches via #define DISABLE_MAX_LIMIT_SWITCHES 1
* The max limit switches make running the window's game controller calibarion challenging, and prevents moving the throttles to max
* without pushing a 'button' on the controller advancing the calibarion to the next screen.
*
* ## Reverse External Light Switch Movement
* Reversing the external lights switch may be needed by some (like me) who inadvertently put the switch / wires on backwards.
* Gave an option in code to reverse instead of pulling the outer grip apart to fix physically.
*
*/

#define DISABLE_MAX_SOL_SIG   ///< If defined the solenoid will be disabled in code, comment out line if the solenoid is used
#define DISABLE_IDLE_SOL_SIG  ///< If defined the solenoid will be disabled in code, comment out line if the solenoid is used
//#define SET_THROTTLE_ZERO           ///< Define to determine if code should allow the hall sensors to set its zero position based on initial position.
#define DISABLE_MAX_LIMIT_SWITCHES 1  ///< disables the max limit switches, change to 0 if you want them enabled, 1 is disabled.
#define REVERSE_EXT_LTS 0             ///< Reverses the read of the external lights switch, set to 0 or 1 if switch movement doesn't trigger in Window's game controller when switch toggle is forward

#define DCSBIOS_DISABLE_SERVO  ///< So the code will compile with an ESP32
#define DCSBIOS_DEFAULT_SERIAL

#include <Wire.h>
#include "SimpleFOC.h"
#include "SimpleFOCDrivers.h"
#include "encoders/mt6835/MagneticSensorMT6835.h"
#include "DcsBios.h"
#include "Joystick_ESP32S2.h"

// Define pins for DCS-BIOS per interconnect diagram.
#define OUTBD_CSX 12         ///< J2 OUTBD_CSx
#define INBD_CSY 5           ///< J3 INBD_CSy
#define INBD_MAX_LIMIT 40    ///< INBD_MAX_LIMIT
#define INBD_IDLE_LIMIT 38   ///< INBD_IDLE_LIMIT
#define OUTBD_MAX_LIMIT 36   ///< OUTBD_MAX_LIMIT
#define OUTBD_IDLE_LIMIT 34  ///< OUTBD_IDLE_LIMIT
#define EXT_LTS 21           ///< EXT_LTS
#define RAID_FLIR 17         ///< RAID_FLIR
#define ATC_ENGAGE 15        ///< ATC_ENGAGE
#define MAX_SOL_SIG 39           ///< MAX_SOL_SIG
#define IDLE_SOL_SIG 37          ///< IDLE_SOL_SIG

//Declare variables for custom non-DCS Bios logic
bool wowLeft = false;            ///< Weight-on-wheels for solenoid logic.
bool wowRight = false;           ///< Weight-on-wheels for solenoid logic.
bool wowNose = false;            ///< Weight-on-wheels for solenoid logic.
bool launchBarExtended = false;  ///< launch bar used for Max solenoid logic.
bool arrestingHookUp = true;     ///< hook up used for Max solenoid logic.


Joystick_ Joystick = Joystick_(
  JOYSTICK_DEFAULT_REPORT_ID,
  JOYSTICK_TYPE_JOYSTICK,
  18,
  0,
  true,
  true,
  true,
  true,
  true,
  false,
  false,
  false,
  false,
  false,
  false);

const bool testAutoSendMode = false;

SPISettings myMT6835SPISettings(1000000, MT6835_BITORDER, SPI_MODE3);
MagneticSensorMT6835 outboardThrottle = MagneticSensorMT6835(OUTBD_CSX, myMT6835SPISettings);
MagneticSensorMT6835 inboardThrottle = MagneticSensorMT6835(INBD_CSY, myMT6835SPISettings);


int pins[7] = { OUTBD_MAX_LIMIT, OUTBD_IDLE_LIMIT, INBD_MAX_LIMIT, INBD_IDLE_LIMIT, EXT_LTS, RAID_FLIR, ATC_ENGAGE };

// DCSBios reads to save airplane state information. <update comment as needed>
void onExtWowLeftChange(unsigned int newValue) {
  wowLeft = newValue;
}
DcsBios::IntegerBuffer extWowLeftBuffer(0x74d8, 0x0100, 8, onExtWowLeftChange);

void onExtWowRightChange(unsigned int newValue) {
  wowRight = newValue;
}
DcsBios::IntegerBuffer extWowRightBuffer(0x74d6, 0x8000, 15, onExtWowRightChange);

void onExtWowNoseChange(unsigned int newValue) {
  wowNose = newValue;
}
DcsBios::IntegerBuffer extWowNoseBuffer(0x74d6, 0x4000, 14, onExtWowNoseChange);

void onHookLeverChange(unsigned int newValue) {
  arrestingHookUp = newValue;  // 1 is hook up
}
DcsBios::IntegerBuffer hookLeverBuffer(0x74a0, 0x0200, 9, onHookLeverChange);

void onLaunchBarSwChange(unsigned int newValue) {
  launchBarExtended = newValue;  // 1 is extended
}
DcsBios::IntegerBuffer launchBarSwBuffer(0x7480, 0x2000, 13, onLaunchBarSwChange);

/**
* Arduino Setup Function
*
* Arduino standard Setup Function. Code who should be executed
* only once at the programm start, belongs in this function.
*
* @note If SET_THROTTLE_ZERO defined the code will read the hall sensors' initial position at power on to set that as zero.
* If used ensure that the throttles are pulled back to the ground idle (not fuel-cut-off/min position).
*
* ### Flight Idle Detent Solenoid
* If weight-on-wheels is true, pull flight idle detent down to allow throttle movement to ground idle position.
* 
* ### Afterburner Lockout Detent Solenoid
* If weight-on-wheels is true, and launch bar is extended or the hook is down then raise the afterburner 
* lockout detent to prevent inadvertent selection of afterburner. Otherwise pull the afterburner lockout detent down.
*
* @note If DISABLE_MAX_SOL_SIG or DISABLE_IDLE_SOL_SIG are not defined the solenoids will be used based on the logic from DCS state.
*
* @todo When flight idle detent is fixed mechanically for more consistent usage, explore using the aircraft's g-load to simiulate high-G
* maneuvers causing the flight idle stop to retract and allow inadvertent selection of ground idle.
*
*/
void setup() {

  // Run DCS Bios setup function
  DcsBios::setup();

  Wire.begin(SDA, SCL);
  Serial.begin(115200);

  outboardThrottle.init();
  inboardThrottle.init();
  inboardThrottle.setRotationDirection(-1);  // reverse hall sensor rotation so both throttles read as increasing when moving forward

#ifdef SET_THROTTLE_ZERO
  //throttle needs to be all the way back to ground idle when plugged into the computer to set the zero value.
  outboardThrottle.setZeroFromCurrentPosition();
  inboardThrottle.setZeroFromCurrentPosition();
#endif

  // set pinmode for the throttle controller's inputs.
  for (int j = 0; j < 7; j++) {
    pinMode(pins[j], INPUT_PULLUP);
  }

  pinMode(IDLE_SOL_SIG, OUTPUT);
  pinMode(MAX_SOL_SIG, OUTPUT);

  digitalWrite(IDLE_SOL_SIG, LOW);
  digitalWrite(MAX_SOL_SIG, LOW);

  Joystick.setXAxisRange(0, 1024); // TDC X-axis
  Joystick.setYAxisRange(0, 1024); // TDC Y-axis
  Joystick.setZAxisRange(0, 1024); // Radar Elevation
  Joystick.setRxAxisRange(0, 2048); // Outboard Throttle Arm
  Joystick.setRyAxisRange(0, 2048); // Inboard Throttle Arm
  Joystick.begin();
}

/**
* Arduino Loop Function
*
* Arduino standard Loop Function. Code who should be executed
* over and over in a loop, belongs in this function.
*/
void loop() {

  //Run DCS Bios loop function
  DcsBios::loop();

  uint32_t temp;  // temp value to hold the analog reads in preparation of doing logic.

  outboardThrottle.update();                          // update the outboard hall sensor to prep for read.
  temp = outboardThrottle.readRawAngle21();           // read outboard hall sensor
  Joystick.setRxAxis(map(temp, 0, 707444, 0, 2048));  //0 and 707444 came from reading the Serial Monitor for the min/max values to then plug into this line.
  // Uncomment the code below if you wish to pass the outboard throttle's raw values to the serial monitor
  //Serial.print("outbThrottle: ");
  //Serial.print(temp);

  inboardThrottle.update();                           // update the inboard hall sensor to prep for read
  temp = inboardThrottle.readRawAngle21();            // read inboard hall sensor
  Joystick.setRyAxis(map(temp, 0, 707444, 0, 2048));  //0 and 707444 came from reading the Serial Monitor for the min/max values to then plug into this line.
  // Uncomment the code below if you wish to pass the inboard throttle's raw values to the serial monitor
  //Serial.print("  inbThrottle: ");
  //Serial.print(valueY);
  //Serial.print("\n");

  // Read the pins and set the joystick's button state
  for (int j = 0; j < 7; j++) {
    if ((DISABLE_MAX_LIMIT_SWITCHES == 1) && ((j != 0) && (j != 2))) {  // If the max throttle limit switches are disabled, skip the read
      if ((REVERSE_EXT_LTS == 1) && (j == 5)) {                         // if REVERSE_EXT_LTS defined as 1 reverse the read value compared to the other pins
        Joystick.setButton(j, digitalRead(pins[j]));
      } else
        Joystick.setButton(j, !digitalRead(pins[j]));
    }
  }

  int index = 0;  // Initialize the index to keep track of which inner grip button we're reading
  Wire.requestFrom(49, 17); // request inner grip pro-mini return current button state info

  while (Wire.available()) { // While something to read from the inner grip
    if (index < 11) { // first 11 values are button presses
      Joystick.setButton(index + 7, Wire.read());  // inner throttle's digital button reads, set as joystick button state
    }
    // Remaining Index values are inner grip analog reads
    else if (index == 11) {          // TDC X-axis
      byte tempLow = Wire.read();      // read low byte
      byte tempHi = Wire.read();       // read high byte
      temp = (tempHi << 8) + tempLow;  //rebuild the TDC X-axis value
      Joystick.setXAxis(map(temp, 0, 65000, 0, 1024));

    } else if (index == 12) {          // TDC Y-axis
      byte tempLow = Wire.read();      // read low byte
      byte tempHi = Wire.read();       // read high byte
      temp = (tempHi << 8) + tempLow;  //rebuild the TDC Y-axis value
      Joystick.setYAxis(map(temp, 0, 65000, 0, 1024));

    } else if (index == 13) {
      byte tempLow = Wire.read();                       // read low byte
      byte tempHi = Wire.read();                        // read high byte
      temp = (tempHi << 8) + tempLow;                   //rebuild the Antenna axis value
      Joystick.setZAxis(map(temp, 754, 978, 0, 1024));  // mapped values determined by reading the retured elevation on the serial monitor
      // enable the lines below to see the Elevation knob's analog read values.
      //Serial.print(" Elevation: ");
      //Serial.print((tempHi << 8) + tempLow);
      //Serial.print("\n");
    }
    index++;  // increment the index for the next iteration through the while loop
  }

#ifndef DISABLE_IDLE_SOL_SIG
  if (wowLeft == wowRight == wowNose == true) {
    digitalWrite(IDLE_SOL_SIG, HIGH);
  } else {
    digitalWrite(IDLE_SOL_SIG, LOW);
  }
#endif

#ifndef DISABLE_MAX_SOL_SIG
  if ((wowLeft == wowRight == wowNose == true) && ((launchBarExtended == true) || (arrestingHookUp == false))) {
    digitalWrite(MAX_SOL_SIG, LOW);  // With weight-on-wheels, if the lanuch bar is extended or the hook is down then raise afterburner lockout / detent to prevent inadvertent afterburner selection.
  } else {
    digitalWrite(MAX_SOL_SIG, HIGH);  // Otherwise pull the afterburner lockout / detent.
  }
#endif
}
