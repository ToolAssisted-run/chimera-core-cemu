/*
 * readback: a Wii U program for the Cemu core's tests of what the console's
 * GPU writes back into its memory. Games draw into colour buffers the CPU
 * reads (a buffer in linear layout, which Cemu copies back from the host's
 * GPU into the console's memory, as the console's GPU would have written
 * it). Each frame it clears a small linear buffer (64x16) to a colour that
 * changes with the frame count, draws a green rectangle over its left half,
 * then draws the TV's picture (which makes the GPU leave the small buffer),
 * waits for the GPU (GX2DrawDone) and reads two pixels of the small buffer
 * with the CPU.
 *
 * The shapes are drawn with the draw program's shaders (tests/homebrew/draw)
 * and have no edge near a pixel's centre, so every card draws them alike.
 *
 * Its record in the "mailbox": the frame count, a hash of the whole small
 * buffer as the CPU read it every frame, the pixel inside the rectangle and
 * the pixel outside it (as 0xRRGGBBAA) of the last frame, how many frames
 * the CPU read this frame's clear colour, and how many it read the previous
 * frame's.
 */
#include <coreinit/cache.h>
#include <coreinit/debug.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/time.h>
#include <gx2/clear.h>
#include <gx2/context.h>
#include <gx2/display.h>
#include <gx2/draw.h>
#include <gx2/enum.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/shaders.h>
#include <gx2/state.h>
#include <gx2/surface.h>
#include <gx2/swap.h>
#include <gx2/utils.h>
#include <vpad/input.h>

#include <string.h>

typedef struct
{
	char magic[16];             /* "CHIMERA-MAILBOX" */
	uint32_t frame;
	uint32_t hash;
	uint32_t inside;
	uint32_t outside;
	uint32_t fresh;
	uint32_t late;
} Mailbox;

static Mailbox mailbox __attribute__((aligned(64)));

#define LE(x) __builtin_bswap32(x)

/* CF instructions (two words each). Export: word0 = ARRAY_BASE | TYPE << 13
 * | RW_GPR << 15; word1 = swizzle xyzw (0x688) | CF_INST << 23 | END << 21 |
 * BARRIER << 31. CF_INST: CALL_FS 0x13, EXPORT_DONE 0x28. */
static const uint32_t vs_code[] __attribute__((aligned(256))) = {
	LE(0x00000000), LE(0x13u << 23),                                   /* CALL_FS */
	LE(60 | 1 << 13 | 1 << 15), LE(0x688 | 0x28u << 23 | 1u << 31),    /* EXPORT_DONE POS0 <- R1 */
	LE(0 | 2 << 13 | 2 << 15), LE(0x688 | 0x28u << 23 | 1 << 21 | 1u << 31), /* EXPORT_DONE PARAM0 <- R2, END */
	0, 0,
};
static const uint32_t ps_code[] __attribute__((aligned(256))) = {
	LE(0 | 0 << 13 | 0 << 15), LE(0x688 | 0x28u << 23 | 1 << 21 | 1u << 31), /* EXPORT_DONE PIXEL0 <- R0, END */
	0, 0,
};

typedef struct
{
	float x, y;
	float r, g, b, a;
} Vertex;

/* the small buffer's rectangle (its left half), then the TV's square */
#define VERTICES 8
static Vertex vertices[VERTICES] __attribute__((aligned(256)));

static void square(Vertex* v, float x0, float y0, float size, float r, float g, float b)
{
	const float xs[4] = {x0, x0 + size, x0 + size, x0};
	const float ys[4] = {y0, y0, y0 + size, y0 + size};
	for (int i = 0; i < 4; i++)
	{
		v[i].x = xs[i];
		v[i].y = ys[i];
		v[i].r = r;
		v[i].g = g;
		v[i].b = b;
		v[i].a = 1.0f;
	}
}

static uint32_t mix(uint32_t h, uint32_t v)
{
	h ^= v;
	h *= 0x01000193u;
	return h ^ (h >> 15);
}

/* the clear colour of frame f, as the buffer holds it (0xRRGGBBAA) */
static uint32_t clear_of(uint32_t f)
{
	return (f % 200) << 24 | ((f * 7) % 256) << 16 | 0x80u << 8 | 0xFFu;
}

static uint32_t pixel(const uint8_t* image, uint32_t pitch, uint32_t x, uint32_t y)
{
	const uint8_t* p = image + (y * pitch + x) * 4;
	return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
}

