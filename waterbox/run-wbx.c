/* run-wbx: the core in miniBox, the way Chimera runs it. The same harness as
 * run-native (gate-harness.h), the same lines: the program is mounted under
 * its own name with "slots" naming it and the settings in "settings", the
 * shape a project mounts, and the GPU bridge's host half answers in this
 * process.
 *
 * usage: run-wbx <core.wbx> <program> --work <dir> [options: see gate-harness.h]
 * SPDX-License-Identifier: MIT */
#include "minibox.h"
#include "gate-harness.h"

#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int chimera_gl_host_init(char *err, int errlen);
const char *chimera_gl_host_description(void);
uintptr_t chimera_gl_host_dispatch(uintptr_t op, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e);
unsigned long chimera_gl_host_unhandled(long *last_op);
void chimera_gl_host_state_loaded(void);

static mb_host *g_host;

typedef struct { FILE *f; } freader;
static intptr_t file_read(uintptr_t ud, uint8_t *d, uintptr_t s) { return (intptr_t)fread(d, 1, s, ((freader *)ud)->f); }

typedef struct { const uint8_t *p; size_t n, pos; } memreader;
static intptr_t mem_reader(uintptr_t ud, uint8_t *d, uintptr_t s)
{
	memreader *m = (memreader *)ud;
	size_t left = m->n - m->pos;
	if (s > left) s = left;
	memcpy(d, m->p + m->pos, s);
	m->pos += s;
	return (intptr_t)s;
}

typedef struct { uint8_t *b; size_t len, cap, pos; } membuf;
static int32_t mem_write(uintptr_t ud, const uint8_t *d, uintptr_t n)
{
	membuf *m = (membuf *)ud;
	if (m->len + n > m->cap) {
		size_t cap = m->cap ? m->cap * 2 : (64u << 20);
		while (cap < m->len + n) cap *= 2;
		uint8_t *b = (uint8_t *)realloc(m->b, cap);
		if (!b) return -1;
		m->b = b;
		m->cap = cap;
	}
	memcpy(m->b + m->len, d, n);
	m->len += n;
	return 0;
}
static intptr_t mem_read(uintptr_t ud, uint8_t *d, uintptr_t n)
{
	membuf *m = (membuf *)ud;
	size_t left = m->len - m->pos;
	if (n > left) n = left;
	memcpy(d, m->b + m->pos, n);
	m->pos += n;
	return (intptr_t)n;
}

/* the machine after Init is the savestates' baseline */
static int do_seal(void)
{
	mb_return r;
	wbx_deactivate_host(g_host, &r);
	wbx_seal(g_host, &r);
	if (r.error_message[0]) { fprintf(stderr, "seal: %s\n", r.error_message); return 0; }
	wbx_activate_host(g_host, &r);
	return 1;
}
static int do_save(uint8_t **buf, size_t *len)
{
	membuf m = { 0 };
	mb_return r;
	wbx_save_state(g_host, mem_write, (uintptr_t)&m, &r);
	if (r.error_message[0]) { fprintf(stderr, "save: %s\n", r.error_message); free(m.b); return 0; }
	*buf = m.b;
	*len = m.len;
	return 1;
}
/* What Chimera's session does after a load: the bridge mints a fresh context
 * id, the renderer's cue that what it remembers is another context's. */
static int do_load(const uint8_t *buf, size_t len)
{
	membuf m = { (uint8_t *)buf, len, len, 0 };
	mb_return r;
	wbx_load_state(g_host, mem_read, (uintptr_t)&m, &r);
	if (r.error_message[0]) { fprintf(stderr, "load: %s\n", r.error_message); return 0; }
	chimera_gl_host_state_loaded();
	return 1;
}

static uintptr_t proc(const char *n)
{
	mb_return r;
	wbx_get_proc_addr(g_host, n, &r);
	if (r.error_message[0] || !r.data) { fprintf(stderr, "missing export %s %s\n", n, r.error_message); exit(2); }
	return r.data;
}

static int mount_text(const char *name, const char *text)
{
	static memreader readers[8];
	static int used;
	mb_return r;
	memreader *m = &readers[used++];
	m->p = (const uint8_t *)text;
	m->n = strlen(text);
	m->pos = 0;
	wbx_mount_file(g_host, name, mem_reader, (uintptr_t)m, false, &r);
	if (r.error_message[0]) { fprintf(stderr, "mount %s: %s\n", name, r.error_message); return 0; }
	return 1;
}

