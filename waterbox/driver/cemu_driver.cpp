// The machine: Cemu started the way its own frontend starts it, without a
// window, with the settings this core chooses, run a frame at a time on the
// machine's own clock (chimera_vsched.h). Everything Cemu keeps on a disk -
// its settings, the console's storage (mlc01), the program - is in the
// machine's own filesystem (memfs.h), under /chimera.
#include "cemu_driver.h"
#include "chimera_vsched.h"
#include "chimera_context.h"
#include "memfs.h"
#include "gl-bridge.h"
#include "chimera_gpu.h"
#include "chimera_input.h"
#include "chimera_nus.h"
#include "chimera_profiles.h"
#include "chimera_savedata.h"

#include "Cafe/CafeSystem.h"
#include "Cemu/Logging/CemuLogging.h"
#include "Cafe/HW/MMU/MMU.h"
#include "Cafe/HW/Espresso/PPCState.h"
#include "Cafe/HW/Latte/Core/Latte.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.h"
#include "Cafe/Filesystem/FST/FST.h"
#include "Cafe/TitleList/TitleInfo.h"
#include "Cafe/TitleList/TitleList.h"
#include "Cemu/ncrypto/ncrypto.h"
#include "config/ActiveSettings.h"
#include "config/CemuConfig.h"
#include "util/crypto/aes128.h"
#include "util/Fiber/Fiber.h"
#include "WindowSystem.h"

void PPCTimer_init();

// upstream's thread-local variables, which every task has its own copy of
extern thread_local PPCInterpreter_t* ppcInterpreterCurrentInstance;
extern thread_local sint32 s_schedulerLockCount;
extern thread_local Fiber* sCurrentFiber;
namespace coreinit
{
	extern thread_local uint32 t_assignedCoreIndex;
	extern thread_local Fiber* t_schedulerFiber;
}

// Cemu's main.cpp has this; a core has no main.cpp
std::atomic_bool g_isGPUInitFinished = false;

namespace
{
	CPUMode s_cpuMode = CPUMode::SinglecoreInterpreter;
	int64_t s_clockStart = 1495889068;
	bool s_log = false;
	bool s_fakeOcclusion = false;
	uint64_t s_frame = 0;
	bool s_rendering = true;
	// a frame ends this long after its vertical blank
	constexpr uint64_t kFrameEndAfterVsync = chimera::vsched::kCoreClock / 500000; // 2 us

	int64_t CalendarSeconds()
	{
		return s_clockStart + (int64_t)(chimera::vsched::NowNs() / 1000000000ull);
	}

	// ---- the screens, through the GPU bridge -----------------------------

