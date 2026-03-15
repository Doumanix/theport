#include <sblib/timer.h>
#include <sblib/eib/bus.h>

/*
 * RP2354 timer IRQ forwarding for sblib Bus timer.
 */

extern Bus* timerBusObj;

extern "C" void TIMER_IRQ_1()
{
    if (timerBusObj)
        timerBusObj->timerInterruptHandler();
}