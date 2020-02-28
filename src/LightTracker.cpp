#include "LightTracker.hpp"
#include <Arduino.h>

#define clamp(x, a, b) ((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))  // x < a ? a : (x > b ? b : x)

#define SERVO_VMIN  15
#define SERVO_VMAX  90

#define SERVO_HMIN  15
#define SERVO_HMAX  165

void LightTracker::Poll(void)
{
    int valueUL = analogRead(m_ULPin);  // top left
    int valueUR = analogRead(m_URPin);  // top right
    int valueDL = analogRead(m_DLPin);  // down left
    int valueDR = analogRead(m_DRPin);  // down right
    //int dtime = analogRead(m_HorizontalMotorPin) / 20;     // read potentiometers
    //int tol = analogRead(m_VerticalMotorPin) / 4;

    int avgU = (valueUL + valueUR) / 2; // average value top
    int avgD = (valueDL + valueDR) / 2; // average value down
    int avgL = (valueUL + valueDL) / 2; // average value left
    int avgR = (valueUR + valueDR) / 2; // average value right

    //int diffV = avgU - avgD;            // check the diffirence of up and down
    //int diffH = avgL - avgR;            // check the diffirence og left and right

    int diffV = avgU - avgD;
    int diffH = avgL - avgR;
    if(abs(diffV) > m_Tolerance)
    {
        if(diffV > 0)
        {
            m_VerticalMotorPosition++;
        }
        else if(diffV < 0)
        {
            m_VerticalMotorPosition--;
        }

        m_VerticalMotorPosition = clamp(m_VerticalMotorPosition, SERVO_VMIN, SERVO_VMAX);
        m_VerticalMotor.write(m_VerticalMotorPosition);
    }

    if(abs(diffH) > m_Tolerance)
    {
        if(diffH < 0)
        {
            m_HorizontalMotorPosition--;
        }
        else if(diffH > 0)
        {
            m_HorizontalMotorPosition++;
        }

        m_HorizontalMotorPosition = clamp(m_HorizontalMotorPosition, SERVO_HMIN, SERVO_HMAX);
        m_HorizontalMotor.write(m_HorizontalMotorPosition);
    }
}

void LightTracker::Begin(void)
{
    m_VerticalMotor.attach(m_VerticalMotorPin);
    m_HorizontalMotor.attach(m_HorizontalMotorPin);

    m_VerticalMotorPosition = m_VerticalMotor.read();
    m_HorizontalMotorPosition = m_HorizontalMotor.read();

    pinMode(m_ULPin, INPUT);
    pinMode(m_URPin, INPUT);
    pinMode(m_DLPin, INPUT);
    pinMode(m_DRPin, INPUT);
}

LightTracker::LightTracker(uint8_t hPin, uint8_t vPin, uint8_t ulPin, uint8_t urPin, uint8_t dlPin, uint8_t drPin, int tolerance) : m_HorizontalMotorPin(hPin), m_VerticalMotorPin(vPin), m_ULPin(ulPin), m_URPin(urPin), m_DLPin(dlPin), m_DRPin(drPin), m_Tolerance(tolerance) { }