	// GL entry points Cemu's own list does not have
	using PFN_ReadPixels = void (*)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
	using PFN_BlitFramebuffer = void (*)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);
	using PFN_ClearTexImage = void (*)(GLuint, GLint, GLenum, GLenum, const void*);

	// The picture last presented, read back when asked. Not the console's
	// memory, and no part of what the console can see.
	constexpr int kPictureMaxWidth = 1280, kPictureMaxHeight = 720 + chimera::kPadHeight;
	uint32_t* s_picture = nullptr;
	bool s_pictureStale = true;

	// The windows Cemu would draw in, the TV's and the GamePad's: each a
	// texture it draws into and a copy of what it last presented.
	struct Screen
	{
		int width, height;
		GLuint fbo = 0, color = 0, presentedFbo = 0, presented = 0;
	};

	class Canvas : public OpenGLCanvasCallbacks
	{
	public:
		bool HasPadViewOpen() const override { return chimera::ShowsGamePad(); }

		// the bridge binds its one context itself; this says which window's
		// stand-in the renderer's "framebuffer 0" is
		bool MakeCurrent(bool padView) override
		{
			m_padCurrent = padView;
			return true;
		}

		uint32 BackbufferFramebuffer() const override
		{
			MakeTargets();
			return m_padCurrent ? m_pad.fbo : m_tv.fbo;
		}

		// The context moved and the renderer was made again: these objects
		// were the old context's. Let go of the names (they name nothing
		// now), and make them again when next asked.
		void Forget()
		{
			using namespace CemuGL;
			for (Screen* screen : {&m_tv, &m_pad})
			{
				if (screen->fbo)
				{
					GLuint fbos[2] = {screen->fbo, screen->presentedFbo}, textures[2] = {screen->color, screen->presented};
					glDeleteFramebuffers(2, fbos);
					glDeleteTextures(2, textures);
				}
				screen->fbo = screen->color = screen->presentedFbo = screen->presented = 0;
			}
			m_context = chimera_gl_context_id();
		}

		// Is the presented picture this context's? (Not before the renderer
		// was made again after a load.)
		bool Current() const
		{
			return m_context == chimera_gl_context_id();
		}

		// A present: each window's stand-in is copied aside (the renderer may
		// draw into it again before the frontend reads it).
		void SwapBuffers(bool swapTV, bool swapDRC) override
		{
			if (!m_tv.fbo)
				return;
			if (swapTV)
				Present(m_tv);
			if (swapDRC && chimera::ShowsGamePad())
				Present(m_pad);
			if (swapTV || swapDRC)
				s_pictureStale = true;
		}

		// the picture a window last presented, BGRA, GL's bottom row first
		bool ReadPresented(bool pad, uint32_t* out)
		{
			const Screen& screen = pad ? m_pad : m_tv;
			if (!screen.presentedFbo)
				return false;
			using namespace CemuGL;
			static auto readPixels = (PFN_ReadPixels)chimera_gl_lookup("glReadPixels");
			GLint prevRead = 0, prevPack = 0;
			glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
			glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &prevPack);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, screen.presentedFbo);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
			glPixelStorei(GL_PACK_ALIGNMENT, 4);
			readPixels(0, 0, screen.width, screen.height, GL_BGRA, GL_UNSIGNED_BYTE, out);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, prevRead);
			glBindBuffer(GL_PIXEL_PACK_BUFFER, prevPack);
			return true;
		}

	private:
		static void Present(const Screen& screen)
		{
			using namespace CemuGL;
			static auto blit = (PFN_BlitFramebuffer)chimera_gl_lookup("glBlitFramebuffer");
			GLint prevRead = 0, prevDraw = 0;
			glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);
			// a blit is clipped by the scissor and converted by sRGB writes:
			// the renderer leaves either on
			const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
			const GLboolean srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
			glDisable(GL_SCISSOR_TEST);
			glDisable(GL_FRAMEBUFFER_SRGB);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, screen.fbo);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, screen.presentedFbo);
			blit(0, 0, screen.width, screen.height, 0, 0, screen.width, screen.height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, prevRead);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDraw);
			if (scissor)
				glEnable(GL_SCISSOR_TEST);
			if (srgb)
				glEnable(GL_FRAMEBUFFER_SRGB);
		}

		// A texture and a framebuffer object of a window's size, black. Cemu
		// keeps its own record of what is bound, so whatever this binds is put
		// back.
		static GLuint MakeTarget(GLuint& texture, int width, int height)
		{
			using namespace CemuGL;
			static auto clearTexImage = (PFN_ClearTexImage)chimera_gl_lookup("glClearTexImage");
			GLint prevTexture = 0, prevRead = 0, prevDraw = 0, prevUnpack = 0;
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);
			glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevRead);
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevDraw);
			glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &prevUnpack);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			glGenTextures(1, &texture);
			glBindTexture(GL_TEXTURE_2D, texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			const uint8_t black[4] = {0, 0, 0, 255};
			clearTexImage(texture, 0, GL_RGBA, GL_UNSIGNED_BYTE, black);
			GLuint fbo = 0;
			glGenFramebuffers(1, &fbo);
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
			glBindTexture(GL_TEXTURE_2D, prevTexture);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, prevUnpack);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, prevRead);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, prevDraw);
			return fbo;
		}

		void MakeTargets() const
		{
			if (m_tv.fbo)
				return;
			for (Screen* screen : {&m_tv, &m_pad})
			{
				screen->fbo = MakeTarget(screen->color, screen->width, screen->height);
				screen->presentedFbo = MakeTarget(screen->presented, screen->width, screen->height);
			}
			m_context = chimera_gl_context_id();
		}

		mutable Screen m_tv{chimera::kTvWidth, chimera::kTvHeight};
		mutable Screen m_pad{chimera::kPadWidth, chimera::kPadHeight};
		mutable uint64_t m_context = 0;
		bool m_padCurrent = false;
	};

	Canvas* s_canvas = nullptr;
}