int main(int argc, char **argv)
{
	if (argc < 2) { fprintf(stderr, "usage: run-wbx <core.wbx> <program> --work <dir> [options]\n"); return 2; }
	const char *core = argv[1];
	struct harness_opts o;
	if (!harness_parse(argc - 1, argv + 1, &o)) return 2;
	if (!harness_fresh_work(o.work)) return 2;

	FILE *wf = fopen(core, "rb");
	if (!wf) { fprintf(stderr, "cannot open %s\n", core); return 1; }
	/* sbrk, sealed, invisible, plain, mmap: the console's 4 GiB address space
	 * is one reservation in the mmap arena, beside the recompiler's table and
	 * the emulator's heap (Linux backs it lazily). Keep in step with
	 * waterbox.config's memoryLayoutMiB. */
	mb_memory_layout_template layout = { (uintptr_t)1024 << 20, 16u << 20, 64u << 20, 64u << 20, (uintptr_t)12 << 30 };
	freader fr = { wf };
	mb_return r;
	wbx_create_host(&layout, "core.wbx", file_read, (uintptr_t)&fr, &r);
	fclose(wf);
	if (r.error_message[0]) { fprintf(stderr, "create: %s\n", r.error_message); return 1; }
	g_host = (mb_host *)r.data;
	wbx_set_clock_start(g_host, (uint64_t)o.clock_start, &r);

	/* the program under its own name, the slot map and the settings */
	const char *base = harness_base(o.program);
	static char vfsname[512];
	snprintf(vfsname, sizeof vfsname, "%s", base);
	wbx_mount_file_path(g_host, vfsname, o.program, &r);
	if (r.error_message[0]) { fprintf(stderr, "mount %s: %s\n", vfsname, r.error_message); return 1; }
	/* firmware is mounted under its id */
	if (o.fonts)
		for (int i = 0; i < 4; i++) {
			static char path[4][4096];
			snprintf(path[i], sizeof path[i], "%s/%s", o.fonts, harness_font_names[i]);
			wbx_mount_file_path(g_host, harness_font_names[i], path[i], &r);
			if (r.error_message[0]) { fprintf(stderr, "mount %s: %s\n", path[i], r.error_message); return 1; }
		}
	static char slots[1024], settings[2048];
	if (o.savedata) {
		wbx_mount_file_path(g_host, harness_base(o.savedata), o.savedata, &r);
		if (r.error_message[0]) { fprintf(stderr, "mount %s: %s\n", o.savedata, r.error_message); return 1; }
	}
	if (o.mii)
		for (size_t i = 0; i < HARNESS_MII_FILES; i++) {
			static char path[HARNESS_MII_FILES][4096];
			snprintf(path[i], sizeof path[i], "%s/%s", o.mii, harness_mii_names[i]);
			if (access(path[i], R_OK) != 0) continue;
			wbx_mount_file_path(g_host, harness_mii_names[i], path[i], &r);
			if (r.error_message[0]) { fprintf(stderr, "mount %s: %s\n", path[i], r.error_message); return 1; }
		}
	if (o.dlc) {
		wbx_mount_file_path(g_host, harness_base(o.dlc), o.dlc, &r);
		if (r.error_message[0]) { fprintf(stderr, "mount %s: %s\n", o.dlc, r.error_message); return 1; }
	}
	if (o.key) {
		wbx_mount_file_path(g_host, harness_key_name, o.key, &r);
		if (r.error_message[0]) { fprintf(stderr, "mount %s: %s\n", o.key, r.error_message); return 1; }
	}
	harness_slots(base, o.savedata ? harness_base(o.savedata) : NULL, o.dlc ? harness_base(o.dlc) : NULL, slots, sizeof slots);
	harness_settings(&o, settings, sizeof settings);
	if (!mount_text("slots", slots) || !mount_text("settings", settings)) return 1;
	wbx_activate_host(g_host, &r);

	/* the GPU bridge, handed over before Init */
	char glerr[256] = "";
	if (chimera_gl_host_init(glerr, sizeof glerr) != 0) { fprintf(stderr, "gpu bridge: %s\n", glerr); return 5; }
	fprintf(stderr, "gpu bridge: %s\n", chimera_gl_host_description());
	void (*set_bridge)(uint64_t) = (void (*)(uint64_t))proc("SetGpuBridge");
	mb_return gr;
	wbx_get_callback_addr(g_host, (mb_external_callback)chimera_gl_host_dispatch, 0, &gr);
	if (!gr.data) { fprintf(stderr, "gpu bridge: could not register the callback\n"); return 5; }
	set_bridge((uint64_t)gr.data);

	/* on Linux the guest's calling convention is the host's: the exports are
	 * called as plain functions */
	const struct harness_core c = {
		(int (*)(void))proc("Init"),
		(const char *(*)(void))proc("GetLoadError"),
		(void (*)(uint64_t))proc("FrameAdvance"),
		(uint32_t *(*)(void))proc("GetVideoBgra"),
		(int (*)(void))proc("GetVideoWidth"),
		(int (*)(void))proc("GetVideoHeight"),
		(uint64_t (*)(void))proc("GetFrameCount"),
		(uint64_t (*)(void))proc("GetClockCycles"),
		(uint64_t (*)(void))proc("GetTaskSwitches"),
		(uint64_t (*)(void))proc("GetRamDigest"),
		(const uint8_t *(*)(void))proc("GetMailbox"),
		(const uint64_t *(*)(void))proc("GetMemoryRanges"),
		do_seal, do_save, do_load, chimera_gl_host_state_loaded,
		(void (*)(int32_t, int32_t))proc("SetAxis"),
		(int16_t *(*)(void))proc("GetAudio"),
		(int (*)(void))proc("GetAudioSampleCount"),
		(int (*)(void))proc("InputWasRead"),
		(int32_t (*)(void))proc("GetSaveDataFileCount"),
		(const char *(*)(int32_t))proc("GetSaveDataFileName"),
		(int64_t (*)(int32_t))proc("GetSaveDataFileSize"),
		(const uint8_t *(*)(int32_t))proc("GetSaveDataFileBuffer"),
	};
	const int rc = harness_run(&c, &o);
	long last = 0;
	const unsigned long unhandled = chimera_gl_host_unhandled(&last);
	if (unhandled)
		fprintf(stderr, "gpu bridge: %lu calls had no case (last opcode %ld)\n", unhandled, last);
	fflush(stdout);
	_Exit(rc);
}
