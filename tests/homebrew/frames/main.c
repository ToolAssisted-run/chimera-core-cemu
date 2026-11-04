/*
 * frames: a Wii U program for the Cemu core's tests. It draws on the TV
 * through OSScreen (no GX2), a picture that changes every frame, and keeps
 * its state in one block of memory the tests can read (the "mailbox"):
 *
 *   - a frame count and a running hash of everything it did;
 *   - the buttons the GamePad held, as VPADRead gave them;
 *   - the console's time (OSGetTime) at each frame;
 *   - a second thread that counts under a mutex and a condition, and a
 *     periodic alarm, so that the scheduler and the timers are part of it.
 *
 * A frame is one pass of the loop, paced by OSSleepTicks to 1/60 s.
 */
#include <coreinit/alarm.h>
#include <coreinit/cache.h>
#include <coreinit/condition.h>
#include <coreinit/debug.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/mutex.h>
#include <coreinit/screen.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <vpad/input.h>

#include <stdio.h>
#include <string.h>

typedef struct
{
	char magic[16];             /* "CHIMERA-MAILBOX" */
	uint32_t frame;
	uint32_t hash;
	uint32_t buttons;
	uint32_t pad_reads;
	uint32_t worker_count;
	uint32_t alarm_count;
	int64_t time;
} Mailbox;

static Mailbox mailbox __attribute__((aligned(64)));
static OSMutex mutex;
static OSCondition cond;
static volatile uint32_t pending;
static OSAlarm alarm_;
static uint8_t worker_stack[0x8000] __attribute__((aligned(16)));
static OSThread worker;

static uint32_t mix(uint32_t h, uint32_t v)
{
	h ^= v;
	h *= 0x01000193u;
	return h ^ (h >> 15);
}

static int worker_main(int argc, void* argv)
{
	for (;;)
	{
		OSLockMutex(&mutex);
		while (pending == 0)
			OSWaitCond(&cond, &mutex);
		pending--;
		mailbox.worker_count++;
		mailbox.hash = mix(mailbox.hash, 0x1000u + mailbox.worker_count);
		OSUnlockMutex(&mutex);
	}
	return 0;
}

static void alarm_callback(OSAlarm* alarm, OSContext* context)
{
	mailbox.alarm_count++;
}

int main(int argc, char** argv)
{
	memcpy(mailbox.magic, "CHIMERA-MAILBOX", 16);
	mailbox.hash = 0x811C9DC5u;
	OSReport("frames: started, mailbox at %p\n", &mailbox);

	OSScreenInit();
	uint32_t tv_size = OSScreenGetBufferSizeEx(SCREEN_TV);
	uint32_t drc_size = OSScreenGetBufferSizeEx(SCREEN_DRC);
	void* tv = MEMAllocFromDefaultHeapEx(tv_size, 0x100);
	void* drc = MEMAllocFromDefaultHeapEx(drc_size, 0x100);
	OSScreenSetBufferEx(SCREEN_TV, tv);
	OSScreenSetBufferEx(SCREEN_DRC, drc);
	OSScreenEnableEx(SCREEN_TV, TRUE);
	OSScreenEnableEx(SCREEN_DRC, TRUE);

	OSInitMutex(&mutex);
	OSInitCond(&cond);
	OSCreateThread(&worker, worker_main, 0, NULL, worker_stack + sizeof(worker_stack), sizeof(worker_stack), 16, OS_THREAD_ATTRIB_AFFINITY_ANY);
	OSResumeThread(&worker);

	OSCreateAlarm(&alarm_);
	OSSetPeriodicAlarm(&alarm_, OSGetTime() + OSMillisecondsToTicks(5), OSMillisecondsToTicks(50), alarm_callback);

	for (;;)
	{
		VPADStatus status;
		VPADReadError error = VPAD_READ_SUCCESS;
		memset(&status, 0, sizeof(status));
		if (VPADRead(VPAD_CHAN_0, &status, 1, &error) > 0 && error == VPAD_READ_SUCCESS)
		{
			mailbox.buttons = status.hold;
			mailbox.pad_reads++;
		}

		OSLockMutex(&mutex);
		pending++;
		mailbox.hash = mix(mailbox.hash, mailbox.frame ^ mailbox.buttons);
		OSSignalCond(&cond);
		OSUnlockMutex(&mutex);

		/* a little work, so that a frame costs instructions */
		uint32_t h = mailbox.hash;
		for (int i = 0; i < 20000; i++)
			h = mix(h, (uint32_t)i);
		mailbox.hash = h;
		mailbox.time = OSGetTime();

		uint32_t f = mailbox.frame;
		uint32_t color = ((f * 3u) & 0xFFu) << 24 | ((f * 5u) & 0xFFu) << 16 | ((f * 7u) & 0xFFu) << 8;
		if (mailbox.buttons & VPAD_BUTTON_A)
			color = 0xFF000000u;
		OSScreenClearBufferEx(SCREEN_TV, color);
		OSScreenClearBufferEx(SCREEN_DRC, color);
		char line[96];
		snprintf(line, sizeof(line), "frame %lu hash %08lx", (unsigned long)f, (unsigned long)mailbox.hash);
		OSScreenPutFontEx(SCREEN_TV, 0, 0, line);
		OSScreenPutFontEx(SCREEN_DRC, 0, 0, line);
		snprintf(line, sizeof(line), "buttons %08lx worker %lu alarm %lu", (unsigned long)mailbox.buttons,
			(unsigned long)mailbox.worker_count, (unsigned long)mailbox.alarm_count);
		OSScreenPutFontEx(SCREEN_TV, 0, 1, line);
		OSScreenPutFontEx(SCREEN_DRC, 0, 1, line);
		DCFlushRange(tv, tv_size);
		DCFlushRange(drc, drc_size);
		OSScreenFlipBuffersEx(SCREEN_TV);
		OSScreenFlipBuffersEx(SCREEN_DRC);

		if ((f % 60) == 0)
			OSReport("frames: frame %lu hash %08lx worker %lu alarm %lu\n", (unsigned long)f, (unsigned long)mailbox.hash,
				(unsigned long)mailbox.worker_count, (unsigned long)mailbox.alarm_count);
		mailbox.frame++;
		OSSleepTicks(OSMillisecondsToTicks(16));
	}
	return 0;
}