uint64_t chimera_gpu_context_id()
{
#if !defined(CHIMERA_GUEST)
	// For the tests only (the negative control of picture:remade): with
	// CHIMERA_TEST_NO_REMAKE the renderer is never told the context moved.
	static const bool noRemake = getenv("CHIMERA_TEST_NO_REMAKE") != nullptr;
	if (noRemake)
		return 1;
#endif
	return chimera_gl_context_id();
}

void chimera_gpu_context_forget()
{
	if (s_canvas)
		s_canvas->Forget();
	s_pictureStale = false; // the picture in memory is the last one shown
}

namespace
{
	// Does the host's driver have this extension? Asked of the context the
	// calls land on now (a state may come from a process on another card).
	bool HostHasExtension(const char* extension)
	{
		static uint64_t context = 0;
		static std::vector<std::string> extensions;
		if (context != chimera_gl_context_id())
		{
			context = chimera_gl_context_id();
			extensions.clear();
			using GetIntegerv = void (*)(GLenum, GLint*);
			using GetStringi = const GLubyte* (*)(GLenum, GLuint);
			auto getIntegerv = (GetIntegerv)chimera_gl_lookup("glGetIntegerv");
			auto getStringi = (GetStringi)chimera_gl_lookup("glGetStringi");
			GLint count = 0;
			if (getIntegerv && getStringi)
				getIntegerv(GL_NUM_EXTENSIONS, &count);
			for (GLint i = 0; i < count; i++)
				if (const GLubyte* e = getStringi(GL_EXTENSIONS, (GLuint)i))
					extensions.emplace_back((const char*)e);
		}
		return std::find(extensions.begin(), extensions.end(), extension) != extensions.end();
	}
}

// What Cemu's loader is given for a GL function. The bridge carries a wrapper
// for every name whether the host's driver has it or not; Cemu decides by
// the pointer whether to use the few functions of vendors' extensions (the
// NVIDIA depth range above all), so those are given only when the driver
// has the extension, as a driver's own loader would.
void* chimera_gl_proc_address(const char* name)
{
	static const std::pair<const char*, const char*> kExtensionFunctions[] = {
		{"glDepthRangedNV", "GL_NV_depth_buffer_float"},
		{"glMaxShaderCompilerThreadsARB", "GL_ARB_parallel_shader_compile"},
		{"glNamedFramebufferTexture2DEXT", "GL_EXT_direct_state_access"},
		{"glPolygonOffsetClampEXT", "GL_EXT_polygon_offset_clamp"},
	};
	for (const auto& [function, extension] : kExtensionFunctions)
		if (strcmp(name, function) == 0 && !HostHasExtension(extension))
			return nullptr;
	return chimera_gl_lookup(name);
}

bool chimera_fake_occlusion()
{
	return s_fakeOcclusion;
}

CPUMode chimera_cpu_mode()
{
	return s_cpuMode;
}

int64_t chimera_clock_start()
{
	return s_clockStart;
}

void chimera_log_write(std::string_view line)
{
	if (s_log)
	{
		fwrite(line.data(), 1, line.size(), stderr);
		fflush(stderr);
	}
}

