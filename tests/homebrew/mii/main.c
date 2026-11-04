/*
 * mii: a Wii U program for the Cemu core's tests of the console's Mii
 * resource. It reads each of the four files of that system title
 * (0005001b-10056000) from where a console keeps them, as games do, and
 * keeps in one block of memory the tests can read (the "mailbox") the size
 * of each (0 when it cannot be opened) and a hash of all their bytes.
 * Nothing of the Mii resource is in this program: what it hashes is what
 * the console has.
 */
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/filesystem.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/screen.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>

#include <stdio.h>
#include <string.h>

typedef struct
{
	char magic[16];             /* "CHIMERA-MAILBOX" */
	uint32_t frame;
	uint32_t hash;              /* of every file's bytes */
	uint32_t size[4];           /* FFLResHigh, FFLResHighLG, FFLResMiddle, FFLResMiddleLG */
} Mailbox;

static Mailbox mailbox __attribute__((aligned(64)));

static const char* const kFiles[4] = {
	"/vol/storage_mlc01/sys/title/0005001b/10056000/content/FFLResHigh.dat",
	"/vol/storage_mlc01/sys/title/0005001b/10056000/content/FFLResHighLG.dat",
	"/vol/storage_mlc01/sys/title/0005001b/10056000/content/FFLResMiddle.dat",
	"/vol/storage_mlc01/sys/title/0005001b/10056000/content/FFLResMiddleLG.dat",
};

int main(int argc, char** argv)
{
	memcpy(mailbox.magic, "CHIMERA-MAILBOX", 16);
	FSInit();
	FSClient* client = (FSClient*)MEMAllocFromDefaultHeap(sizeof(FSClient));
	FSCmdBlock* block = (FSCmdBlock*)MEMAllocFromDefaultHeap(sizeof(FSCmdBlock));
	uint8_t* buf = (uint8_t*)MEMAllocFromDefaultHeapEx(0x10000, 0x40);
	FSAddClient(client, FS_ERROR_FLAG_ALL);
	FSInitCmdBlock(block);
	uint32_t h = 0x811C9DC5u;
	for (int i = 0; i < 4; i++)
	{
		FSFileHandle file;
		uint32_t size = 0;
		if (FSOpenFile(client, block, kFiles[i], "r", &file, FS_ERROR_FLAG_ALL) == FS_STATUS_OK)
		{
			for (;;)
			{
				const FSStatus got = FSReadFile(client, block, buf, 1, 0x10000, file, 0, FS_ERROR_FLAG_ALL);
				if ((int32_t)got <= 0)
					break;
				for (int32_t k = 0; k < (int32_t)got; k++)
				{
					h ^= buf[k];
					h *= 0x01000193u;
				}
				size += (uint32_t)got;
			}
			FSCloseFile(client, block, file, FS_ERROR_FLAG_ALL);
		}
		mailbox.size[i] = size;
		h ^= size;
		h *= 0x01000193u;
	}
	mailbox.hash = h;
	OSReport("mii: hash %08lx sizes %lu %lu %lu %lu\n", (unsigned long)h, (unsigned long)mailbox.size[0],
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
		OSScreenClearBufferEx(SCREEN_TV, 0x00403000u);
		OSScreenClearBufferEx(SCREEN_DRC, 0x00403000u);
		snprintf(line, sizeof(line), "mii resource %08lx", (unsigned long)mailbox.hash);
		OSScreenPutFontEx(SCREEN_TV, 0, 0, line);
		for (int i = 0; i < 4; i++)
		{
			snprintf(line, sizeof(line), "file %d: %lu bytes", i, (unsigned long)mailbox.size[i]);
			OSScreenPutFontEx(SCREEN_TV, 0, 1 + i, line);
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
