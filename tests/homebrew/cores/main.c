/*
 * cores: a Wii U program for the Cemu core's tests of the console's three
 * CPU cores. Games put their threads on all three. One thread is pinned to
 * each core (affinity CPU0, CPU1, CPU2); they pass a token round in a ring
 * under one mutex and condition, each thread doing some work when it has
 * it, and each also sleeps a different number of ticks every few rounds.
 * A periodic alarm counts as well. The main thread draws nothing: it waits
 * a sixtieth of a second, counts a frame, and goes idle meanwhile.
 *
 * Its record in the "mailbox": the frame count, a hash of every turn in the
 * order the turns were taken, the turns taken, the cores the three threads
 * found themselves on (OSGetCoreId, 4 bits each, 0x210 when all is right),
 * the alarm's count, and how many times a thread found itself on a core it
 * was not pinned to.
 */
#include <coreinit/alarm.h>
#include <coreinit/condition.h>
#include <coreinit/core.h>
#include <coreinit/debug.h>
#include <coreinit/mutex.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

#include <string.h>

typedef struct
{
	char magic[16];             /* "CHIMERA-MAILBOX" */
	uint32_t frame;
	uint32_t hash;
	uint32_t turns;
	uint32_t cores;
	uint32_t alarms;
	uint32_t misplaced;
} Mailbox;

static Mailbox mailbox __attribute__((aligned(64)));
static OSMutex mutex;
static OSCondition cond;
static uint32_t token;          /* whose turn: 0, 1 or 2 */
static OSAlarm alarm_;
static uint8_t stacks[3][0x8000] __attribute__((aligned(16)));
static OSThread threads[3];

static uint32_t mix(uint32_t h, uint32_t v)
{
	h ^= v;
	h *= 0x01000193u;
	return h ^ (h >> 15);
}

static int ring_main(int argc, void* argv)
{
	const uint32_t me = (uint32_t)argc;
	for (uint32_t round = 0;; round++)
	{
		OSLockMutex(&mutex);
		while (token != me)
			OSWaitCond(&cond, &mutex);
		const uint32_t core = OSGetCoreId();
		mailbox.cores = (mailbox.cores & ~(0xFu << (me * 4))) | (core & 0xFu) << (me * 4);
		if (core != me)
			mailbox.misplaced++;
		mailbox.turns++;
		mailbox.hash = mix(mailbox.hash, me << 28 | core << 24 | (mailbox.turns & 0xFFFFFF));
		token = (me + 1) % 3;
		OSSignalCond(&cond);
		OSUnlockMutex(&mutex);

		/* work of its own, outside the lock */
		uint32_t h = me;
		for (int i = 0; i < 2000 * (int)(me + 1); i++)
			h = mix(h, (uint32_t)i);
		if ((round % 4) == me)
			OSSleepTicks(OSMicrosecondsToTicks(250 * (me + 1)));
		if (h == 0)
			OSReport("cores: %lu\n", (unsigned long)h); /* keeps the work */
	}
	return 0;
}

static void alarm_callback(OSAlarm* alarm, OSContext* context)
{
	mailbox.alarms++;
}

int main(int argc, char** argv)
{
	memcpy(mailbox.magic, "CHIMERA-MAILBOX", 16);
	mailbox.hash = 0x811C9DC5u;
	OSReport("cores: started, mailbox at %p\n", &mailbox);

	OSInitMutex(&mutex);
	OSInitCond(&cond);
	static const OSThreadAttributes affinity[3] = {OS_THREAD_ATTRIB_AFFINITY_CPU0, OS_THREAD_ATTRIB_AFFINITY_CPU1,
		OS_THREAD_ATTRIB_AFFINITY_CPU2};
	for (int i = 0; i < 3; i++)
	{
		OSCreateThread(&threads[i], ring_main, i, NULL, stacks[i] + sizeof(stacks[i]), sizeof(stacks[i]), 16, affinity[i]);
		OSResumeThread(&threads[i]);
	}

	OSCreateAlarm(&alarm_);
	OSSetPeriodicAlarm(&alarm_, OSGetTime() + OSMillisecondsToTicks(3), OSMillisecondsToTicks(7), alarm_callback);

	for (;;)
	{
		OSSleepTicks(OSMicrosecondsToTicks(16683));
		mailbox.frame++;
	}
	return 0;
}
