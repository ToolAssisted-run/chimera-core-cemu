/* The test harness, shared by run-native (the core's exports called
 * directly) and run-wbx (the same exports through miniBox), so that both
 * print the same lines from the same loop and differ only by the sandbox.
 *
 * usage: <runner> <program> --work <dir> [options]
 *   --frames N          frames to run (default 300)
 *   --every N           a line every N frames (default 60)
 *   --cpu M             interpreter (default) or recompiler
 *   --clock-start S     the console's calendar at power-on, seconds since 1970
 *   --input FILE        lines "F MASK [A0 A1 ...]": from frame F on, these
 *                       buttons held (the core's button order) and these axes
 *                       (the core's axis order; those not given keep their
 *                       neutral values); blank and # lines ignored
 *   --set NAME=VALUE    a setting, as a project would hold it (repeatable)
 *   --savedata ZIP      the project's save data slot: a zip of usr/save/...
 *                       entries, mounted under its own name
 *   --savedata-out DIR  the console's saved games after the last frame, as
 *                       Export Save Data gives them (a file a name)
 *   --fonts DIR         the console's fonts: DIR's CafeCn.ttf, CafeKr.ttf,
 *                       CafeStd.ttf and CafeTw.ttf, mounted as firmware is
 *                       (under their names), the setting console_fonts
 *                       "cemu" unless --set says "console"; without it
 *                       "standin" (Cemu's stand-in)
 *   --mii DIR           the console's Mii resource files DIR has, mounted
 *                       the same way, mii_resource "console"; without it
 *                       "none"
 *   --dlc ZIP, --key F  a NUS game's add-on content, and the common key
 *   --screenshot F=PATH the picture after frame F, as a TGA
 *   --rerecord          a state saved and loaded before every frame (run-wbx)
 *   --save-state FILE   the machine after the last frame (run-wbx)
 *   --state FILE        start from a state another process saved (run-wbx)
 *   --dump-ram FILE     the console's memory at the end (FILE.index names it)
 *   --log               Cemu's log on standard error
 *   --timeout S         give up after S seconds of the host's (default 600)
 *   --stall F,MS        the host sleeps MS milliseconds before frame F (the
 *                       machine must not notice it: the test clock:stall)
 *   --new-context N     a new GPU context before every Nth frame, as a state
 *                       load gives one (the renderer must make itself again)
 *
 * The program is mounted as a project mounts it: under its own name, with
 * "slots" naming it in the "game" slot and "settings" holding the settings.
 * Natively those are files in the work directory, which is made fresh.
 *
 * A line: "frame F clock=C switches=S mailbox=M ram=R video=V WxH
 * audio=N/A lag=L", where C is the machine's clock in core cycles, S the
 * switches between its tasks, M the test program's own record (or "none"),
 * R a digest of the console's memory, V of the picture (of W by H), N the
 * sample pairs the console has sent the TV since the line before and A a
 * digest of them, L the frames since the line before in which it did not
 * read the GamePad. The last line: "end frames=F clock=C ...".
 * SPDX-License-Identifier: MIT */
#pragma once

#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define HARNESS_MAX_SHOTS 64
#define HARNESS_MAX_INPUT 4096
#define HARNESS_MAX_SETS 16
#define HARNESS_AXES 6

struct harness_opts {
	const char *program, *work;
	uint64_t frames, every;
	const char *cpu;
	int64_t clock_start;
	int rerecord, log, timeout;
	uint64_t stall_frame, stall_ms, new_context;
	const char *input, *save_state, *load_state, *dump_ram, *fonts, *savedata, *savedata_out, *dlc, *key, *mii;
	int shots, sets;
	const char *set[HARNESS_MAX_SETS];
	uint64_t shot_frame[HARNESS_MAX_SHOTS];
	const char *shot_path[HARNESS_MAX_SHOTS];
};

