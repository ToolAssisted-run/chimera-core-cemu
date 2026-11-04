/*
 * pad: a Wii U program for the Cemu core's tests of the GamePad and the
 * sound. Each pass of its loop (a frame, paced to 1/60 s) it reads the
 * GamePad and keeps what it read in one block of memory the tests can read
 * (the "mailbox"):
 *
 *   - the buttons held (VPAD's hold word, sticks' directions included);
 *   - the touch screen: x and y as the GamePad reports them, or 0 when not
 *     touched;
 *   - the sticks, each as a whole number from -100 to 100;
 *   - a running hash of all of it, and of what was pressed and released.
 *
 * Every tenth pass it does not read the GamePad, so that a frame now and
 * then is a lag frame.
 *
 * It plays a square wave on the TV through the console's sound library
 * (AX); while A is held the wave is an octave higher. It draws on both
 * screens: the TV says what was read, the GamePad shows a cross where it is
 * touched.
 */
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/screen.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <sndcore2/core.h>
#include <sndcore2/voice.h>
#include <vpad/input.h>

#include <stdio.h>
#include <string.h>

typedef struct
{
	char magic[16];             /* "CHIMERA-MAILBOX" */
	uint32_t frame;
	uint32_t hash;
	uint32_t hold;
	uint32_t pad_reads;
	uint32_t touch;             /* x << 16 | y, or 0 */
	uint32_t sticks;            /* lx, ly, rx, ry: a signed byte each */
	uint32_t trig_count, release_count;
} Mailbox;

static Mailbox mailbox __attribute__((aligned(64)));

#define WAVE_SAMPLES 4800 /* 100 ms at 48 kHz: ten periods of 100 Hz */
static int16_t wave[WAVE_SAMPLES] __attribute__((aligned(64)));

static uint32_t mix(uint32_t h, uint32_t v)
{
	h ^= v;
	h *= 0x01000193u;
	return h ^ (h >> 15);
}

static int8_t stick_byte(float v)
{
	int i = (int)(v * 100.0f + (v < 0 ? -0.5f : 0.5f));
	return (int8_t)(i < -100 ? -100 : (i > 100 ? 100 : i));
}

static AXVoice* start_voice(void)
{
	AXInitParams params = {AX_INIT_RENDERER_48KHZ, 0, AX_INIT_PIPELINE_SINGLE};
	AXInitWithParams(&params);
	for (int i = 0; i < WAVE_SAMPLES; i++)
		wave[i] = (i / 240) % 2 ? 8000 : -8000;
	DCFlushRange(wave, sizeof(wave));

	AXVoice* voice = AXAcquireVoice(31, NULL, NULL);
	if (!voice)
		return NULL;
	AXVoiceBegin(voice);
	AXSetVoiceType(voice, 0);
	AXVoiceOffsets offsets;
	memset(&offsets, 0, sizeof(offsets));
	offsets.dataType = AX_VOICE_FORMAT_LPCM16;
	offsets.loopingEnabled = AX_VOICE_LOOP_ENABLED;
	offsets.loopOffset = 0;
	offsets.endOffset = WAVE_SAMPLES - 1;
	offsets.currentOffset = 0;
	offsets.data = wave;
	AXSetVoiceOffsets(voice, &offsets);
	AXVoiceVeData ve = {0x8000, 0};
	AXSetVoiceVe(voice, &ve);
	AXVoiceDeviceMixData tv[6];
	memset(tv, 0, sizeof(tv));
	tv[0].bus[0].volume = 0x8000;
	tv[1].bus[0].volume = 0x8000;
	AXSetVoiceDeviceMix(voice, AX_DEVICE_TYPE_TV, 0, tv);
	AXSetVoiceSrcType(voice, AX_VOICE_SRC_TYPE_LINEAR);
	AXSetVoiceSrcRatio(voice, 1.0f);
	AXSetVoiceState(voice, AX_VOICE_STATE_PLAYING);
	AXVoiceEnd(voice);
	return voice;
}