namespace
{
	// What upstream's window does with a file it is given (MainWindow::
	// FileLoad): a title (a .wua archive, a .wuhb bundle) is added to the
	// title list and its base title prepared, with its update and its
	// add-on content when the archive holds them; a program on its own (.rpx,
	// .elf) is prepared as it is. Returns an empty string, or why not.
	std::string PrepareTitle(const fs::path& path, const std::string& name);

	// A NUS title, and its add-on content: each opened first, to say plainly
	// when the key does not open it (a wrong key reads as a damaged title)
	std::string PrepareNusTitle(const fs::path& tmd, const std::string& name, const fs::path& dlcTmd, const std::string& dlcName)
	{
		for (const auto& [path, what] : {std::pair{tmd, name}, std::pair{dlcTmd, dlcName}})
		{
			if (path.empty())
				continue;
			FSTVolume::ErrorCode fstError = FSTVolume::ErrorCode::OK;
			FSTVolume* volume = FSTVolume::OpenFromContentFolder(path.parent_path(), &fstError);
			if (!volume)
			{
				if (fstError == FSTVolume::ErrorCode::TITLE_TIK_MISSING)
					return "error.nus_no_ticket\x1f" + what;
				if (fstError == FSTVolume::ErrorCode::BAD_TITLE_TMD || fstError == FSTVolume::ErrorCode::BAD_TITLE_TIK)
					return "error.nus_damaged\x1f" + what;
				return "error.nus_cannot_decrypt\x1f" + what;
			}
			delete volume;
		}
		if (!dlcTmd.empty())
		{
			TitleInfo aoc{dlcTmd};
			if (!aoc.IsValid() || aoc.GetTitleType() != TitleIdParser::TITLE_TYPE::AOC)
				return "error.dlc_not_dlc\x1f" + dlcName;
			CafeTitleList::AddTitleFromPath(dlcTmd);
		}
		TitleInfo title{tmd};
		if (title.IsValid() && title.GetTitleType() == TitleIdParser::TITLE_TYPE::AOC)
			return "error.no_base_title\x1f" + name;
		return PrepareTitle(tmd, name);
	}

	std::string PrepareTitle(const fs::path& path, const std::string& name)
	{
		TitleInfo title{path};
		if (title.IsValid())
		{
			CafeTitleList::AddTitleFromPath(path);
			TitleId base;
			if (!CafeTitleList::FindBaseTitleId(title.GetAppTitleId(), base))
				return "error.no_base_title\x1f" + name;
			if (CafeSystem::PrepareForegroundTitle(base) != CafeSystem::PREPARE_STATUS_CODE::SUCCESS)
				return "error.cannot_load\x1f" + name;
			return {};
		}
		const CafeTitleFileType type = DetermineCafeSystemFileType(path);
		if (type == CafeTitleFileType::RPX || type == CafeTitleFileType::ELF)
		{
			if (CafeSystem::PrepareForegroundTitleFromStandaloneRPX(path) != CafeSystem::PREPARE_STATUS_CODE::SUCCESS)
				return "error.cannot_load\x1f" + name;
			return {};
		}
		return "error.not_a_title\x1f" + name;
	}
}

// the console's common key, for ncrypto.cpp (which has none of its own in
// this core): the one the project gives, or zeros and false
static std::vector<uint8_t> s_commonKey;
static bool s_stackReport;

bool chimera_common_key(uint8_t* out16)
{
	if (s_commonKey.size() != 16)
	{
		std::memset(out16, 0, 16);
		return false;
	}
	std::memcpy(out16, s_commonKey.data(), 16);
	return true;
}

namespace
{
	bool EndsWithZip(const std::string& name)
	{
		return name.size() > 4 && (name.compare(name.size() - 4, 4, ".zip") == 0 || name.compare(name.size() - 4, 4, ".ZIP") == 0 ||
			name.compare(name.size() - 4, 4, ".Zip") == 0);
	}
}

