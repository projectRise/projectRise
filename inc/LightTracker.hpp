/*! LightTracker.hpp
    Light source tracker using photoresistors with vertical/horizontal servomotors.
    \file       LightTracker.hpp
    \authors    albrdev (albrdev@gmail.com)
    \date       2020-02-27
*/

#ifndef __LIGHTTRACKER_HPP__
#define __LIGHTTRACKER_HPP__

#include <stdint.h>
#include <Servo.h>

class LightTracker
{
private:
    uint8_t m_HorizontalMotorPin;
    uint8_t m_VerticalMotorPin;

    uint8_t m_ULPin;
    uint8_t m_URPin;
    uint8_t m_DLPin;
    uint8_t m_DRPin;

    int m_ActiveLightLevel;
    int m_Tolerance;

    int m_HoriztonalAngleMin;
    int m_HoriztonalAngleMax;
    int m_VerticalAngleMin;
    int m_VerticalAngleMax;

    Servo m_HorizontalMotor;
    Servo m_VerticalMotor;


public:
    int GetHorizontalPosition(void);
    int GetVerticalPosition(void);

    bool Poll(void);
    void Begin(void);

    LightTracker(uint8_t servoHPin, uint8_t servoVPin, uint8_t prULPin, uint8_t prURPin, uint8_t prDLPin, uint8_t prDRPin, int activeLightLevel = 250, int tolerance = 50, int angleHMargin = 15, int angleVMargin = 15);
};

#endif // __LIGHTTRACKER_HPP__
