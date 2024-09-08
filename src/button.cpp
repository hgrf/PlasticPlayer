#include "button.h"

#include <Arduino.h>

static std::list<Button *> sButtons;

Button::Button(int pin) : mPin(pin), mState(HIGH), mLastState(HIGH), mLastDebounceTime(0)
{
}

void Button::begin()
{
    pinMode(mPin, INPUT_PULLUP);
    mLastState = digitalRead(mPin);
    sButtons.push_back(this);
    attachInterrupt(mPin, isr, CHANGE);
}

void Button::process()
{
    int reading = digitalRead(mPin);

    if (reading == mLastState)
    {
        return;
    }

    if ((millis() - mLastDebounceTime) > DEBOUNCE_DELAY)
    {
        if (reading != mState)
        {
            mState = reading;

            std::lock_guard<std::mutex> lock(mMutex);
            mQueue.push(reading == LOW ? PRESS : RELEASE);
            mCondition.notify_one();
        }
    }

    // NOTE: this means that the first state change out of the debounce period is taken into account,
    //       it should be the last one
    mLastDebounceTime = millis();
    mLastState = reading;
}

Button::ButtonEvent Button::waitForEvent(void)
{
    std::unique_lock<std::mutex> lock(mMutex);
    while (mQueue.empty())
    {
        mCondition.wait(lock);
    }
    ButtonEvent val = mQueue.front();
    mQueue.pop();
    return val;
}

bool Button::hasEvent(void)
{
    std::lock_guard<std::mutex> lock(mMutex);
    return !mQueue.empty();
}

void Button::isr()
{
    for (auto button : sButtons)
    {
        button->process();
    }
}