namespace
{
	// every file of an open volume under path, written under out
	bool WriteVolume(FSTVolume* v, const std::string& path, const std::string& out, int& files)
	{
		FSTDirectoryIterator it;
		if (!v->OpenDirectoryIterator(path, it))
			return false;
		fs::create_directories(_utf8ToPath(out));
		FSTFileHandle fh;
		while (v->Next(it, fh))
		{
			const std::string name(v->GetName(fh));
			const std::string inner = path + "/" + name;
			if (v->IsDirectory(fh))
			{
				if (!WriteVolume(v, inner, out + "/" + name, files))
					return false;
				continue;
			}
			bool ok = false;
			const std::vector<uint8> data = v->ExtractFile(inner, &ok);
			if (!ok)
				return false;
			FILE* f = fopen((out + "/" + name).c_str(), "wb");
			if (!f)
				return false;
			fwrite(data.data(), 1, data.size(), f);
			fclose(f);
			files++;
		}
		return true;
	}
}

namespace chimera
{
	std::string ExtractNus(const std::string& zip, const std::vector<uint8_t>& key, const std::string& out)
	{
		s_commonKey = key;
		AES128_init();
		const std::string folder = std::string(memfs::ROOT) + "/extract";
		std::string tmd, error;
		if (!MountNusZip(zip, zip, folder, tmd, error))
			return error;
		FSTVolume::ErrorCode fstError = FSTVolume::ErrorCode::OK;
		FSTVolume* v = FSTVolume::OpenFromContentFolder(_utf8ToPath(folder), &fstError);
		if (!v)
			return "the title could not be opened (" + std::to_string((int)fstError) + ")";
		int files = 0;
		const bool written = WriteVolume(v, "", out, files);
		delete v;
		if (!written)
			return "a file could not be read or written";
		fprintf(stderr, "extracted %d files to %s\n", files, out.c_str());
		return {};
	}

	std::string StoragePath()
	{
		return std::string(memfs::ROOT) + "/user/mlc01";
	}

	// The folders and files upstream Cemu's window makes in a new console
	// storage (CemuApp::CreateDefaultMLCFiles), which this core has no window
	// to make: the title folders, the Mii Maker save folders (a game's Mii
	// library writes its database there, FFL_HDB.dat, and without the folder
	// it does not start) and the system's language and country lists.
	static void MakeStorageDefaults(const std::string& mlc)
	{
		for (const char* dir : { "sys", "usr", "usr/title/00050000", "usr/title/0005000c", "usr/title/0005000e",
			"usr/save/00050010/1004a000/user/common/db", "usr/save/00050010/1004a100/user/common/db",
			"usr/save/00050010/1004a200/user/common/db", "sys/title/0005001b/1005c000/content" })
			memfs::mkdirs(mlc + "/" + dir);
		const std::string lang = mlc + "/sys/title/0005001b/1005c000/content";
		std::string text;
		for (const char* code : { "ja", "en", "fr", "de", "it", "es", "zh", "ko", "nl", "pt", "ru", "zh" })
			text += std::string("\"") + code + "\",\n";
		memfs::put(lang + "/language.txt", std::vector<uint8_t>(text.begin(), text.end()));
		text.clear();
		for (sint32 i = 0; i < (sint32)NCrypto::GetCountryCount(); i++)
		{
			const std::string code = NCrypto::GetCountryAsString(i);
			text += (code == "NN" || code == "nn") ? std::string("NULL,\n") : "\"" + code + "\",\n";
		}
		memfs::put(lang + "/country.txt", std::vector<uint8_t>(text.begin(), text.end()));
	}