struct harness_core {
	int (*init)(void);
	const char *(*load_error)(void);
	void (*frame_advance)(uint64_t);
	uint32_t *(*video)(void);
	int (*width)(void);
	int (*height)(void);
	uint64_t (*frame_count)(void);
	uint64_t (*clock)(void);
	uint64_t (*switches)(void);
	uint64_t (*ram_digest)(void);
	const uint8_t *(*mailbox)(void);
	/* the console's mapped memory: (base, size, pointer) triples, ended by a
	 * size of 0 */
	const uint64_t *(*ranges)(void);
	/* states (null natively): the machine sealed after Init, saved into a
	 * buffer the callee allocates, loaded from one */
	int (*seal)(void);
	int (*save)(uint8_t **buf, size_t *len);
	int (*load)(const uint8_t *buf, size_t len);
	/* a new GPU context, as the frontend gives after a load */
	void (*new_context)(void);
	void (*set_axis)(int32_t, int32_t);
	int16_t *(*audio)(void);
	int (*audio_count)(void);
	int (*input_was_read)(void);
	int32_t (*savedata_count)(void);
	const char *(*savedata_name)(int32_t);
	int64_t (*savedata_size)(int32_t);
	const uint8_t *(*savedata_buffer)(int32_t);
};

static int harness_parse(int argc, char **argv, struct harness_opts *o)
{
	memset(o, 0, sizeof *o);
	o->frames = 300;
	o->every = 60;
	o->timeout = 600;
	o->cpu = "interpreter";
	o->clock_start = 1495889068;
	for (int i = 1; i < argc; i++) {
		const char *a = argv[i];
		if (!strcmp(a, "--work") && i + 1 < argc) o->work = argv[++i];
		else if (!strcmp(a, "--frames") && i + 1 < argc) o->frames = strtoull(argv[++i], NULL, 10);
		else if (!strcmp(a, "--every") && i + 1 < argc) o->every = strtoull(argv[++i], NULL, 10);
		else if (!strcmp(a, "--cpu") && i + 1 < argc) o->cpu = argv[++i];
		else if (!strcmp(a, "--clock-start") && i + 1 < argc) o->clock_start = strtoll(argv[++i], NULL, 10);
		else if (!strcmp(a, "--input") && i + 1 < argc) o->input = argv[++i];
		else if (!strcmp(a, "--fonts") && i + 1 < argc) o->fonts = argv[++i];
		else if (!strcmp(a, "--savedata") && i + 1 < argc) o->savedata = argv[++i];
		else if (!strcmp(a, "--savedata-out") && i + 1 < argc) o->savedata_out = argv[++i];
		else if (!strcmp(a, "--dlc") && i + 1 < argc) o->dlc = argv[++i];
		else if (!strcmp(a, "--key") && i + 1 < argc) o->key = argv[++i];
		else if (!strcmp(a, "--mii") && i + 1 < argc) o->mii = argv[++i];
		else if (!strcmp(a, "--rerecord")) o->rerecord = 1;
		else if (!strcmp(a, "--save-state") && i + 1 < argc) o->save_state = argv[++i];
		else if (!strcmp(a, "--state") && i + 1 < argc) o->load_state = argv[++i];
		else if (!strcmp(a, "--dump-ram") && i + 1 < argc) o->dump_ram = argv[++i];
		else if (!strcmp(a, "--log")) o->log = 1;
		else if (!strcmp(a, "--timeout") && i + 1 < argc) o->timeout = atoi(argv[++i]);
		else if (!strcmp(a, "--new-context") && i + 1 < argc) o->new_context = strtoull(argv[++i], NULL, 10);
		else if (!strcmp(a, "--set") && i + 1 < argc && o->sets < HARNESS_MAX_SETS && strchr(argv[i + 1], '=')) o->set[o->sets++] = argv[++i];
		else if (!strcmp(a, "--stall") && i + 1 < argc) {
			if (sscanf(argv[++i], "%" SCNu64 ",%" SCNu64, &o->stall_frame, &o->stall_ms) != 2) { fprintf(stderr, "--stall wants F,MS\n"); return 0; }
		}
		else if (!strcmp(a, "--screenshot") && i + 1 < argc && o->shots < HARNESS_MAX_SHOTS) {
			char *spec = argv[++i], *eq = strchr(spec, '=');
			if (!eq) { fprintf(stderr, "--screenshot wants F=PATH\n"); return 0; }
			*eq = '\0';
			o->shot_frame[o->shots] = strtoull(spec, NULL, 10);
			o->shot_path[o->shots++] = eq + 1;
		} else if (!o->program && a[0] != '-') o->program = a;
		else { fprintf(stderr, "unknown argument %s\n", a); return 0; }
	}
	if (!o->program || !o->work) {
		fprintf(stderr, "usage: %s <program> --work <dir> [--frames N] [--every N] [--cpu interpreter|recompiler]"
			" [--clock-start S] [--input FILE] [--screenshot F=PATH]... [--rerecord] [--save-state FILE]"
			" [--state FILE] [--dump-ram FILE] [--log] [--timeout S] [--dlc ZIP] [--key FILE] [--mii DIR]\n", argv[0]);
		return 0;
	}
	return 1;
}

