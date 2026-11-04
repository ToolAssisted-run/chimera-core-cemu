/*
 * draw: a Wii U program for the Cemu core's tests of the console's GPU
 * library (GX2), which is what games draw with. Each frame it clears a
 * colour buffer and draws three shapes into it with a vertex and a pixel
 * shader of its own: a red triangle that moves with the frame count and
 * the left stick, a green square drawn as GX2's QUADS from the vertex
 * buffer in order, and a yellow square drawn as QUADS through an index
 * buffer. It copies the buffer to the TV and the GamePad and swaps.
 *
 * The shaders are R700 machine code written by hand below (GX2's shader
 * compiler is not free software): the vertex shader calls the fetch shader
 * GX2 makes for the attributes and exports the position (register 1) and
 * the colour (register 2); the pixel shader exports the colour it is given.
 * The GPU reads its code little-endian, so each word is swapped.
 *
 * It asks how many of the triangle's pixels were drawn (an occlusion query
 * read by the CPU), after GX2DrawDone has waited for the frame's commands.
 *
 * Its record in the "mailbox": the frame count, a hash of what it read of
 * the GamePad, of the swap counts GX2 reports and of the query's answers, the
 * buttons held, the swap count, the query's last answer (0 while none came),
 * the stick.
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
	uint32_t hold;
	uint32_t swaps;
	uint32_t samples;
	uint32_t stick;
} Mailbox;

static Mailbox mailbox __attribute__((aligned(64)));

#define LE(x) __builtin_bswap32(x)

/* GX2's occlusion queries (wut has no header for them) */
#define GX2_QUERY_TYPE_OCCLUSION_CPU 0
void GX2QueryBegin(uint32_t type, void* query);
void GX2QueryEnd(uint32_t type, void* query);
BOOL GX2QueryGetOcclusionResult(void* query, uint64_t* result);
static uint8_t query[0x40] __attribute__((aligned(64)));

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

#define VERTICES 11
static Vertex vertices[VERTICES] __attribute__((aligned(256)));
static uint16_t indices[4] __attribute__((aligned(256))) = {7, 8, 9, 10};

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

	GX2SetTVEnable(TRUE);
	GX2SetDRCEnable(TRUE);
	OSReport("draw: started, mailbox at %p\n", &mailbox);

	for (;;)
	{
		VPADStatus status;
		VPADReadError error = VPAD_READ_SUCCESS;
		memset(&status, 0, sizeof(status));
		float sx = 0.0f, sy = 0.0f;
		if (VPADRead(VPAD_CHAN_0, &status, 1, &error) > 0 && error == VPAD_READ_SUCCESS)
		{
			mailbox.hold = status.hold;
			sx = status.leftStick.x;
			sy = status.leftStick.y;
			mailbox.stick = (uint32_t)(int32_t)(sx * 1000.0f) << 16 | ((uint32_t)(int32_t)(sy * 1000.0f) & 0xFFFF);
		}

		/* the triangle moves across with the frames and with the stick */
		const uint32_t f = mailbox.frame;
		const float tx = (float)((int)(f % 120) - 60) / 120.0f + sx * 0.25f;
		const float ty = sy * 0.25f;
		const float tri[3][2] = {{-0.3f + tx, -0.3f + ty}, {0.3f + tx, -0.3f + ty}, {tx, 0.4f + ty}};
		for (int i = 0; i < 3; i++)
		{
			vertices[i].x = tri[i][0];
			vertices[i].y = tri[i][1];
			vertices[i].r = 1.0f;
			vertices[i].g = 0.0f;
			vertices[i].b = 0.0f;
			vertices[i].a = 1.0f;
		}
		square(&vertices[3], -0.9f, 0.5f, 0.3f, 0.0f, 1.0f, 0.0f);
		square(&vertices[7], 0.6f, -0.8f, 0.3f, 1.0f, 1.0f, 0.0f);
		GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, vertices, sizeof(vertices));
		GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, indices, sizeof(indices));

		GX2ClearColor(&cb, 0.1f, 0.2f, (float)(f % 64) / 64.0f, 1.0f);
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
		GX2QueryBegin(GX2_QUERY_TYPE_OCCLUSION_CPU, query);
		GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, 3, 0, 1);
		GX2QueryEnd(GX2_QUERY_TYPE_OCCLUSION_CPU, query);
		GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 3, 1);
		GX2DrawIndexedEx(GX2_PRIMITIVE_MODE_QUADS, 4, GX2_INDEX_TYPE_U16, indices, 0, 1);

		GX2CopyColorBufferToScanBuffer(&cb, GX2_SCAN_TARGET_TV);
		GX2CopyColorBufferToScanBuffer(&cb, GX2_SCAN_TARGET_DRC);
		GX2SwapScanBuffers();
		GX2Flush();
		GX2DrawDone();
		uint64_t samples = 0;
		if (GX2QueryGetOcclusionResult(query, &samples))
			mailbox.samples = (uint32_t)samples;
		GX2WaitForVsync();

		uint32_t swaps = 0, flips = 0;
		OSTime lastFlip = 0, lastVsync = 0;
		GX2GetSwapStatus(&swaps, &flips, &lastFlip, &lastVsync);
		mailbox.swaps = swaps;
		mailbox.hash = mix(mailbox.hash, mailbox.samples);
		mailbox.hash = mix(mailbox.hash, mailbox.hold);
		mailbox.hash = mix(mailbox.hash, mailbox.stick);
		mailbox.hash = mix(mailbox.hash, swaps ^ flips << 16);
		mailbox.hash = mix(mailbox.hash, (uint32_t)(lastFlip - lastVsync));
		mailbox.frame++;
	}
	return 0;
}