	std::string Boot(const BootOptions& options)
	{
		s_cpuMode = options.recompiler ? CPUMode::SinglecoreRecompiler : CPUMode::SinglecoreInterpreter;
		s_clockStart = options.clockStart;
		s_log = options.log;
		s_fakeOcclusion = options.fakeOcclusion;
		s_stackReport = options.stackReport;
		if (options.log && options.logTypes)
			cemuLog_setActiveLoggingFlags(options.logTypes);
		vsched::RegisterTaskLocal(&ppcInterpreterCurrentInstance, sizeof(ppcInterpreterCurrentInstance));
		vsched::RegisterTaskLocal(&s_schedulerLockCount, sizeof(s_schedulerLockCount));
		vsched::RegisterTaskLocal(&sCurrentFiber, sizeof(sCurrentFiber));
		vsched::RegisterTaskLocal(&coreinit::t_assignedCoreIndex, sizeof(coreinit::t_assignedCoreIndex));
		vsched::RegisterTaskLocal(&coreinit::t_schedulerFiber, sizeof(coreinit::t_schedulerFiber));

		// the machine's filesystem: Cemu's own folder, the console's storage
		// (mlc01, Cemu's default place for it) and the program, grafted in
		// read-only from where the frontend put it
		memfs::set_clock(CalendarSeconds);
		const std::string user = std::string(memfs::ROOT) + "/user";
		memfs::mkdirs(user + "/mlc01");
		MakeStorageDefaults(StoragePath());
		// the saved games the project brings, before the console starts, so
		// that they are in the machine every state is taken against
		if (!options.savedata.empty())
		{
			std::string saveError;
			if (!LoadSaveData(options.savedata, options.savedataName, StoragePath(), saveError))
				return saveError;
		}
		// the console's Mii resource, where a console keeps it: the content
		// of its system title 0005001b-10056000, which games that draw Miis
		// read (SAVEGetSharedDataTitlePath)
		if (!options.miiResource.empty())
		{
			const std::string content = StoragePath() + "/sys/title/0005001b/10056000/content";
			memfs::mkdirs(content);
			for (const auto& [name, mounted] : options.miiResource)
				if (!memfs::graft(content + "/" + name, mounted))
					return "error.cannot_open\x1f" + name;
		}
		// Cemu's per-game profiles, in its data folder, where it reads the
		// one of the title it runs (gameProfiles/default/<title id>.ini)
		WriteGameProfiles(user + "/gameProfiles/default");
		memfs::mkdirs(std::string(memfs::ROOT) + "/game");
		const std::string base = options.game.substr(options.game.find_last_of('/') + 1);
		std::string game = std::string(memfs::ROOT) + "/game/" + base;
		// a NUS game: its folder, from the zip, and its add-on content's; the
		// files are encrypted, and the common key opens them
		std::string dlcTmd;
		const bool nus = EndsWithZip(base);
		if (nus || !options.dlc.empty())
		{
			if (!nus)
				return "error.dlc_without_nus\x1f" + options.dlcName;
			if (options.commonKey.size() != 16)
				return "error.nus_no_key\x1f" + base;
			s_commonKey = options.commonKey;
			std::string nusError;
			if (!MountNusZip(options.game, base, std::string(memfs::ROOT) + "/game/title", game, nusError))
				return nusError;
			if (!options.dlc.empty() && !MountNusZip(options.dlc, options.dlcName, std::string(memfs::ROOT) + "/game/aoc", dlcTmd, nusError))
				return nusError;
		}
		else if (!memfs::graft(game, options.game))
			return "error.cannot_open\x1f" + base;
		// the console's shared fonts, where Cemu looks for its own copies
		// (resources/sharedFonts in its data folder); with none, every game
		// that asks is given Cemu's stand-in
		if (!options.fonts.empty())
		{
			const std::string fonts = user + "/resources/sharedFonts";
			memfs::mkdirs(fonts);
			for (const std::string& font : options.fonts)
			{
				const std::string name = font.substr(font.find_last_of('/') + 1);
				if (!memfs::graft(fonts + "/" + name, font))
					return "error.cannot_open\x1f" + name;
			}
		}

		std::set<fs::path> failedWriteAccess;
		const fs::path userPath = _utf8ToPath(user);
		ActiveSettings::SetPaths(true, userPath / "cemu", userPath, userPath, userPath / "cache", userPath, failedWriteAccess);
		GetConfigHandle().SetFilename(ActiveSettings::GetConfigPath("settings.xml").generic_wstring());
		ActiveSettings::Init();
		// the console's language (System Settings), which games ask for
		GetConfig().console_language.SetValue((CafeConsoleLanguage)options.consoleLanguage);
		WindowSystem::Create();
		AES128_init();
		PPCTimer_init();
		s_canvas = new Canvas();
		SetOpenGLCanvasCallbacks(s_canvas);
		s_picture = new uint32_t[kPictureMaxWidth * kPictureMaxHeight];
		for (int i = 0; i < kPictureMaxWidth * kPictureMaxHeight; i++)
			s_picture[i] = 0xFF000000u; // black where no screen is

		// the console is started inside a task: everything it starts waits
		// and runs on the machine's clock
		std::string error;
		bool done = false;
		vsched::Spawn("boot", [&]() {
			CafeSystem::Initialize();
			g_renderer = std::make_unique<OpenGLRenderer>();
			if (nus)
				error = PrepareNusTitle(_utf8ToPath(game), base, dlcTmd.empty() ? fs::path() : _utf8ToPath(dlcTmd), options.dlcName);
			else
				error = PrepareTitle(_utf8ToPath(game), base);
			if (error.empty())
				CafeSystem::LaunchForegroundTitle();
			done = true;
		});
		vsched::RunUntil(vsched::kNever, [&]() { return done; });
		return error;
	}