static const char *harness_base(const char *path)
{
	const char *s = strrchr(path, '/');
	return s ? s + 1 : path;
}

/* The settings object both flavours read, as a project would mount it: the
 * --set values, and the harness's own for the settings they do not name. */
static int harness_is_set(const struct harness_opts *o, const char *name)
{
	for (int i = 0; i < o->sets; i++)
		if (!strncmp(o->set[i], name, strlen(name)) && o->set[i][strlen(name)] == '=')
			return 1;
	return 0;
}

static void harness_settings(const struct harness_opts *o, char *buf, size_t n)
{
	char clock[32];
	snprintf(clock, sizeof clock, "%" PRId64, o->clock_start);
	const char *defaults[][3] = {
		{ "cpu_mode", o->cpu, "\"" },
		{ "clock_start", clock, "" },
		{ "log", o->log ? "true" : "false", "" },
		{ "console_fonts", o->fonts ? "cemu" : "standin", "\"" },
		{ "mii_resource", o->mii ? "console" : "none", "\"" },
	};
	int at = snprintf(buf, n, "{");
	const char *sep = "";
	for (int i = 0; i < o->sets && at > 0 && (size_t)at < n; i++, sep = ", ") {
		const char *eq = strchr(o->set[i], '=');
		at += snprintf(buf + at, n - (size_t)at, "%s\"%.*s\": \"%s\"", sep, (int)(eq - o->set[i]), o->set[i], eq + 1);
	}
	for (size_t i = 0; i < sizeof defaults / sizeof defaults[0] && at > 0 && (size_t)at < n; i++) {
		if (harness_is_set(o, defaults[i][0])) continue;
		at += snprintf(buf + at, n - (size_t)at, "%s\"%s\": %s%s%s", sep, defaults[i][0], defaults[i][2], defaults[i][1], defaults[i][2]);
		sep = ", ";
	}
	if (at > 0 && (size_t)at < n)
		snprintf(buf + at, n - (size_t)at, "}");
}

/* The console's Mii resource (its system title 0005001b-10056000), the files
 * of it the core knows, mounted under their names: --mii DIR mounts each of
 * them DIR has. */
static const char *const harness_mii_names[] = { "FFLResHigh.dat", "FFLResHighLG.dat", "FFLResMiddle.dat", "FFLResMiddleLG.dat" };
#define HARNESS_MII_FILES (sizeof harness_mii_names / sizeof harness_mii_names[0])

/* The console's fonts, the names they are mounted under. */
static const char *const harness_font_names[4] = { "CafeCn.ttf", "CafeKr.ttf", "CafeStd.ttf", "CafeTw.ttf" };

