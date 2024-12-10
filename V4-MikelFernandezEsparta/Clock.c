#include "Clock.h"
#include "Processor.h"
#include "ComputerSystemBase.h"

int tics=0;

void Clock_Update() {
	tics++;
	intervalBetweenInterrupts--;

    if (intervalBetweenInterrupts == 0) {
		Processor_RaiseInterrupt(CLOCKINT_BIT);
		intervalBetweenInterrupts = DEFAULT_INTERVAL_BETWEEN_INTERRUPTS;
	}
}

int Clock_GetTime() {
	return tics;
}
