#include "WeatherShield.hpp"
#include <Arduino.h>

float WeatherShield::GetTemperature(void)
{
    return m_HumiditySensor.getTemp();
}

float WeatherShield::GetHumidity(void)
{
    return m_HumiditySensor.getRH();
}

float WeatherShield::GetPressure(void)
{
    return m_PressureSensor.readPressure();
}

// Returns the voltage of the light sensor based on the 3.3V rail
// This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
float WeatherShield::GetLightLevel(void)
{
    float operatingVoltage = analogRead(m_VREFPin);
    float lightSensor = analogRead(m_LightSensorPin);

    operatingVoltage = m_VREFVoltage / operatingVoltage;    // The reference voltage is 3.3V
    lightSensor = operatingVoltage * lightSensor;

    return lightSensor / m_VREFVoltage;
}

// Returns the voltage of the raw pin based on the 3.3V rail
// This allows us to ignore what VCC might be (an Arduino plugged into USB has VCC of 4.5 to 5.2V)
// Battery level is connected to the RAW pin on Arduino and is fed through two 5% resistors:
// 3.9K on the high side (R1), and 1K on the low side (R2)
float WeatherShield::GetBatteryLevel(void)
{
    float operatingVoltage = analogRead(m_VREFPin);
    float rawVoltage = analogRead(m_BatterySensorPin);

    operatingVoltage = m_VREFVoltage / operatingVoltage;    // The reference voltage is 3.3V
    rawVoltage = operatingVoltage * rawVoltage;             // Convert the 0 to 1023 int to actual voltage on PIN_BATTERYSENSOR pin
    rawVoltage *= 4.9;                                      // (3.9k+1k)/1k - multiple PIN_BATTERYSENSOR voltage by the voltage divider to get actual system voltage

    return rawVoltage;
}

bool WeatherShield::Begin(void)
{
    pinMode(m_VREFPin, INPUT);
    pinMode(m_LightSensorPin, INPUT);
    pinMode(m_BatterySensorPin, INPUT);

    // Configure the pressure sensor
    m_PressureSensor.begin();
    m_PressureSensor.setModeBarometer();    // Measure pressure in Pascals from 20 to 110 kPa
    m_PressureSensor.setOversampleRate(7);  // Set Oversample to the recommended 128
    m_PressureSensor.enableEventFlags();    // Enable all three pressure and temp event flags

    // Configure the humidity sensor
    m_HumiditySensor.begin();
    return GetHumidity() != 998.0f && GetPressure() >= 0.0f;
}

WeatherShield::WeatherShield(uint8_t vrefPin, uint8_t lightSensorPin, uint8_t batterySensorPin, float vrefVoltage) : m_VREFPin(vrefPin), m_LightSensorPin(lightSensorPin), m_BatterySensorPin(batterySensorPin), m_VREFVoltage(vrefVoltage) { }
