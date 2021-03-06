/*
    ChibiOS/GFX - Copyright (C) 2012
                 Joel Bodenmann aka Tectu <joel@unormal.org>

    This file is part of ChibiOS/GFX.

    ChibiOS/GFX is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ChibiOS/GFX is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file    src/gtimer/gtimer.c
 * @brief   GTIMER sub-system code.
 *
 * @addtogroup GTIMER
 * @{
 */
#include "ch.h"
#include "hal.h"
#include "gfx.h"

#if GFX_USE_GTIMER || defined(__DOXYGEN__)

#if !CH_USE_MUTEXES || !CH_USE_SEMAPHORES
	#error "GTIMER: CH_USE_MUTEXES and CH_USE_SEMAPHORES must be defined in chconf.h"
#endif

#define GTIMER_FLG_PERIODIC		0x0001
#define GTIMER_FLG_INFINITE		0x0002
#define GTIMER_FLG_JABBED		0x0004
#define GTIMER_FLG_SCHEDULED	0x0008

/* Don't rework this macro to use a ternary operator - the gcc compiler stuffs it up */
#define TimeIsWithin(x, start, end)	((end >= start && x >= start && x <= end) || (end < start && (x >= start || x <= end)))

/* This mutex protects access to our tables */
static MUTEX_DECL(mutex);
static Thread 			*pThread = 0;
static GTimer			*pTimerHead = 0;
static BSEMAPHORE_DECL(waitsem, TRUE);
static WORKING_AREA(waTimerThread, GTIMER_THREAD_WORKAREA_SIZE);

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

static msg_t GTimerThreadHandler(void *arg) {
	(void)arg;
	GTimer			*pt;
	systime_t		tm;
	systime_t		nxtTimeout;
	systime_t		lastTime;
	GTimerFunction	fn;
	void			*param;

	#if CH_USE_REGISTRY
		chRegSetThreadName("GTimer");
	#endif

	nxtTimeout = TIME_INFINITE;
	lastTime = 0;
	while(1) {
		/* Wait for work to do. */
		chThdYield();					// Give someone else a go no matter how busy we are
		chBSemWaitTimeout(&waitsem, nxtTimeout);
		
	restartTimerChecks:
	
		// Our reference time
		tm = chTimeNow();
		nxtTimeout = TIME_INFINITE;
		
		/* We need to obtain the mutex */
		chMtxLock(&mutex);

		if (pTimerHead) {
			pt = pTimerHead;
			do {
				// Do we have something to do for this timer?
				if ((pt->flags & GTIMER_FLG_JABBED) || (!(pt->flags & GTIMER_FLG_INFINITE) && TimeIsWithin(pt->when, lastTime, tm))) {
				
					// Is this timer periodic?
					if ((pt->flags & GTIMER_FLG_PERIODIC) && pt->period != TIME_IMMEDIATE) {
						// Yes - Update ready for the next period
						if (!(pt->flags & GTIMER_FLG_INFINITE)) {
							// We may have skipped a period.
							// We use this complicated formulae rather than a loop
							//	because the gcc compiler stuffs up the loop so that it
							//	either loops forever or doesn't get executed at all.
							pt->when += ((tm + pt->period - pt->when) / pt->period) * pt->period;
						}

						// We are definitely no longer jabbed
						pt->flags &= ~GTIMER_FLG_JABBED;
						
					} else {
						// No - get us off the timers list
						if (pt->next == pt->prev)
							pTimerHead = 0;
						else {
							pt->next->prev = pt->prev;
							pt->prev->next = pt->next;
							if (pTimerHead == pt)
								pTimerHead = pt->next;
						}
						pt->flags = 0;
					}
					
					// Call the callback function
					fn = pt->fn;
					param = pt->param;
					chMtxUnlock();
					fn(param);
					
					// We no longer hold the mutex, the callback function may have taken a while
					// and our list may have been altered so start again!
					goto restartTimerChecks;
				}
				
				// Find when we next need to wake up
				if (!(pt->flags & GTIMER_FLG_INFINITE) && pt->when - tm < nxtTimeout)
					nxtTimeout = pt->when - tm;
				pt = pt->next;
			} while(pt != pTimerHead);
		}

		// Ready for the next loop
		lastTime = tm;
		chMtxUnlock();
	}
	return 0;
}

void gtimerInit(GTimer *pt) {
	pt->flags = 0;
}

void gtimerStart(GTimer *pt, GTimerFunction fn, void *param, bool_t periodic, systime_t millisec) {
	chMtxLock(&mutex);
	
	// Start our thread if not already going
	if (!pThread)
		pThread = chThdCreateStatic(waTimerThread, sizeof(waTimerThread), HIGHPRIO, GTimerThreadHandler, NULL);

	// Is this already scheduled?
	if (pt->flags & GTIMER_FLG_SCHEDULED) {
		// Cancel it!
		if (pt->next == pt->prev)
			pTimerHead = 0;
		else {
			pt->next->prev = pt->prev;
			pt->prev->next = pt->next;
			if (pTimerHead == pt)
				pTimerHead = pt->next;
		}
	}
	
	// Set up the timer structure
	pt->fn = fn;
	pt->param = param;
	pt->flags = GTIMER_FLG_SCHEDULED;
	if (periodic)
		pt->flags |= GTIMER_FLG_PERIODIC;
	if (millisec == TIME_INFINITE) {
		pt->flags |= GTIMER_FLG_INFINITE;
		pt->period = TIME_INFINITE;
	} else {
		pt->period = MS2ST(millisec);
		pt->when = chTimeNow() + pt->period;
	}

	// Just pop it on the end of the queue
	if (pTimerHead) {
		pt->next = pTimerHead;
		pt->prev = pTimerHead->prev;
		pt->prev->next = pt;
		pt->next->prev = pt;
	} else
		pt->next = pt->prev = pTimerHead = pt;

	// Bump the thread
	if (!(pt->flags & GTIMER_FLG_INFINITE))
		chBSemSignal(&waitsem);
	chMtxUnlock();
}

void gtimerStop(GTimer *pt) {
	chMtxLock(&mutex);
	if (pt->flags & GTIMER_FLG_SCHEDULED) {
		// Cancel it!
		if (pt->next == pt->prev)
			pTimerHead = 0;
		else {
			pt->next->prev = pt->prev;
			pt->prev->next = pt->next;
			if (pTimerHead == pt)
				pTimerHead = pt->next;
		}
		// Make sure we know the structure is dead!
		pt->flags = 0;
	}
	chMtxUnlock();
}

bool_t gtimerIsActive(GTimer *pt) {
	return (pt->flags & GTIMER_FLG_SCHEDULED) ? TRUE : FALSE;
}

void gtimerJab(GTimer *pt) {
	chMtxLock(&mutex);
	
	// Jab it!
	pt->flags |= GTIMER_FLG_JABBED;

	// Bump the thread
	chBSemSignal(&waitsem);
	chMtxUnlock();
}

void gtimerJabI(GTimer *pt) {
	// Jab it!
	pt->flags |= GTIMER_FLG_JABBED;

	// Bump the thread
	chBSemSignalI(&waitsem);
}

#endif /* GFX_USE_GTIMER */
/** @} */

