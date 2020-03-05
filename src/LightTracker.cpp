#include "LightTracker.hpp"
#include <Arduino.h>

#define SERVO_ANGLE_MIN 0
#define SERVO_ANGLE_MAX 180

int LightTracker::GetHorizontalPosition(void)
{
    return m_HorizontalMotor.read();
}

int LightTracker::GetVerticalPosition(void)
{
    return m_VerticalMotor.read();
}

bool LightTracker::Poll(void)
{
    int valueUL = analogRead(m_ULPin);  // top left
    int valueUR = analogRead(m_URPin);  // top right
    int valueDL = analogRead(m_DLPin);  // down left
    int valueDR = analogRead(m_DRPin);  // down right

    if(valueUL < m_ActiveLightLevel && valueUR < m_ActiveLightLevel && valueDL < m_ActiveLightLevel && valueDR < m_ActiveLightLevel)
    {
        if(m_VerticalMotor.attached())
        {
            m_VerticalMotor.detach();
        }

        if(m_HorizontalMotor.attached())
        {
            m_HorizontalMotor.detach();
        }

        return false;
    }

    int avgU = (valueUL + valueUR) / 2; // average value top
    int avgD = (valueDL + valueDR) / 2; // average value down
    int avgL = (valueUL + valueDL) / 2; // average value left
    int avgR = (valueUR + valueDR) / 2; // average value right

    int diffV = avgU - avgD;
    int diffH = avgL - avgR;

    bool isVUpdating = abs(diffV) >= m_Tolerance;
    if(isVUpdating)
    {
        int position = GetVerticalPosition();
        int dir = 0;
        if(diffV > 0 && position < m_VerticalAngleMax)
        {
            dir++;
        }
        else if(diffV < 0 && position > m_VerticalAngleMin)
        {
            dir--;
        }

        isVUpdating = dir != 0;
        if(isVUpdating)
        {
            if(!m_VerticalMotor.attached())
            {
                m_VerticalMotor.attach(m_VerticalMotorPin);
            }

            m_VerticalMotor.write(position + dir);
        }
    }

    bool isHUpdating = abs(diffH) >= m_Tolerance;
    if(isHUpdating)
    {
        int position = GetHorizontalPosition();
        int dir = 0;
        if(diffH < 0 && position > m_HoriztonalAngleMin)
        {
            dir--;
        }
        else if(diffH > 0 && position < m_HoriztonalAngleMax)
        {
            dir++;
        }

        isHUpdating = dir != 0;
        if(isHUpdating)
        {
            if(!m_HorizontalMotor.attached())
            {
                m_HorizontalMotor.attach(m_HorizontalMotorPin);
            }

            m_HorizontalMotor.write(position + dir);
        }
    }

    if(!isVUpdating && m_VerticalMotor.attached())
    {
        m_VerticalMotor.detach();
    }

    if(!isHUpdating && m_HorizontalMotor.attached())
    {
        m_HorizontalMotor.detach();
    }

    return isVUpdating || isHUpdating;
}

void LightTracker::Begin(void)
{
    m_VerticalMotor.attach(m_VerticalMotorPin);
    m_HorizontalMotor.attach(m_HorizontalMotorPin);

    pinMode(m_ULPin, INPUT);
    pinMode(m_URPin, INPUT);
    pinMode(m_DLPin, INPUT);
    pinMode(m_DRPin, INPUT);
}

LightTracker::LightTracker(uint8_t hPin, uint8_t vPin, uint8_t ulPin, uint8_t urPin, uint8_t dlPin, uint8_t drPin, int activeLightLevel, int tolerance, int angleHMargin, int angleVMargin) : m_HorizontalMotorPin(hPin), m_VerticalMotorPin(vPin), m_ULPin(ulPin), m_URPin(urPin), m_DLPin(dlPin), m_DRPin(drPin), m_ActiveLightLevel(activeLightLevel), m_Tolerance(tolerance), m_HoriztonalAngleMin(SERVO_ANGLE_MIN + angleHMargin), m_HoriztonalAngleMax(SERVO_ANGLE_MAX - angleHMargin), m_VerticalAngleMin(SERVO_ANGLE_MIN + angleVMargin), m_VerticalAngleMax(SERVO_ANGLE_MAX - angleVMargin) { }