int main(int argc, char** argv)
{
	memcpy(mailbox.magic, "CHIMERA-MAILBOX", 16);
	mailbox.hash = 0x811C9DC5u;
	OSReport("pad: started, mailbox at %p\n", &mailbox);

	OSScreenInit();
	uint32_t tv_size = OSScreenGetBufferSizeEx(SCREEN_TV);
	uint32_t drc_size = OSScreenGetBufferSizeEx(SCREEN_DRC);
	void* tv = MEMAllocFromDefaultHeapEx(tv_size, 0x100);
	void* drc = MEMAllocFromDefaultHeapEx(drc_size, 0x100);
	OSScreenSetBufferEx(SCREEN_TV, tv);
	OSScreenSetBufferEx(SCREEN_DRC, drc);
	OSScreenEnableEx(SCREEN_TV, TRUE);
	OSScreenEnableEx(SCREEN_DRC, TRUE);

	AXVoice* voice = start_voice();
	OSReport("pad: voice %p\n", voice);
	int high = 0;

	for (uint32_t pass = 0;; pass++)
	{
		if (pass % 10 != 9)
		{
			VPADStatus status;
			VPADReadError error = VPAD_READ_SUCCESS;
			memset(&status, 0, sizeof(status));
			if (VPADRead(VPAD_CHAN_0, &status, 1, &error) > 0 && error == VPAD_READ_SUCCESS)
			{
				mailbox.pad_reads++;
				mailbox.hold = status.hold;
				mailbox.touch = status.tpNormal.touched ? ((uint32_t)status.tpNormal.x << 16 | status.tpNormal.y) : 0;
				mailbox.sticks = (uint32_t)(uint8_t)stick_byte(status.leftStick.x) << 24 |
					(uint32_t)(uint8_t)stick_byte(status.leftStick.y) << 16 |
					(uint32_t)(uint8_t)stick_byte(status.rightStick.x) << 8 |
					(uint32_t)(uint8_t)stick_byte(status.rightStick.y);
				if (status.trigger)
					mailbox.trig_count++;
				if (status.release)
					mailbox.release_count++;
				mailbox.hash = mix(mailbox.hash, status.hold);
				mailbox.hash = mix(mailbox.hash, status.trigger);
				mailbox.hash = mix(mailbox.hash, status.release);
				mailbox.hash = mix(mailbox.hash, mailbox.touch);
				mailbox.hash = mix(mailbox.hash, mailbox.sticks);
			}
		}

		const int want_high = (mailbox.hold & VPAD_BUTTON_A) != 0;
		if (voice && want_high != high)
		{
			AXVoiceBegin(voice);
			AXSetVoiceSrcRatio(voice, want_high ? 2.0f : 1.0f);
			AXVoiceEnd(voice);
			high = want_high;
		}
		mailbox.hash = mix(mailbox.hash, voice ? AXGetVoiceCurrentOffsetEx(voice, wave) : 0);

		uint32_t f = mailbox.frame;
		OSScreenClearBufferEx(SCREEN_TV, 0x20203000u);
		OSScreenClearBufferEx(SCREEN_DRC, 0x10101000u);
		char line[96];
		snprintf(line, sizeof(line), "pass %lu hold %08lx reads %lu", (unsigned long)f, (unsigned long)mailbox.hold,
			(unsigned long)mailbox.pad_reads);
		OSScreenPutFontEx(SCREEN_TV, 0, 0, line);
		snprintf(line, sizeof(line), "touch %08lx sticks %08lx", (unsigned long)mailbox.touch, (unsigned long)mailbox.sticks);
		OSScreenPutFontEx(SCREEN_TV, 0, 1, line);
		OSScreenPutFontEx(SCREEN_DRC, 0, 0, "GamePad");
		if (mailbox.touch)
		{
			/* the touch panel's raw range on the screen's 854x480 */
			int x = ((int)(mailbox.touch >> 16) - 92) * 854 / 3883;
			int y = (4095 - 254 - (int)(mailbox.touch & 0xFFFF)) * 480 / 3694;
			for (int d = -10; d <= 10; d++)
			{
				if (x + d >= 0 && x + d < 854 && y >= 0 && y < 480)
					OSScreenPutPixelEx(SCREEN_DRC, x + d, y, 0xFFFF0000u);
				if (y + d >= 0 && y + d < 480 && x >= 0 && x < 854)
					OSScreenPutPixelEx(SCREEN_DRC, x, y + d, 0xFFFF0000u);
			}
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
