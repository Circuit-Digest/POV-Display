/*
 * Project Name: POV display 
 * Project Brief: Firmware for ESP32 POV Display. Display resolution 128 pixels
 * Author: Jobit Joseph
 * Copyright © Jobit Joseph
 * Copyright © Semicon Media Pvt Ltd
 * Copyright © Circuitdigest.com
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, in version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <Arduino.h>
#include <MultiShiftRegister.h>
#include "Images.h"
#include "Precompute.h"

// Pin Definitions
#define latchPin 14
#define clockPin 15
#define dataPin 13
#define latchPinx 17
#define clockPinx 18
#define dataPinx 16
#define HALL_SENSOR1_PIN 36
#define HALL_SENSOR2_PIN 39

// Global Variables
volatile unsigned long lastHallTrigger = 0;
volatile float rotationTime = 0;  // Time for one rotation in milliseconds
int hallSensor1State = 0;
int hallSensor2State = 0;
bool halfFrame = false;
int numberOfRegisters = 8;
int offset = 270;
int repeatvalue = 1;
int hys = 3000;
int frame = 0;
int repeat = 0;
int anim = 0;
int frameHoldTime = 1;     // Number of loops to hold each frame
int frameHoldCounter = 0;  // Counter to track loops for current frame

//Shift register Driver instances for both hands
MultiShiftRegister msr(numberOfRegisters, latchPin, clockPin, dataPin);
MultiShiftRegister msrx(numberOfRegisters, latchPinx, clockPinx, dataPinx);

//Function to calculate polar co-ordintes, and get corresponding data from the arrays.
int getValueFromAngle(const uint8_t arrayName[][16], int angle, int radius) {
  // Adjust the angle by subtracting offset to rotate counter-clockwise
  int adjustedAngle = angle - offset;
  if (adjustedAngle < 0) adjustedAngle += 360;  // Ensure the angle stays within 0-359 degrees

  // Invert the targetX calculation to flip the image horizontally
  int targetX = 127 - precomputedCos[radius][adjustedAngle];  // Flipping targetX
  int targetY = precomputedSin[radius][adjustedAngle];
  if (targetX >= 0 && targetX < 128 && targetY >= 0 && targetY < 128) {
    int byteIndex = targetX / 8;
    int bitIndex = 7 - (targetX % 8);
    return (arrayName[targetY][byteIndex] >> bitIndex) & 1;  // Extract the bit value and return it
  } else {
    return -1;  // Out of bounds
  }
}

//Hall sensor 1 interrupt routine
void ISR_HallSensor1() {
  unsigned long currentTime = micros();
  // Check if HYS ms have passed since the last trigger
  if (currentTime - lastHallTrigger >= hys) {
    rotationTime = (currentTime - lastHallTrigger) / 1000.0;
    lastHallTrigger = currentTime;
    hallSensor1State = 1;
    halfFrame = true;
  }
}

//Hall sensor 2 interrupt routine
void ISR_HallSensor2() {
  unsigned long currentTime = micros();
  // Check if HYS ms have passed since the last trigger
  if (currentTime - lastHallTrigger >= hys) {
    lastHallTrigger = currentTime;
    hallSensor2State = 1;
    halfFrame = false;
  }
}

//Function to calculate RPM and display each frame accordingly 
void DisplayFrame(const uint8_t ImageName[][16]) {
  float timePerSegment = rotationTime / 360.0;  // Assuming 180 segments per half rotation

  for (int i = 0; i < 180; i++) {
    unsigned long segmentStartTime = micros();

    for (int j = 0; j < 64; j++) {
      // First arm (msr) displays the first half of the frame
      if (getValueFromAngle(ImageName, i + (halfFrame ? 0 : 180), j)) {
        msr.set(j);
      } else {
        msr.clear(j);
      }

      // Second arm (msrx) displays the second half of the frame
      if (getValueFromAngle(ImageName, i + (halfFrame ? 180 : 0), j)) {
        msrx.set(j);
      } else {
        msrx.clear(j);
      }
    }

    msr.shift();
    msrx.shift();

    while (micros() - segmentStartTime < timePerSegment * 1000)
      ;
  }
}

void setup() {
  // Initialize pins
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(latchPinx, OUTPUT);
  pinMode(clockPinx, OUTPUT);
  pinMode(dataPinx, OUTPUT);
  pinMode(HALL_SENSOR1_PIN, INPUT);
  pinMode(HALL_SENSOR2_PIN, INPUT);

  attachInterrupt(digitalPinToInterrupt(HALL_SENSOR1_PIN), ISR_HallSensor1, RISING);
  attachInterrupt(digitalPinToInterrupt(HALL_SENSOR2_PIN), ISR_HallSensor2, RISING);
  for (int i = 0; i < 64; i++) {
    msr.clear(i);
    msrx.clear(i);
  }
  msr.shift();
  msrx.shift();
  Serial.begin(115200);
}

void loop() {
  if (hallSensor1State || hallSensor2State) {
    if (anim == 0) {
      DisplayFrame(CDArrays[frame]);
      // Increment the counter
      frameHoldCounter++;
      // Check if it's time to move to the next frame
      if (frameHoldCounter >= frameHoldTime) {
        // Reset the counter
        frameHoldCounter = 0;
        // Move to the next frame
        frame++;
        if (frame > 21) {
          frame = 0;
          repeat++;
          if (repeat == 1) {
            repeat = 0;
            anim++;
            frameHoldTime = 1;
            repeatvalue = 1;
          }
        }
      }
    }
    if (anim == 1) {
      DisplayFrame(ImageCD_22);
      // Increment the counter
      frameHoldCounter++;
      // Check if it's time to move to the next frame
      if (frameHoldCounter >= frameHoldTime) {
        // Reset the counter
        frameHoldCounter = 0;
        // Move to the next frame
        frame++;
        if (frame > 50) {
          frame = 0;
          repeat++;
          if (repeat == repeatvalue) {
            repeat = 0;
            anim++;
            frameHoldTime = 2;
            repeatvalue = 1;
          }
        }
      }
    }
    if (anim == 2) {
      DisplayFrame(ViasionArrays[frame]);
      // Increment the counter
      frameHoldCounter++;
      // Check if it's time to move to the next frame
      if (frameHoldCounter >= frameHoldTime) {
        // Reset the counter
        frameHoldCounter = 0;
        // Move to the next frame
        frame++;
        if (frame > 21) {
          frame = 0;
          repeat++;
          if (repeat == repeatvalue) {
            repeat = 0;
            anim++;
            frameHoldTime = 1;
            repeatvalue = 1;
          }
        }
      }
    }
    if (anim == 3) {
      DisplayFrame(Image_Viasion22);
      // Increment the counter
      frameHoldCounter++;
      // Check if it's time to move to the next frame
      if (frameHoldCounter >= frameHoldTime) {
        // Reset the counter
        frameHoldCounter = 0;
        // Move to the next frame
        frame++;
        if (frame > 50) {
          frame = 0;
          repeat++;
          if (repeat == repeatvalue) {
            repeat = 0;
            anim++;
            frameHoldTime = 2;
            repeatvalue = 1;
          }
        }
      }
    }
    if (anim == 4) {
      DisplayFrame(ViasionOTRArrays[frame]);
      // Increment the counter
      frameHoldCounter++;
      // Check if it's time to move to the next frame
      if (frameHoldCounter >= frameHoldTime) {
        // Reset the counter
        frameHoldCounter = 0;
        // Move to the next frame
        frame++;
        if (frame > 11) {
          frame = 0;
          repeat++;
          if (repeat == repeatvalue) {
            repeat = 0;
            anim++;
            frameHoldTime = 1;
            repeatvalue = 10;
          }
        }
      }
    }
    if (anim == 5) {
      DisplayFrame(CatRunArray[frame]);
      // Increment the counter
      frameHoldCounter++;
      // Check if it's time to move to the next frame
      if (frameHoldCounter >= frameHoldTime) {
        // Reset the counter
        frameHoldCounter = 0;
        // Move to the next frame
        frame++;
        if (frame > 9) {
          frame = 0;
          repeat++;
          if (repeat == repeatvalue) {
            repeat = 0;
            anim++;
            frameHoldTime = 2;
            repeatvalue = 2;
          }
        }
      }
    } else if (anim == 6) {
      DisplayFrame(CatArrays[frame]);
      // Increment the counter
      frameHoldCounter++;
      // Check if it's time to move to the next frame
      if (frameHoldCounter >= frameHoldTime) {
        // Reset the counter
        frameHoldCounter = 0;
        // Move to the next frame
        frame++;
        if (frame > 61) {
          frame = 0;
          repeat++;
          if (repeat == repeatvalue) {
            repeat = 0;
            anim++;
            frameHoldTime = 2;
            repeatvalue = 3;
          }
        }
      }
    } else if (anim == 7) {
      DisplayFrame(RunningSFArrays[frame]);
      // Increment the counter
      frameHoldCounter++;
      // Check if it's time to move to the next frame
      if (frameHoldCounter >= frameHoldTime) {
        // Reset the counter
        frameHoldCounter = 0;
        // Move to the next frame
        frame++;
        if (frame > 11) {
          frame = 0;
          repeat++;
          if (repeat == repeatvalue) {
            repeat = 0;
            anim++;
            frameHoldTime = 2;
            repeatvalue = 1;
          }
        }
      }
    } else if (anim == 8) {
      DisplayFrame(Dance1Arrays[frame]);
      // Increment the counter
      frameHoldCounter++;
      // Check if it's time to move to the next frame
      if (frameHoldCounter >= frameHoldTime) {
        // Reset the counter
        frameHoldCounter = 0;
        // Move to the next frame
        frame++;
        if (frame > 93) {
          frame = 0;
          repeat++;
          if (repeat == repeatvalue) {
            repeat = 0;
            anim++;
            frameHoldTime = 1;
            repeatvalue = 4;
          }
        }
      }
    } else if (anim == 9) {
      DisplayFrame(EYEArrays[frame]);  // Increment the counter
      frameHoldCounter++;
      // Check if it's time to move to the next frame
      if (frameHoldCounter >= frameHoldTime) {
        // Reset the counter
        frameHoldCounter = 0;
        // Move to the next frame
        frame++;
        if (frame > 73) {
          frame = 0;
          repeat++;
          if (repeat == repeatvalue) {
            repeat = 0;
            anim++;
            frameHoldTime = 2;
            repeatvalue = 4;
          }
        }
      }
    } else if (anim == 10) {
      DisplayFrame(GlobexArrays[frame]);  // Increment the counter
      frameHoldCounter++;
      // Check if it's time to move to the next frame
      if (frameHoldCounter >= frameHoldTime) {
        // Reset the counter
        frameHoldCounter = 0;
        // Move to the next frame
        frame++;
        if (frame > 14) {
          frame = 0;
          repeat++;
          if (repeat == repeatvalue) {
            repeat = 0;
            anim++;
            frameHoldTime = 20;
            repeatvalue = 1;
          }
        }
      }
    } else if (anim == 11) {
      DisplayFrame(ClockArrays[frame]);  // Increment the counter
      frameHoldCounter++;
      // Check if it's time to move to the next frame
      if (frameHoldCounter >= frameHoldTime) {
        // Reset the counter
        frameHoldCounter = 0;
        // Move to the next frame
        frame++;
        if (frame > 13) {
          frame = 0;
          repeat++;
          if (repeat == repeatvalue) {
            repeat = 0;
            anim = 0;
            frameHoldTime = 2;
            repeatvalue = 1;
          }
        }
      }
    }
    hallSensor1State = 0;
    hallSensor2State = 0;
  }
}
