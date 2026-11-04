/*
 * saves: a Wii U program for the Cemu core's tests of save data. At start it
 * reads its saved game (chimera.sav in the console's save folder for the
 * first account) if there is one, and then every 30 frames writes the frame
 * count into it, as a game saves. Its record in the "mailbox": the frame
 * count, a hash, the count read at start (0 if there was no save), how many
 * times it has saved, the status of the last save.
 */
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/filesystem.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/screen.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <nn/save.h>

#include <stdio.h>
#include <string.h>

typedef struct
{
	char magic[16];             /* "CHIMERA-MAILBOX" */
	uint32_t frame;
	uint32_t hash;
	uint32_t loaded;
	uint32_t saves;
	uint32_t status;
	uint32_t unused;
} Mailbox;

static Mailbox mailbox __attribute__((aligned(64)));
static uint32_t record[16] __attribute__((aligned(64)));

#define SLOT 1 /* the first account */

static int save_now(FSClient* client, FSCmdBlock* block)
{
	FSFileHandle handle;
	FSStatus s = SAVEOpenFile(client, block, SLOT, "chimera.sav", "w", &handle, FS_ERROR_FLAG_ALL);
	if (s != FS_STATUS_OK)
		return (int)s;
	record[0] = 0x43484D53; /* "CHMS" */
	record[1] = mailbox.frame;
	s = FSWriteFile(client, block, (uint8_t*)record, 1, 8, handle, 0, FS_ERROR_FLAG_ALL);
	FSCloseFile(client, block, handle, FS_ERROR_FLAG_ALL);
	SAVEFlushQuota(client, block, SLOT, FS_ERROR_FLAG_ALL);
	return s == 8 ? 0 : (int)s;
}

int main(int argc, char** argv)
{
	memcpy(mailbox.magic, "CHIMERA-MAILBOX", 16);
	mailbox.hash = 0x811C9DC5u;

	FSInit();
	FSClient* client = (FSClient*)MEMAllocFromDefaultHeapEx(sizeof(FSClient), 0x40);
	FSCmdBlock* block = (FSCmdBlock*)MEMAllocFromDefaultHeapEx(sizeof(FSCmdBlock), 0x40);
	FSAddClient(client, FS_ERROR_FLAG_ALL);
	FSInitCmdBlock(block);
	SAVEInit();
	SAVEInitSaveDir(SLOT);

	FSFileHandle handle;
	if (SAVEOpenFile(client, block, SLOT, "chimera.sav", "r", &handle, FS_ERROR_FLAG_ALL) == FS_STATUS_OK)
	{
		memset(record, 0, sizeof(record));
		if (FSReadFile(client, block, (uint8_t*)record, 1, 8, handle, 0, FS_ERROR_FLAG_ALL) == 8 && record[0] == 0x43484D53)
			mailbox.loaded = record[1];
		FSCloseFile(client, block, handle, FS_ERROR_FLAG_ALL);
	}
	OSReport("saves: started, read %lu\n", (unsigned long)mailbox.loaded);

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
		if (mailbox.frame % 30 == 29)
		{
			mailbox.status = (uint32_t)save_now(client, block);
			mailbox.saves++;
		}
		mailbox.hash ^= mailbox.frame + mailbox.loaded + mailbox.status;
		mailbox.hash *= 0x01000193u;
		char line[96];
		OSScreenClearBufferEx(SCREEN_TV, 0x30100000u);
		OSScreenClearBufferEx(SCREEN_DRC, 0x30100000u);
		snprintf(line, sizeof(line), "read %lu saves %lu status %ld", (unsigned long)mailbox.loaded,
			(unsigned long)mailbox.saves, (long)(int32_t)mailbox.status);
		OSScreenPutFontEx(SCREEN_TV, 0, 0, line);
		DCFlushRange(tv, tv_size);
		DCFlushRange(drc, drc_size);
		OSScreenFlipBuffersEx(SCREEN_TV);
		OSScreenFlipBuffersEx(SCREEN_DRC);
		mailbox.frame++;
		OSSleepTicks(OSMillisecondsToTicks(16));
	}
	return 0;
}