int main(int argc, char** argv)
{
	memcpy(mailbox.magic, "CHIMERA-MAILBOX", 16);
	mailbox.hash = 0x811C9DC5u;

	void* cmdbuf = MEMAllocFromDefaultHeapEx(0x400000, 0x40);
	uint32_t init[] = {GX2_INIT_CMD_BUF_BASE, (uint32_t)cmdbuf, GX2_INIT_CMD_BUF_POOL_SIZE, 0x400000,
		GX2_INIT_ARGC, (uint32_t)argc, GX2_INIT_ARGV, (uint32_t)argv, GX2_INIT_END};
	GX2Init(init);

	/* the two screens' scan buffers */
	uint32_t tvSize, drcSize, unk;
	GX2CalcTVSize(GX2_TV_RENDER_MODE_WIDE_720P, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_BUFFERING_MODE_DOUBLE, &tvSize, &unk);
	GX2CalcDRCSize(GX2_DRC_RENDER_MODE_SINGLE, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_BUFFERING_MODE_DOUBLE, &drcSize, &unk);
	void* tvScan = MEMAllocFromDefaultHeapEx(tvSize, 0x1000);
	void* drcScan = MEMAllocFromDefaultHeapEx(drcSize, 0x1000);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, tvScan, tvSize);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, drcScan, drcSize);
	GX2SetTVBuffer(tvScan, tvSize, GX2_TV_RENDER_MODE_WIDE_720P, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_BUFFERING_MODE_DOUBLE);
	GX2SetTVScale(1280, 720);
	GX2SetDRCBuffer(drcScan, drcSize, GX2_DRC_RENDER_MODE_SINGLE, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_BUFFERING_MODE_DOUBLE);
	GX2SetDRCScale(854, 480);

	/* the colour buffer drawn into */
	GX2ColorBuffer cb;
	memset(&cb, 0, sizeof(cb));
	cb.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
	cb.surface.width = 1280;
	cb.surface.height = 720;
	cb.surface.depth = 1;
	cb.surface.mipLevels = 1;
	cb.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
	cb.surface.aa = GX2_AA_MODE1X;
	cb.surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
	cb.surface.tileMode = GX2_TILE_MODE_DEFAULT;
	cb.viewNumSlices = 1;
	GX2CalcSurfaceSizeAndAlignment(&cb.surface);
	GX2InitColorBufferRegs(&cb);
	cb.surface.image = MEMAllocFromDefaultHeapEx(cb.surface.imageSize, cb.surface.alignment);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, cb.surface.image, cb.surface.imageSize);

	GX2ContextState* ctx = (GX2ContextState*)MEMAllocFromDefaultHeapEx(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT);
	GX2SetupContextStateEx(ctx, TRUE);
	GX2SetContextState(ctx);
	GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_REGISTER);

	/* the shaders: the fetch shader GX2 makes, and the two above */
	GX2AttribStream attribs[2];
	memset(attribs, 0, sizeof(attribs));
	attribs[0].location = 0;
	attribs[0].offset = 0;
	attribs[0].format = GX2_ATTRIB_FORMAT_FLOAT_32_32;
	attribs[0].type = GX2_ATTRIB_INDEX_PER_VERTEX;
	attribs[0].mask = GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
	attribs[0].endianSwap = GX2_ENDIAN_SWAP_DEFAULT;
	attribs[1].location = 1;
	attribs[1].offset = 8;
	attribs[1].format = GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32;
	attribs[1].type = GX2_ATTRIB_INDEX_PER_VERTEX;
	attribs[1].mask = GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_Z, GX2_SQ_SEL_W);
	attribs[1].endianSwap = GX2_ENDIAN_SWAP_DEFAULT;
	GX2FetchShader fs;
	const uint32_t fsSize = GX2CalcFetchShaderSizeEx(2, GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
	uint8_t* fsCode = (uint8_t*)MEMAllocFromDefaultHeapEx(fsSize, GX2_SHADER_PROGRAM_ALIGNMENT);
	GX2InitFetchShaderEx(&fs, fsCode, 2, attribs, GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, fsCode, fsSize);

	GX2VertexShader vs;
	memset(&vs, 0, sizeof(vs));
	vs.regs.sq_pgm_resources_vs = 3 | 1 << 8; /* three registers, a stack entry for the call */
	vs.regs.spi_vs_out_config = 0;            /* one parameter exported */
	vs.regs.num_spi_vs_out_id = 1;
	vs.regs.spi_vs_out_id[0] = 0xFFFFFF00;    /* parameter 0 is semantic 0 */
	for (int i = 1; i < 10; i++)
		vs.regs.spi_vs_out_id[i] = 0xFFFFFFFF;
	vs.regs.sq_vtx_semantic_clear = 0xFFFFFFFC;
	vs.regs.num_sq_vtx_semantic = 2;          /* semantic 0 into R1, semantic 1 into R2 */
	vs.regs.sq_vtx_semantic[0] = 0;
	vs.regs.sq_vtx_semantic[1] = 1;
	for (int i = 2; i < 32; i++)
		vs.regs.sq_vtx_semantic[i] = 0xFF;
	vs.regs.vgt_vertex_reuse_block_cntl = 14;
	vs.regs.vgt_hos_reuse_depth = 16;
	vs.size = sizeof(vs_code);
	vs.program = (void*)vs_code;
	vs.mode = GX2_SHADER_MODE_UNIFORM_REGISTER;
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, (void*)vs_code, sizeof(vs_code));

	GX2PixelShader ps;
	memset(&ps, 0, sizeof(ps));
	ps.regs.sq_pgm_resources_ps = 1;          /* one register */
	ps.regs.sq_pgm_exports_ps = 2;            /* one colour */
	ps.regs.spi_ps_in_control_0 = 1 | 1 << 28; /* one input, perspective */
	ps.regs.num_spi_ps_input_cntl = 1;
	ps.regs.spi_ps_input_cntls[0] = 0 | 1 << 8; /* semantic 0 */
	ps.regs.cb_shader_mask = 0xF;
	ps.regs.cb_shader_control = 1;
	ps.size = sizeof(ps_code);
	ps.program = (void*)ps_code;
	ps.mode = GX2_SHADER_MODE_UNIFORM_REGISTER;
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, (void*)ps_code, sizeof(ps_code));

	/* the small buffer the CPU reads: linear, as the CPU can read it */
	GX2ColorBuffer lb;
	memset(&lb, 0, sizeof(lb));
	lb.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
	lb.surface.width = 64;
	lb.surface.height = 16;
	lb.surface.depth = 1;
	lb.surface.mipLevels = 1;
	lb.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
	lb.surface.aa = GX2_AA_MODE1X;
	lb.surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
	lb.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
	lb.viewNumSlices = 1;
	GX2CalcSurfaceSizeAndAlignment(&lb.surface);
	GX2InitColorBufferRegs(&lb);
	lb.surface.image = MEMAllocFromDefaultHeapEx(lb.surface.imageSize, lb.surface.alignment);
	memset(lb.surface.image, 0, lb.surface.imageSize);
	DCFlushRange(lb.surface.image, lb.surface.imageSize);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, lb.surface.image, lb.surface.imageSize);

	square(&vertices[0], -1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 0.0f);
	vertices[1].x = 0.0f; vertices[2].x = 0.0f; /* the left half: x -1..0, y -1..1 */
	vertices[2].y = 1.0f; vertices[3].y = 1.0f;
	square(&vertices[4], -0.5f, -0.5f, 1.0f, 0.0f, 1.0f, 0.0f);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, vertices, sizeof(vertices));

	GX2SetTVEnable(TRUE);
	GX2SetDRCEnable(TRUE);
	OSReport("readback: started, mailbox at %p\n", &mailbox);

	for (;;)
	{
		const uint32_t f = mailbox.frame;
		const uint32_t c = clear_of(f);
		GX2ClearColor(&lb, (float)(c >> 24) / 255.0f, (float)((c >> 16) & 0xFF) / 255.0f, (float)((c >> 8) & 0xFF) / 255.0f, 1.0f);
		GX2SetContextState(ctx);
		GX2SetColorBuffer(&lb, GX2_RENDER_TARGET_0);
		GX2SetViewport(0, 0, 64, 16, 0.0f, 1.0f);
		GX2SetScissor(0, 0, 64, 16);
		GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
		GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
		GX2SetFetchShader(&fs);
		GX2SetVertexShader(&vs);
		GX2SetPixelShader(&ps);
		GX2SetAttribBuffer(0, sizeof(vertices), sizeof(Vertex), vertices);
		GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);

		/* the TV's picture: the GPU leaves the small buffer for it */
		GX2ClearColor(&cb, 0.1f, 0.2f, 0.3f, 1.0f);
		GX2SetContextState(ctx);
		GX2SetColorBuffer(&cb, GX2_RENDER_TARGET_0);
		GX2SetViewport(0, 0, 1280, 720, 0.0f, 1.0f);
		GX2SetScissor(0, 0, 1280, 720);
		GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
		GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
		GX2SetFetchShader(&fs);
		GX2SetVertexShader(&vs);
		GX2SetPixelShader(&ps);
		GX2SetAttribBuffer(0, sizeof(vertices), sizeof(Vertex), vertices);
		GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 4, 1);
		GX2CopyColorBufferToScanBuffer(&cb, GX2_SCAN_TARGET_TV);
		GX2CopyColorBufferToScanBuffer(&cb, GX2_SCAN_TARGET_DRC);
		GX2SwapScanBuffers();
		GX2Flush();
		GX2DrawDone();

		/* what the CPU reads of the small buffer */
		DCInvalidateRange(lb.surface.image, lb.surface.imageSize);
		const uint8_t* image = (const uint8_t*)lb.surface.image;
		for (uint32_t y = 0; y < 16; y++)
			for (uint32_t x = 0; x < 64; x++)
				mailbox.hash = mix(mailbox.hash, pixel(image, lb.surface.pitch, x, y));
		mailbox.inside = pixel(image, lb.surface.pitch, 8, 8);
		mailbox.outside = pixel(image, lb.surface.pitch, 56, 8);
		if (mailbox.outside == c)
			mailbox.fresh++;
		else if (f > 0 && mailbox.outside == clear_of(f - 1))
			mailbox.late++;
		GX2WaitForVsync();
		mailbox.frame++;
	}
	return 0;
}
