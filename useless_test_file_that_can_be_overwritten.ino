#include <uStepperS.h>
#include "SerialChecker.h"
#include "MilliTimer.h"

SerialChecker scpc(Serial);

uStepperS stepper;

float velocity = 100;
float maxvelocity = 300;
float angle = 0;
float distance = 0;

bool upstop = true;
bool upstopOld = true;
bool downstop = true;
bool downstopOld = true;

bool topLED = false;
bool botLED = false;

bool direc = false;   //0 for upward, 1 for downward
bool direcOld = false;

MilliTimer DebounceTimer(25);
MilliTimer ZeroTimeout(15000);

enum class states{
  IDLE,
  ZEROING,
  MOVING
};

states motorstate = states::IDLE;

void zeroing(bool reset = false);

void setup() {
  scpc.init();
  scpc.setMsgMinLen(1);
  scpc.setMsgMaxLen(64);
  scpc.println("yo1");
  Serial.println("yo2");
}

void loop() {
  checkEndStops();
  checkSerialPC();

  switch(motorstate){
    case states::IDLE:
      //do nothing
      break;
    case states::ZEROING:
      zeroing();
      break;
    case states::MOVING:
      // photodiode scan function
      if(stepper.getMotorState() == 0){
        motorstate = states::IDLE;
        scpc.println("bouncymotor returned to idle");
        scpc.print("Angle: ");
        scpc.println(stepper.encoder.getAngle());
        scpc.print("Microsteps: ");
        scpc.println(stepper.driver.getPosition());
      }
      break;
  }
}

void zeroing(bool reset = false){
  // 1. If off the endstop, move up at speed X = maxvelocity
  // 2. Check for endstop hit. If true, Move down some steps.
  // 3. If moved off the endstop and stopped, move up at speed X = __ until top end stop hit.
  // 4. Zero the motor position counter.
  static uint8_t zStep = 1;
  if(ZeroTimeout.timedOut(true)){
    zStep = 1;
    direc = direcOld;
  }
  if(reset){
    zStep = 1;
  }
  upstop = digitalRead(7);
  switch(zStep){
    case 1:
      // check we haven't already hit the endstop
      if(upstop == true){
        direc = 0;
        stepper.setMaxVelocity(maxvelocity);
        stepper.runContinous(0);
      }
      zStep++;
      break;
    case 2:
      if(upstop == false){
        direc = 1;
        // make motor start moving 500 steps downwards
        stepper.setMaxVelocity(50);
        stepper.moveSteps(12000);
        ZeroTimeout.reset();
        zStep++;
      }
      break;
    case 3:
      if((stepper.getMotorState() == 0) && (upstop == true)){
        direc = 0;
        stepper.setMaxVelocity(25);
        stepper.moveSteps(-20000);
        ZeroTimeout.reset();
        zStep++;      
      }
      break;
    case 4:
      if(upstop == false){
        stepper.stop(HARD);
        stepper.encoder.setHome();
        ZeroTimeout.reset();
        zStep = 1;
        direc = 1;
        motorstate = states::IDLE;
        scpc.println("Zeroing complete.");
      }
      break;
  }
}

void checkSerialPC(){
  if(scpc.check()){
    if(scpc.contains("kek")){
      scpc.println("nice work everyone!"); 
    }
  }
  else if(scpc.contains("ze")){ // zero the stage to top microswitch
    direcOld = direc;
    zeroing(true);
    motorstate = states::ZEROING;
    scpc.println("Zeroing...");
  } 
  else if(scpc.contains("su")){ // set direc to up
    if(direc == 1){
      direc = 0;
      scpc.println("direction set to: up");
      if(motorstate == states::MOVING){
        stepper.stop(HARD);
        stepper.runContinous(direc);
      }
    }
    else if(direc == 0){
      scpc.println("Already moving upward.");
    }
  }
  else if(scpc.contains("sd")){ // set direc to down
    if(direc == 0){
      direc = 1;
      scpc.println("direction set to: down");
      if(motorstate == states::MOVING){
        stepper.stop(HARD);
        stepper.runContinous(direc);
      }
    }
    else if(direc == 1){
      scpc.println("Already moving downward.");
    }
  }
  else if(scpc.contains("ms")){
    motorstate = states::MOVING;
    if(direc == 0){
      stepper.moveSteps(-51200);
    }
    else if(direc == 1){
      stepper.moveSteps(51200);
    }
  } 
}

void checkEndStops(){
  if(DebounceTimer.timedOut(true)){
    DebounceTimer.reset();

    upstop = digitalRead(7);
    downstop = digitalRead(8);
    
    if(upstop != upstopOld){
      stopMotorTop();
    }
    upstopOld = upstop;
 
    if(downstop != downstopOld){
      stopMotorBot();
    }
    downstopOld = downstop;
  }
}

void stopMotorTop(){
  if(direc == 0){
    stepper.stop(HARD);
    topLED = true;
    if(motorstate == states::MOVING){
      motorstate = states::IDLE;
    }
  }
  topLED = false;
}

void stopMotorBot(){
  if(direc == 1){
    stepper.stop(HARD);
    botLED = true;     //change to digitalwrite later?
    if(motorstate == states::MOVING){
      motorstate = states::IDLE;
    }
  }
  botLED = false;
}
