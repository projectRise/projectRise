#ifndef __LIGHTTRACKER_HPP__
#define __LIGHTTRACKER_HPP__

#include <stdint.h>
#include <Servo.h>

class LightTracker
{
private:
    uint8_t m_HorizontalMotorPin;
    uint8_t m_VerticalMotorPin;

    Servo m_HorizontalMotor;
    Servo m_VerticalMotor;

    uint8_t m_ULPin;
    uint8_t m_URPin;
    uint8_t m_DLPin;
    uint8_t m_DRPin;

    int m_HorizontalMotorPosition;
    int m_VerticalMotorPosition;

    int m_Tolerance;

public:
    bool Poll(void);
    void Begin(void);

    LightTracker(uint8_t servoHPin, uint8_t servoVPin, uint8_t prULPin, uint8_t prURPin, uint8_t prDLPin, uint8_t prDRPin, int tolerance = 10);
};

#endif // __LIGHTTRACKER_HPP__