/* The slot map a project would mount. */
static void harness_slots(const char *game, const char *savedata, const char *dlc, char *buf, size_t n)
{
	int at = snprintf(buf, n, "{\"game\": [\"%s\"]", game);
	if (savedata && at > 0 && (size_t)at < n)
		at += snprintf(buf + at, n - (size_t)at, ", \"savedata\": [\"%s\"]", savedata);
	if (dlc && at > 0 && (size_t)at < n)
		at += snprintf(buf + at, n - (size_t)at, ", \"dlc\": [\"%s\"]", dlc);
	if (at > 0 && (size_t)at < n)
		snprintf(buf + at, n - (size_t)at, "}");
}

/* the common key's firmware id: the name it is mounted under */
static const char *const harness_key_name = "wiiu-common-key.bin";

/* The saved games, as Export Save Data writes them: a file a name under dir. */
static int harness_savedata_out(const struct harness_core *c, const char *dir)
{
	const int32_t count = c->savedata_count();
	for (int32_t i = 0; i < count; i++) {
		const char *name = c->savedata_name(i);
		char path[4096], cmd[8400];
		snprintf(path, sizeof path, "%s/%s", dir, name);
		char *slash = strrchr(path, '/');
		if (slash) {
			*slash = '\0';
			snprintf(cmd, sizeof cmd, "mkdir -p '%s'", path);
			if (system(cmd) != 0) return 0;
			*slash = '/';
		}
		FILE *f = fopen(path, "wb");
		if (!f) return 0;
		fwrite(c->savedata_buffer(i), 1, (size_t)c->savedata_size(i), f);
		fclose(f);
	}
	return 1;
}

/* A fresh work directory: made, or emptied when it carries our mark. */
static int harness_fresh_work(const char *work)
{
	char mark[4096], cmd[8400];
	snprintf(mark, sizeof mark, "%s/.cemu-harness", work);
	struct stat st;
	if (stat(work, &st) == 0) {
		if (stat(mark, &st) != 0) {
			fprintf(stderr, "%s exists and is not a work directory of ours\n", work);
			return 0;
		}
		snprintf(cmd, sizeof cmd, "find '%s' -mindepth 1 ! -name .cemu-harness -delete", work);
		return system(cmd) == 0;
	}
	snprintf(cmd, sizeof cmd, "mkdir -p '%s' && touch '%s'", work, mark);
	return system(cmd) == 0;
}

static uint64_t harness_fnv1a(const uint8_t *p, size_t n)
{
	uint64_t h = 0xcbf29ce484222325ull;
	for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 0x100000001b3ull; }
	return h;
}

/* uncompressed 32-bit TGA, top row first */
static int harness_tga(const char *path, const uint8_t *bgra, int w, int h)
{
	FILE *f = fopen(path, "wb");
	if (!f) return 0;
	uint8_t hd[18] = { 0 };
	hd[2] = 2; hd[12] = w & 0xff; hd[13] = w >> 8; hd[14] = h & 0xff; hd[15] = h >> 8;
	hd[16] = 32; hd[17] = 0x28;
	fwrite(hd, 1, sizeof hd, f);
	fwrite(bgra, 1, (size_t)w * h * 4, f);
	return fclose(f) == 0;
}

struct harness_input { uint64_t from, mask; int axes; int32_t axis[HARNESS_AXES]; };
static int harness_read_input(const char *path, struct harness_input *in, int *n)
{
	*n = 0;
	FILE *f = fopen(path, "r");
	if (!f) { fprintf(stderr, "cannot read %s\n", path); return 0; }
	char line[512];
	while (fgets(line, sizeof line, f)) {
		char *p = line;
		while (*p == ' ' || *p == '\t') p++;
		if (*p == '#' || *p == '\n' || *p == '\0') continue;
		if (*n == HARNESS_MAX_INPUT) { fclose(f); return 0; }
		char *end;
		in[*n].from = strtoull(p, &end, 10);
		in[*n].mask = strtoull(end, &end, 0);
		in[*n].axes = 0;
		while (in[*n].axes < HARNESS_AXES) {
			char *next;
			const long v = strtol(end, &next, 0);
			if (next == end) break;
			in[*n].axis[in[*n].axes++] = (int32_t)v;
			end = next;
		}
		(*n)++;
	}
	fclose(f);
	return 1;
}