	void RunFrame()
	{
		s_frame++;
		vsched::RunUntil(LatteTiming_chimeraVsyncTime(s_frame) + kFrameEndAfterVsync);
		if (s_stackReport && s_frame % 600 == 0)
			StackReport(s_frame);
	}

	uint64_t FrameCount()
	{
		return s_frame;
	}

	uint64_t ClockCycles()
	{
		return vsched::Now();
	}

	uint64_t TaskSwitches()
	{
		return vsched::Switches();
	}

	void SetRendering(bool on)
	{
		s_rendering = on;
	}

	// The picture: the TV's, the GamePad's, or the TV's above the GamePad's
	// (which is centred under it), as the setting says.
	const uint32_t* Picture()
	{
		if (!s_picture)
			return nullptr;
		if (s_rendering && s_pictureStale && s_canvas && s_canvas->Current())
		{
			static uint32_t* rows = new uint32_t[kTvWidth * kTvHeight];
			int w, h, padX, padY;
			PictureSize(w, h);
			GamePadArea(padX, padY);
			bool read = true;
			if (GetScreens() != Screens::GamePad)
			{
				if ((read = s_canvas->ReadPresented(false, rows)))
					for (int y = 0; y < kTvHeight; y++) // GL's rows are bottom first
						memcpy(&s_picture[y * w], &rows[(kTvHeight - 1 - y) * kTvWidth], kTvWidth * 4);
			}
			if (read && ShowsGamePad())
			{
				if ((read = s_canvas->ReadPresented(true, rows)))
					for (int y = 0; y < kPadHeight; y++)
						memcpy(&s_picture[(padY + y) * w + padX], &rows[(kPadHeight - 1 - y) * kPadWidth], kPadWidth * 4);
			}
			if (read)
				s_pictureStale = false;
		}
		return s_picture;
	}

	int PictureWidth()
	{
		int w, h;
		PictureSize(w, h);
		return w;
	}

	int PictureHeight()
	{
		int w, h;
		PictureSize(w, h);
		return h;
	}

	const uint8_t* FindMailbox()
	{
		static const char magic[16] = "CHIMERA-MAILBOX";
		for (MMURange* range : memory_getMMURanges())
		{
			if (!range->isMapped())
				continue;
			const uint8_t* p = memory_getPointerFromVirtualOffset(range->getBase());
			const size_t size = range->getEnd() - range->getBase();
			for (size_t off = 0; off + 64 <= size; off += 64)
			{
				if (memcmp(p + off, magic, 16) == 0)
					return p + off;
			}
		}
		return nullptr;
	}

