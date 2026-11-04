/*
 * fonts: a Wii U program for the Cemu core's tests of the console's shared
 * fonts. It asks the console for each of its four fonts (OSGetSharedData)
 * and keeps in one block of memory the tests can read (the "mailbox") the
 * size of each and a hash of all their bytes. It shows them on the TV.
 * No font is in this program: what it hashes is what the console gave.
 */
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/memory.h>
#include <coreinit/screen.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

#include <stdio.h>
#include <string.h>

typedef struct
{
	char magic[16];             /* "CHIMERA-MAILBOX" */
	uint32_t frame;
	uint32_t hash;              /* of every font's bytes */
	uint32_t size[4];           /* Chinese (simplified), Korean, standard, Chinese (traditional) */
} Mailbox;

static Mailbox mailbox __attribute__((aligned(64)));

int main(int argc, char** argv)
{
	memcpy(mailbox.magic, "CHIMERA-MAILBOX", 16);
	uint32_t h = 0x811C9DC5u;
	for (int type = 0; type < 4; type++)
	{
		void* data = NULL;
		uint32_t size = 0;
		if (!OSGetSharedData((OSSharedDataType)type, 0, &data, &size) || !data)
			size = 0;
		mailbox.size[type] = size;
		const uint8_t* p = (const uint8_t*)data;
		for (uint32_t i = 0; i < size; i++)
		{
			h ^= p[i];
			h *= 0x01000193u;
		}
		h ^= size;
		h *= 0x01000193u;
	}
	mailbox.hash = h;
	OSReport("fonts: hash %08lx sizes %lu %lu %lu %lu\n", (unsigned long)h, (unsigned long)mailbox.size[0],
		(unsigned long)mailbox.size[1], (unsigned long)mailbox.size[2], (unsigned long)mailbox.size[3]);

	OSScreenInit();
	uint32_t tv_size = OSScreenGetBufferSizeEx(SCREEN_TV);
	uint32_t drc_size = OSScreenGetBufferSizeEx(SCREEN_DRC);
	void* tv = MEMAllocFromDefaultHeapEx(tv_size, 0x100);
	void* drc = MEMAllocFromDefaultHeapEx(drc_size, 0x100);
	OSScreenSetBufferEx(SCREEN_TV, tv);
	OSScreenSetBufferEx(SCREEN_DRC, drc);
	OSScreenEnableEx(SCREEN_TV, TRUE);
	OSScreenEnableEx(SCREEN_DRC, TRUE);
	for (;;)
	{
		char line[96];
		OSScreenClearBufferEx(SCREEN_TV, 0x00304000u);
		OSScreenClearBufferEx(SCREEN_DRC, 0x00304000u);
		snprintf(line, sizeof(line), "fonts %08lx", (unsigned long)mailbox.hash);
		OSScreenPutFontEx(SCREEN_TV, 0, 0, line);
		for (int type = 0; type < 4; type++)
		{
			snprintf(line, sizeof(line), "font %d: %lu bytes", type, (unsigned long)mailbox.size[type]);
			OSScreenPutFontEx(SCREEN_TV, 0, 1 + type, line);
		}
		DCFlushRange(tv, tv_size);
		DCFlushRange(drc, drc_size);
		OSScreenFlipBuffersEx(SCREEN_TV);
		OSScreenFlipBuffersEx(SCREEN_DRC);
		mailbox.frame++;
		OSSleepTicks(OSMillisecondsToTicks(16));
	}
	return 0;
}
