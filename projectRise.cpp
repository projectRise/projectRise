/*
 Weather Shield Example
 By: Nathan Seidle
 SparkFun Electronics
 Date: June 10th, 2016
 License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

 This example prints the current humidity, air pressure, temperature and light levels.

 The weather shield is capable of a lot. Be sure to checkout the other more advanced examples for creating
 your own weather station.

 Updated by Joel Bartlett
 03/02/2017
 Removed HTU21D code and replaced with Si7021
 */

#include <Wire.h> //I2C needed for sensors
#include "SparkFunMPL3115A2.h" //Pressure sensor - Search "SparkFun MPL3115" and install from Library Manager
#include "SparkFun_Si7021_Breakout_Library.h" //Humidity sensor - Search "SparkFun Si7021" and install from Library Manager
#include <SPI.h>
#include "SdFat.h"
#include "LowPower.h"

SdFat SD;
#define SD_CS_PIN SS
File myFile;

MPL3115A2 myPressure; //Create an instance of the pressure sensor
Weather myHumidity;//Create an instance of the humidity sensor

//Hardware pin definitions
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
const byte STAT_BLUE = 7;
const byte STAT_GREEN = 8;

const byte REFERENCE_3V3 = A3;
const byte LIGHT = A1;
const byte BATT = A2;

//Global Variables
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
long lastSecond; //The millis counter to see when a second rolls by

float get_battery_level();
float get_light_level();

void setup()
{
    Serial.begin(115200);
    //Leds that show status on board
    pinMode(24, OUTPUT);
    pinMode(25, OUTPUT);
    pinMode(27, OUTPUT);
    while (!Serial) {
        ; // wait for serial port to connect. Needed for native USB port only
    }

    Serial.print("Initializing SD card...");

    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("initialization failed!");
        return;
    }
    Serial.println("initialization done.");

    Serial.println("Weather Shield Example");

    pinMode(STAT_BLUE, OUTPUT); //Status LED Blue
    pinMode(STAT_GREEN, OUTPUT); //Status LED Green

    pinMode(REFERENCE_3V3, INPUT);
    pinMode(LIGHT, INPUT);

    //Configure the pressure sensor
    myPressure.begin(); // Get sensor online
    myPressure.setModeBarometer(); // Measure pressure in Pascals from 20 to 110 kPa
    myPressure.setOversampleRate(7); // Set Oversample to the recommended 128
    myPressure.enableEventFlags(); // Enable all three pressure and temp event flags

    //Configure the humidity sensor
    myHumidity.begin();

    lastSecond = millis();

    Serial.println("Weather Shield online!");
}
    // Enter idle state for 8 s with the rest of peripherals turned off
    // Each microcontroller comes with different number of peripherals
    // Comment off line of code where necessary

    // ATmega328P, ATmega168
    //LowPower.idle(SLEEP_8S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, 
    //              SPI_OFF, USART0_OFF, TWI_OFF);

    // ATmega32U4
    //LowPower.idle(SLEEP_8S, ADC_OFF, TIMER4_OFF, TIMER3_OFF, TIMER1_OFF, 
    //		  TIMER0_OFF, SPI_OFF, USART1_OFF, TWI_OFF, USB_OFF);

    // ATmega2560
    //LowPower.idle(SLEEP_2S, ADC_OFF, TIMER5_OFF, TIMER4_OFF, TIMER3_OFF, 
    //        TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART3_OFF, 
    //        USART2_OFF, USART1_OFF, USART0_OFF, TWI_OFF);

    // ATmega256RFR2
    //LowPower.idle(SLEEP_8S, ADC_OFF, TIMER5_OFF, TIMER4_OFF, TIMER3_OFF, 
    //      TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF,
    //      USART1_OFF, USART0_OFF, TWI_OFF);
        //Print readings every second

