// All Debug messages will be sent to BOTH: USB and Bluetooth

#include "hal.h"
#include "debug.h"
#include "motor.h"
#include "QTRSensors.h"
#include "battery.h"
#include "utils.h"
#include "ui.h"
#include "pid.h"
#include "dec2bin.h"


/////////////////////////////////////////////////////////////////////////
          int result[8];
unsigned char sensorByte;
      int lineNumber;
      int error;



int8_t left, right;
////////////////////////////////////////////////////////////////////////

void setup()
{
    init_GPIO();
    batteryVoltageCheck(PRINT); // Check
    
    DUMP_L1_a("\r Press the TOP Button to continue:"); // NOTE THE  USE OF \r ONLY (not \n)
    //waitForSwitchPress(SWITCH_TOP, PRINT);
    //DUMP_L1_a("[2J"); // CLEAR SCREEN COMMAND

    int i;

    DUMP_L1_a("\r\n===================================");
    
    delay(100);
    batteryVoltageCheck(PRINT); // Check
    delay(100);

    DUMP_L1_ab("\r\n NOTE 1:  FOR MOTOR PWM:    ANALOG_WRITE_RESOLUTION= ", ANALOG_WRITE_RESOLUTION);
    DUMP_L1_a ("\r\n NOTE 2:  Voltage BOOSTER output = 12 Volt");
    DUMP_L1_a ("\r\n          If you change Booster's voltage, then PID constants (Kp, Ki, Kd) needs to be changed");
    DUMP_L1_a ("\r\n");
    DUMP_L1_a ("\r\n                               ||");
    DUMP_L1_a ("\r\n Place the robot on a Line F--ollo--wer ARENA (White paper with a Black line-strip path)");
    DUMP_L1_a ("\r\n                               ||");
    DUMP_L1_a ("\r\n                               ||");

    DUMP_L1_a ("\r\n Press the MIDDEL Button to RUN the robot using PID algo:");
    waitForSwitchPress(SWITCH_MIDDLE, NO_PRINT); 
    //DUMP_L1_a("[2J"); // CLEAR SCREEN COMMAND

   
    delay(1000); // Remove your hand (from robot and switch), so that proper calibration can be done
    
       
    performCalibration(PRINT);

    
    
    // New readline() function as described by Pololu:
    // Manually slide the robot across the balck line to observe the readings
    while(1)
    {    
        getSensorArrayValuesCalibrated(&result[0], &sensorByte, NO_PRINT); 
        lineNumber = readLine(&result[0], sensorByte, NO_PRINT);
        error = lineNumber - 3500;
        DUMP_L1_abcd("\r\n lineNumber=", lineNumber, " Error=  ", error);
        delay(300);
    }




    delay(4000); // Reposition the robot on track if required. 
                 //Remove your hand (from robot and switch), so that proper calibration can be done

}
/////////////////////////////////////////////////////////////////////////

#define DRY_RUN 1

#define STANDARD_MOTOR_SPEED   10000
#define STANDARD_MOTOR_SPEED   10000

#define MAX_ALLOWED_SPEED      20000
#define MIN_ALLOWED_SPEED       9000

#define NUMBER_OF_ITERATIONS_PER_SECOND    50L
#define STOP_SIGN_MULTI_CHECK_SAMPLE_COUNT 20

const long LOOP_TIME_IN_MICRO_SECONDS = (1000000L / NUMBER_OF_ITERATIONS_PER_SECOND );

      int reallyOnBlackBlock =0; // Re-assurance count
      int previousError;

// Decide for your self
const int Kp =    2; // PID constants
const int Kd =    0;
const int Ki =    0;

      int pTerm;
      int iTerm = 0; // Initialize
      int dTerm;
      int outputPID;

      int leftMotorSpeed, rightMotorSpeed; // Motor Speeds

      unsigned int lcv=0; // loop count variable
      unsigned long timeStamp;
      unsigned long timeStampSpaceRemaining; // How much more room we have!

      char str[120];

      