static uint32_t harness_be32(const uint8_t *p)
{
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

/* the sound and the lag frames since the line before */
static uint64_t harness_audio_pairs, harness_audio_hash = 0xcbf29ce484222325ull, harness_lag;

static void harness_line(const struct harness_core *c, const char *head)
{
	const int w = c->width(), h = c->height();
	const uint8_t *pic = (const uint8_t *)c->video();
	const uint8_t *mb = c->mailbox();
	printf("%s clock=%" PRIu64 " switches=%" PRIu64, head, c->clock(), c->switches());
	if (mb)
		printf(" mailbox=%u/%08x/%u/%u/%u/%u", harness_be32(mb + 16), harness_be32(mb + 20), harness_be32(mb + 24),
			harness_be32(mb + 28), harness_be32(mb + 32), harness_be32(mb + 36));
	else
		printf(" mailbox=none");
	printf(" ram=%016" PRIx64 " video=%016" PRIx64 " %dx%d audio=%" PRIu64 "/%016" PRIx64 " lag=%" PRIu64 "\n",
		c->ram_digest(), pic ? harness_fnv1a(pic, (size_t)w * h * 4) : 0, w, h, harness_audio_pairs, harness_audio_hash,
		harness_lag);
	fflush(stdout);
	harness_audio_pairs = harness_lag = 0;
	harness_audio_hash = 0xcbf29ce484222325ull;
}

/* Init, the frames, the lines. Returns the process's exit status. */
static int harness_run(const struct harness_core *c, const struct harness_opts *o)
{
	alarm((unsigned)o->timeout);
	if (!c->init()) {
		const char *why = c->load_error();
		fprintf(stderr, "Init failed: %s\n", why && *why ? why : "the machine died (see above)");
		return 6;
	}
	const int states = o->rerecord || o->save_state || o->load_state;
	if (states && (!c->seal || !c->save || !c->load)) {
		fprintf(stderr, "this runner has no states\n");
		return 2;
	}
	if (states && !c->seal()) {
		fprintf(stderr, "seal failed\n");
		return 6;
	}
	if (o->load_state) {
		FILE *f = fopen(o->load_state, "rb");
		if (!f) { fprintf(stderr, "cannot read %s\n", o->load_state); return 6; }
		fseek(f, 0, SEEK_END);
		const size_t len = (size_t)ftell(f);
		fseek(f, 0, SEEK_SET);
		uint8_t *buf = (uint8_t *)malloc(len);
		if (!buf || fread(buf, 1, len, f) != len || !c->load(buf, len)) { fprintf(stderr, "cannot load %s\n", o->load_state); return 6; }
		fclose(f);
		free(buf);
		char head[64];
		snprintf(head, sizeof head, "loaded frame %" PRIu64, c->frame_count());
		harness_line(c, head);
	}
	static struct harness_input script[HARNESS_MAX_INPUT];
	int script_n = 0;
	if (o->input && !harness_read_input(o->input, script, &script_n)) return 6;
	/* The tests' negative control (HARNESS_BREAK_STALE_STATE): the state a
	 * rerecord loads half way, and the one --save-state writes, are the
	 * machine a frame earlier than they should be. */
	const int stale = getenv("HARNESS_BREAK_STALE_STATE") != NULL;
	uint8_t *prev = NULL;
	size_t prev_len = 0;
	for (uint64_t i = 1; i <= o->frames; i++) {
		if (o->rerecord) {
			uint8_t *buf = NULL;
			size_t len = 0;
			if (!c->save(&buf, &len)) { fprintf(stderr, "rerecord failed\n"); return 6; }
			const int use_prev = stale && prev && i == o->frames / 2;
			if (!c->load(use_prev ? prev : buf, use_prev ? prev_len : len)) { fprintf(stderr, "rerecord failed\n"); return 6; }
			free(prev);
			prev = buf;
			prev_len = len;
		} else if (stale && o->save_state && i == o->frames) {
			free(prev);
			if (!c->save(&prev, &prev_len)) { fprintf(stderr, "save failed\n"); return 6; }
		}
		const uint64_t next = c->frame_count() + 1;
		if (o->stall_ms && next == o->stall_frame)
			usleep((useconds_t)(o->stall_ms * 1000));
		if (o->new_context && next % o->new_context == 0)
			c->new_context();
		/* the axes' neutral values: the sticks centred, the touch point in the
		 * middle */
		uint64_t mask = 0;
		int32_t axis[HARNESS_AXES] = { 0, 0, 0, 0, 32768, 32768 };
		for (int s = 0; s < script_n; s++)
			if (script[s].from <= next) {
				mask = script[s].mask;
				for (int a = 0; a < script[s].axes; a++) axis[a] = script[s].axis[a];
			}
		for (int a = 0; a < HARNESS_AXES; a++)
			c->set_axis(a, axis[a]);
		c->frame_advance(mask);
		{
			const int pairs = c->audio_count();
			const int16_t *samples = c->audio();
			if (pairs > 0 && samples) {
				harness_audio_pairs += (uint64_t)pairs;
				const uint8_t *b = (const uint8_t *)samples;
				for (size_t k = 0; k < (size_t)pairs * 4; k++) { harness_audio_hash ^= b[k]; harness_audio_hash *= 0x100000001b3ull; }
			}
			if (!c->input_was_read())
				harness_lag++;
		}
		const uint64_t f = c->frame_count();
		if (!c->video()) {
			fprintf(stderr, "the machine died in frame %" PRIu64 "\n", f);
			return 7;
		}
		if (o->every && f % o->every == 0) {
			char head[64];
			snprintf(head, sizeof head, "frame %" PRIu64, f);
			harness_line(c, head);
		}
		for (int s = 0; s < o->shots; s++)
			if (o->shot_frame[s] == f && !harness_tga(o->shot_path[s], (const uint8_t *)c->video(), c->width(), c->height()))
				fprintf(stderr, "could not write %s\n", o->shot_path[s]);
	}
	char head[64];
	snprintf(head, sizeof head, "end frames=%" PRIu64, c->frame_count());
	harness_line(c, head);
	if (o->dump_ram) {
		char index[4096];
		snprintf(index, sizeof index, "%s.index", o->dump_ram);
		FILE *fi = fopen(index, "w"), *fd = fopen(o->dump_ram, "wb");
		if (!fi || !fd) { fprintf(stderr, "cannot write %s\n", o->dump_ram); return 6; }
		for (const uint64_t *r = c->ranges(); r[1]; r += 3) {
			fprintf(fi, "range %08" PRIx64 " %" PRIx64 "\n", r[0], r[1]);
			fwrite((const void *)(uintptr_t)r[2], 1, (size_t)r[1], fd);
		}
		fclose(fi);
		fclose(fd);
	}
	if (o->savedata_out && !harness_savedata_out(c, o->savedata_out)) {
		fprintf(stderr, "cannot write the save data to %s\n", o->savedata_out);
		return 6;
	}
	if (o->save_state) {
		uint8_t *buf = NULL;
		size_t len = 0;
		FILE *f = fopen(o->save_state, "wb");
		if (stale && prev) {
			buf = prev;
			len = prev_len;
			prev = NULL;
		} else if (!c->save(&buf, &len))
			buf = NULL;
		if (!f || !buf || fwrite(buf, 1, len, f) != len) { fprintf(stderr, "cannot write %s\n", o->save_state); return 6; }
		fclose(f);
		free(buf);
	}
	free(prev);
	return 0;
}
