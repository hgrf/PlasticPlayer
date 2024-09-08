#ifndef BUTTON_H
#define BUTTON_H

#include <condition_variable>
#include <list>
#include <mutex>
#include <queue>
#include <thread>

class Button
{
public:
  enum ButtonEvent
  {
    PRESS,
    RELEASE
  };

  Button(int pin);
  void begin();
  void process();
  ButtonEvent waitForEvent(void);
  bool hasEvent(void);

private:
  static void isr();

  std::mutex mMutex;
  std::condition_variable mCondition;
  std::queue<ButtonEvent> mQueue;
  int mPin;
  int mState;
  int mLastState;
  unsigned long mLastDebounceTime;
  static const unsigned long DEBOUNCE_DELAY = 50;
};

#endif // BUTTON_H
