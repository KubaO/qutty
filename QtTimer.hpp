#ifndef QTTIMER_H
#define QTTIMER_H

#include <QObject>
#include <QTimerEvent>
extern "C" {
#include "putty.h"
}

class QtTimer : public QObject {
  Q_OBJECT

  int timerId = -1;
  long nextTick = 0;

 public:
  void startTimerForTick(long nextTick) {
    long ticks = nextTick - GETTICKCOUNT();
    if (ticks <= 0) ticks = 1; /* just in case */
    if (timerId != -1) this->killTimer(timerId);
    timerId = this->startTimer(ticks);
    this->nextTick = nextTick;
  }

 protected:
  void timerEvent(QTimerEvent *event) override {
    unsigned long next;
    killTimer(timerId);

    // only one timer is active at any point of time
    assert(event->timerId() == timerId);
    timerId = -1;

    if (run_timers(this->nextTick, &next)) {
      startTimerForTick(next);
    }
  }
};

#endif  // QTTIMER_H
