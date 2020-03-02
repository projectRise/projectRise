#ifndef __WEATHERSHIELD_HPP__
#define __WEATHERSHIELD_HPP__

#include <stdint.h>
#include <Wire.h> //I2C needed for sensors
#include "SparkFunMPL3115A2.h" //Pressure sensor - Search "SparkFun MPL3115" and install from Library Manager
#include "SparkFun_Si7021_Breakout_Library.h" //Humidity sensor - Search "SparkFun Si7021" and install from Library Manager

class WeatherShield
{
private:
    uint8_t m_VREFPin;
    uint8_t m_LightSensorPin;
    uint8_t m_BatterySensorPin;

    float m_VREFVoltage;

    MPL3115A2 m_PressureSensor;
    Weather m_HumiditySensor;

public:
    float GetTemperature(void);
    float GetHumidity(void);
    float GetPressure(void);
    float GetLightLevel(void) const;
    float GetBatteryLevel(void) const;

    bool Begin(void);

    WeatherShield(uint8_t vrefPin, uint8_t lightSensorPin, uint8_t batterySensorPin, float vrefVoltage = 3.3f);
};

#endif // __WEATHERSHIELD_HPP__