	uint64_t MemoryDigest()
	{
		// a word at a time: the console's memory is a gigabyte and more
		uint64_t h = 0xcbf29ce484222325ull;
		for (MMURange* range : memory_getMMURanges())
		{
			if (!range->isMapped())
				continue;
			const uint64_t* p = (const uint64_t*)memory_getPointerFromVirtualOffset(range->getBase());
			const size_t words = (range->getEnd() - range->getBase()) / 8;
			for (size_t i = 0; i < words; i++)
			{
				h ^= p[i];
				h *= 0x100000001b3ull;
				h ^= h >> 29;
			}
		}
		return h;
	}

	const uint64_t* MemoryRanges()
	{
		static std::vector<uint64_t> out;
		out.clear();
		for (MMURange* range : memory_getMMURanges())
		{
			if (!range->isMapped())
				continue;
			out.push_back(range->getBase());
			out.push_back(range->getEnd() - range->getBase());
			out.push_back((uint64_t)(uintptr_t)memory_getPointerFromVirtualOffset(range->getBase()));
		}
		out.push_back(0);
		out.push_back(0);
		out.push_back(0);
		return out.data();
	}

	// ---- the console's memory, for the frontend's tools ------------------

	namespace
	{
		struct DomainDef
		{
			const char* name;
			uint32 base, size;
		};
		// MEM2, the main memory (1 GiB), and MEM1, the fast 32 MiB beside the
		// GPU: where a game keeps what it keeps
		constexpr DomainDef kDomains[] = {
			{"MEM2", 0x10000000, 0x40000000},
			{"MEM1", 0xF4000000, 0x02000000},
		};

		// the mapped range an address of the console's is in, or null
		MMURange* RangeOf(uint64_t address)
		{
			for (MMURange* range : memory_getMMURanges())
				if (range->isMapped() && address >= range->getBase() && address < range->getEnd())
					return range;
			return nullptr;
		}
	}

	int DomainCount()
	{
		return (int)(sizeof kDomains / sizeof kDomains[0]);
	}

	const char* DomainName(int i)
	{
		return i >= 0 && i < DomainCount() ? kDomains[i].name : "";
	}

	uint8_t* DomainPointer(int i)
	{
		if (i < 0 || i >= DomainCount() || !RangeOf(kDomains[i].base))
			return nullptr;
		return memory_getPointerFromVirtualOffset(kDomains[i].base);
	}

	int64_t DomainSize(int i)
	{
		return i >= 0 && i < DomainCount() && RangeOf(kDomains[i].base) ? kDomains[i].size : 0;
	}

	// the whole address space: what is mapped reads as it is, the rest as 0
	uint8_t BusPeek(uint64_t address)
	{
		if (address >> 32 || !RangeOf(address))
			return 0;
		return *memory_getPointerFromVirtualOffset((uint32)address);
	}

	void BusPoke(uint64_t address, uint8_t value)
	{
		if (address >> 32 || !RangeOf(address))
			return;
		*memory_getPointerFromVirtualOffset((uint32)address) = value;
	}

	void BusRead(uint64_t address, uint8_t* out, size_t len)
	{
		size_t done = 0;
		while (done < len)
		{
			const uint64_t at = address + done;
			MMURange* range = at >> 32 ? nullptr : RangeOf(at);
			if (!range)
			{
				out[done++] = 0;
				continue;
			}
			const size_t n = std::min<size_t>(len - done, range->getEnd() - at);
			memcpy(out + done, memory_getPointerFromVirtualOffset((uint32)at), n);
			done += n;
		}
	}

	// the mapped ranges: (address, length) pairs, ended by a length of 0
	void BusRanges(std::vector<int64_t>& out)
	{
		out.clear();
		for (MMURange* range : memory_getMMURanges())
		{
			if (!range->isMapped())
				continue;
			out.push_back(range->getBase());
			out.push_back((int64_t)range->getEnd() - range->getBase());
		}
		out.push_back(0);
		out.push_back(0);
	}
}