void loop()
{

    // STEP: Note the current timeStamp in micro-seconds
    timeStamp = micros();

    // STEP: Get current position data   
   getSensorArrayValuesCalibrated(&result[0], &sensorByte, NO_PRINT); 
   lineNumber = readLine(&result[0], sensorByte, NO_PRINT);
    
    // STEP: Check if STOP BLOCK is detected :
    if(lineNumber == POSITION_OF_BLACK_BLOCK_DETECTED)
    {
        reallyOnBlackBlock++;
        if(reallyOnBlackBlock >= STOP_SIGN_MULTI_CHECK_SAMPLE_COUNT)
        {
            motionStop(); // Stop Motors
            digitalWrite(LED_SUCCESS_PIN_NO, HIGH);
            
            delay(100);
            DUMP_L1_a("\r\n  BLACK BLOCK DETECTED   STOP \r\n");
              
            left  = digitalRead(IR_LEFT_PIN_NO);
            right = digitalRead(IR_RIGHT_PIN_NO);

            drawLine(lineNumber, left, right, true);

            DUMP_L1_a ("\r\n Press the BOTTOM Button to IGNORE (or RESET button to Start Over)");
            beep(5, 400); // 
            batteryVoltageCheck(1); // Check

            iTerm = 0;
            
            waitForSwitchPress(SWITCH_BOTTOM, NO_PRINT);              
            //while(1);  // FINALLY HANG
            
        }

        // Wait for some timeStamp and then return
        while( (micros() - timeStamp) <  LOOP_TIME_IN_MICRO_SECONDS)
            ;
        return; // The loop() function returns for now, to be called again; further code is NOT executed    
    }
    reallyOnBlackBlock=0;

    // STEP:  In Maths, number-line have negative on left, positive on right
     error = lineNumber - 3500;
    //        DUMP_L1_abcd("\r\n lineNumber=", lineNumber, " Error=  ", error);    
    //error=0; // For Testing Motors at SET Max Speed: Ignores PID Algo (as Error=0)
    

    // STEP: Calculate Proportional Term
       pTerm = error * Kp;

    // STEP: Calculate Integral Term
       iTerm = iTerm + (error * Ki);

    // STEP: Calculate Derivative Term
       dTerm = (error -  previousError) * Kd;

    // STEP: Calculate P + I + D
    outputPID = pTerm + iTerm + dTerm;

    //outputPID = outputPID % 65536; // Motor speed 16 Bit resolution: The number will 
               
    leftMotorSpeed   = STANDARD_MOTOR_SPEED  - outputPID;
    rightMotorSpeed  = STANDARD_MOTOR_SPEED  + outputPID;

         if(leftMotorSpeed > MAX_ALLOWED_SPEED)  leftMotorSpeed = MAX_ALLOWED_SPEED;
    else if(leftMotorSpeed < MIN_ALLOWED_SPEED)  leftMotorSpeed = MIN_ALLOWED_SPEED;
    
         if(rightMotorSpeed > MAX_ALLOWED_SPEED) rightMotorSpeed = MAX_ALLOWED_SPEED;
    else if(rightMotorSpeed < MIN_ALLOWED_SPEED) rightMotorSpeed = MIN_ALLOWED_SPEED;
    
    if( DRY_RUN ==0) // If its an actural run then give commands to motor
        { 
            moveMotors( leftMotorSpeed, rightMotorSpeed, NO_PRINT);    

//////            if(lcv%100==0)
//////            {
//////            sprintf(str,"\r\n%4d E=%5d P=%6d I=%7d D=%6d O=%5d\t\tL=%3d R=%3d", 
//////                            lcv, error, pTerm, iTerm, dTerm, outputPID, leftMotorSpeed, rightMotorSpeed);
//////            DUMP_L1_a(str);
//////            }
        }

    previousError = error;
    
    // The above loop must always take a unit timeStamp to execute
    // For integration dt is unit timeStamp
    // We want Robot to correct its position 50 timeStamps per second
    // 1sec/50 counts = 1000ms/50 counts = 20ms/count or 20,000 uS/ count
    // 


//    timeStampSpaceRemaining=0;
//    while( (micros() - timeStamp) < LOOP_TIME_IN_MICRO_SECONDS ) // Consume full unit loop timeStamp [Pass/kill timeStamp, if extra]
//    {    
//        delayMicroseconds(1);
//        timeStampSpaceRemaining++;
//    }
//    if(timeStampSpaceRemaining < 10)
//    {
//
//       DUMP_L1_ab(" T= ", timeStampSpaceRemaining); 
//       while(1); // HANG
//    }


    // For perfect timeStamp keeping calculations:
    // There must be NO-Code after timeStamp count is done
    // There must be (almost) no (timeStamp-consuming-code) below this line for actual run 
    // For a DRY RUN we can write code after this line

    if( DRY_RUN==1)
        {
            sprintf(str,"\r\n%4d E=%5d P=%6d I=%7d D=%6d O=%5d\t\tL=%3d R=%3d", 
                            lcv, error, pTerm, iTerm, dTerm, outputPID, leftMotorSpeed, rightMotorSpeed);
            DUMP_L1_a(str);
            delay(800); // If its not an actual run, observe the data on Terminal

        }

    lcv++;
    batteryVoltageCheck(NO_PRINT); // Check
}

/////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////



