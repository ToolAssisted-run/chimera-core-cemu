/*
 * storage: a Wii U program for the Cemu core's tests of the console's
 * storage as a new one is set up (what upstream Cemu's window makes in it).
 * It keeps in one block of memory the tests can read (the "mailbox"):
 * - which of six folders it finds: bit 0-2 when a file can be made in the
 *   Mii Maker's database folder of each region (save data of titles
 *   00050010-1004a000, -1004a100, -1004a200, where a game's Mii library
 *   keeps FFL_HDB.dat; the file is removed again), bit 3-5 when the folders
 *   titles are installed in are there (usr/title/00050000, 0005000c,
 *   0005000e);
 * - the size and a hash of the system's list of languages and of its list
 *   of countries (system title 0005001b-1005c000, language.txt and
 *   country.txt).
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
	uint32_t languageHash;      /* of language.txt's bytes */
	uint32_t found;             /* the folders, as above */
	uint32_t languageSize;      /* 0 when it cannot be opened */
	uint32_t countrySize;
	uint32_t countryHash;
} Mailbox;

static Mailbox mailbox __attribute__((aligned(64)));

static const char* const kMiiMaker[3] = {
	"/vol/storage_mlc01/usr/save/00050010/1004a000/user/common/db",
	"/vol/storage_mlc01/usr/save/00050010/1004a100/user/common/db",
	"/vol/storage_mlc01/usr/save/00050010/1004a200/user/common/db",
};

static const char* const kTitles[3] = {
	"/vol/storage_mlc01/usr/title/00050000",
	"/vol/storage_mlc01/usr/title/0005000c",
	"/vol/storage_mlc01/usr/title/0005000e",
};

static const char kLists[] = "/vol/storage_mlc01/sys/title/0005001b/1005c000/content";

/* the file's size, and a hash of its bytes into *hash */
static uint32_t ReadFile(FSClient* client, FSCmdBlock* block, const char* path, uint8_t* buf, uint32_t* hash)
{
	FSFileHandle file;
	uint32_t size = 0;
	uint32_t h = 0x811C9DC5u;
	if (FSOpenFile(client, block, path, "r", &file, FS_ERROR_FLAG_ALL) == FS_STATUS_OK)
	{
		for (;;)
		{
			const FSStatus got = FSReadFile(client, block, buf, 1, 0x1000, file, 0, FS_ERROR_FLAG_ALL);
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
	*hash = h;
	return size;
}

int main(int argc, char** argv)
{
	memcpy(mailbox.magic, "CHIMERA-MAILBOX", 16);
	FSInit();
	FSClient* client = (FSClient*)MEMAllocFromDefaultHeap(sizeof(FSClient));
	FSCmdBlock* block = (FSCmdBlock*)MEMAllocFromDefaultHeap(sizeof(FSCmdBlock));
	uint8_t* buf = (uint8_t*)MEMAllocFromDefaultHeapEx(0x1000, 0x40);
	FSAddClient(client, FS_ERROR_FLAG_ALL);
	FSInitCmdBlock(block);
	uint32_t found = 0;
	char path[160];
	for (int i = 0; i < 3; i++)
	{
		FSFileHandle file;
		snprintf(path, sizeof(path), "%s/chimera-test.dat", kMiiMaker[i]);
		if (FSOpenFile(client, block, path, "w", &file, FS_ERROR_FLAG_ALL) == FS_STATUS_OK)
		{
			FSCloseFile(client, block, file, FS_ERROR_FLAG_ALL);
			FSRemove(client, block, path, FS_ERROR_FLAG_ALL);
			found |= 1u << i;
		}
	}
	for (int i = 0; i < 3; i++)
	{
		FSStat stat;
		if (FSGetStat(client, block, kTitles[i], &stat, FS_ERROR_FLAG_ALL) == FS_STATUS_OK && (stat.flags & FS_STAT_DIRECTORY))
			found |= 1u << (3 + i);
	}
	mailbox.found = found;
	snprintf(path, sizeof(path), "%s/language.txt", kLists);
	mailbox.languageSize = ReadFile(client, block, path, buf, &mailbox.languageHash);
	snprintf(path, sizeof(path), "%s/country.txt", kLists);
	mailbox.countrySize = ReadFile(client, block, path, buf, &mailbox.countryHash);
	OSReport("storage: folders %02lx languages %lu bytes %08lx countries %lu bytes %08lx\n", (unsigned long)found,
		(unsigned long)mailbox.languageSize, (unsigned long)mailbox.languageHash, (unsigned long)mailbox.countrySize,
		(unsigned long)mailbox.countryHash);

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
		snprintf(line, sizeof(line), "storage folders %02lx", (unsigned long)mailbox.found);
		OSScreenPutFontEx(SCREEN_TV, 0, 0, line);
		snprintf(line, sizeof(line), "languages %lu bytes, countries %lu bytes", (unsigned long)mailbox.languageSize,
			(unsigned long)mailbox.countrySize);
		OSScreenPutFontEx(SCREEN_TV, 0, 1, line);
		DCFlushRange(tv, tv_size);
		DCFlushRange(drc, drc_size);
		OSScreenFlipBuffersEx(SCREEN_TV);
		OSScreenFlipBuffersEx(SCREEN_DRC);
		mailbox.frame++;
		OSSleepTicks(OSMillisecondsToTicks(16));
	}
	return 0;
}