void loop()
{
    if (millis() - lastSecond >= 1000)
    {
        //Prints before the LowPower.powerSave() funtion wont run.
        // ATmega2560
        digitalWrite(25, LOW);
        digitalWrite(27, HIGH);
        LowPower.powerSave(SLEEP_8S, ADC_OFF, BOD_OFF, TIMER2_OFF);
        Serial.println("Wake up");
        digitalWrite(27, LOW);
        digitalWrite(25, LOW);
        digitalWrite(24, HIGH);
        //digitalWrite(STAT_BLUE, HIGH); //Blink stat LED

        lastSecond += 1000;

        //Check Humidity Sensor
        float humidity = myHumidity.getRH();

        if (humidity == 998) //Humidty sensor failed to respond
        {
            Serial.println("I2C communication to sensors is not working. Check solder connections.");

            //Try re-initializing the I2C comm and the sensors
            myPressure.begin();
            myPressure.setModeBarometer();
            myPressure.setOversampleRate(7);
            myPressure.enableEventFlags();
            myHumidity.begin();
        }
        else
        {
            Serial.print("Humidity = ");
            Serial.print(humidity);
            Serial.print("%,");
            float temp_h = myHumidity.getTemp();
            Serial.print(" temp_h = ");
            Serial.print(temp_h, 2);
            Serial.print("C,");

            //Check Pressure Sensor
            float pressure = myPressure.readPressure();
            Serial.print(" Pressure = ");
            Serial.print(pressure);
            Serial.print("Pa,");

            /*
                  //Check tempf from pressure sensor
                  float tempf = myPressure.readTempF();
                  Serial.print(" temp_p = ");
                  Serial.print(tempf, 2);
                  Serial.print("F,");
            */
            //Check light sensor
            float light_lvl = get_light_level();
            Serial.print(" light_lvl = ");
            Serial.print(light_lvl);
            Serial.print("V,");

            //Check batt level
            float batt_lvl = get_battery_level();
            Serial.print(" VinPin = ");
            Serial.print(batt_lvl);
            Serial.print("V");

            Serial.println();

            // open the file. note that only one file can be open at a time,
            // so you have to close this one before opening another.
            myFile = SD.open("weather.txt", FILE_WRITE);

            // if the file opened okay, write to it:
            if (myFile) {
                Serial.print("Writing to test.txt...");
                myFile.print("Humidity = ");
                myFile.print(humidity);
                myFile.print("%,");
                myFile.print(" temp_h = ");
                myFile.print(temp_h, 2);
                myFile.print("C,");

                myFile.print(" Pressure = ");
                myFile.print(pressure);
                myFile.print("Pa,");

                myFile.print(" light_lvl = ");
                myFile.print(light_lvl);
                myFile.print("V,");

                myFile.print(" VinPin = ");
                myFile.print(batt_lvl);
                myFile.print("V");

                myFile.println();

                // close the file:
                myFile.close();
                Serial.println("done.");
            }
            else {
                // if the file didn't open, print an error:
                Serial.println("error opening test.txt");
            }
        }
        digitalWrite(24, LOW);
        digitalWrite(25, HIGH);
        // digitalWrite(STAT_BLUE, LOW); //Turn off stat LED
    }
    //delay(2000);
    //Serial.println("Sleep");
}

//Returns the voltage of the light sensor based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
float get_light_level()
{
    float operatingVoltage = analogRead(REFERENCE_3V3);

    float lightSensor = analogRead(LIGHT);

    operatingVoltage = 3.3 / operatingVoltage; //The reference voltage is 3.3V

    lightSensor = operatingVoltage * lightSensor;

    return (lightSensor);
}

//Returns the voltage of the raw pin based on the 3.3V rail
//This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
//Battery level is connected to the RAW pin on Arduino and is fed through two 5% resistors:
//3.9K on the high side (R1), and 1K on the low side (R2)
float get_battery_level()
{
    float operatingVoltage = analogRead(REFERENCE_3V3);

    float rawVoltage = analogRead(BATT);

    operatingVoltage = 3.30 / operatingVoltage; //The reference voltage is 3.3V

    rawVoltage = operatingVoltage * rawVoltage; //Convert the 0 to 1023 int to actual voltage on BATT pin

    rawVoltage *= 4.90; //(3.9k+1k)/1k - multiple BATT voltage by the voltage divider to get actual system voltage

    return (rawVoltage);
}