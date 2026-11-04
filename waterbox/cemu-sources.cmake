# Written by waterbox/gen-sources.py from upstream's CMakeLists.txt files.
# Paths are under extern/cemu/src (dependencies/ is one level up).

set(CEMU_COMMON_SOURCES
  Common/CafeStaticVar.cpp
  Common/cpu_features.cpp
  Common/precompiled.cpp
  Common/SysAllocator.cpp
  Common/unix/platform.cpp
  Common/unix/FileStream_unix.cpp
)
# left out: Common/ExceptionHandler/ExceptionHandler.cpp - the crash handler (a sandbox has no signals)
# left out: Common/windows/platform.cpp - Windows
# left out: Common/windows/FileStream_win32.cpp - Windows
# left out: Common/ExceptionHandler/ExceptionHandler_win32.cpp - Windows
# left out: Common/ExceptionHandler/ExceptionHandler_posix.cpp - the crash handler (a sandbox has no signals)
# left out: Common/ExceptionHandler/ELFSymbolTable.cpp - the crash handler (a sandbox has no signals)

set(CEMU_CAFE_SOURCES
  Cafe/Account/Account.cpp
  Cafe/CafeSystem.cpp
  Cafe/Filesystem/fsc.cpp
  Cafe/Filesystem/fscDeviceHostFS.cpp
  Cafe/Filesystem/fscDeviceRedirect.cpp
  Cafe/Filesystem/fscDeviceWua.cpp
  Cafe/Filesystem/fscDeviceWud.cpp
  Cafe/Filesystem/fscDeviceWuhb.cpp
  Cafe/Filesystem/FST/FST.cpp
  Cafe/Filesystem/FST/KeyCache.cpp
  Cafe/Filesystem/WUD/wud.cpp
  Cafe/Filesystem/WUHB/WUHBReader.cpp
  Cafe/GamePatch.cpp
  Cafe/GameProfile/GameProfile.cpp
  Cafe/GraphicPack/GraphicPack2.cpp
  Cafe/GraphicPack/GraphicPack2PatchesApply.cpp
  Cafe/GraphicPack/GraphicPack2Patches.cpp
  Cafe/GraphicPack/GraphicPack2PatchesParser.cpp
  Cafe/HW/ACR/ACR.cpp
  Cafe/HW/AI/AI.cpp
  Cafe/HW/Espresso/Debugger/Debugger.cpp
  Cafe/HW/Espresso/Debugger/DebugSymbolStorage.cpp
  Cafe/HW/Espresso/Debugger/GDBStub.cpp
  Cafe/HW/Espresso/Debugger/GDBBreakpoints.cpp
  Cafe/HW/Espresso/Interpreter/PPCInterpreterFPU.cpp
  Cafe/HW/Espresso/Interpreter/PPCInterpreterHLE.cpp
  Cafe/HW/Espresso/Interpreter/PPCInterpreterImpl.cpp
  Cafe/HW/Espresso/Interpreter/PPCInterpreterMain.cpp
  Cafe/HW/Espresso/Interpreter/PPCInterpreterOPC.cpp
  Cafe/HW/Espresso/Interpreter/PPCInterpreterPS.cpp
  Cafe/HW/Espresso/PPCScheduler.cpp
  Cafe/HW/Espresso/PPCSchedulerLLE.cpp
  Cafe/HW/Espresso/PPCTimer.cpp
  Cafe/HW/Espresso/Recompiler/PPCRecompiler.cpp
  Cafe/HW/Espresso/Recompiler/IML/IMLSegment.cpp
  Cafe/HW/Espresso/Recompiler/IML/IMLInstruction.cpp
  Cafe/HW/Espresso/Recompiler/IML/IMLDebug.cpp
  Cafe/HW/Espresso/Recompiler/IML/IMLAnalyzer.cpp
  Cafe/HW/Espresso/Recompiler/IML/IMLOptimizer.cpp
  Cafe/HW/Espresso/Recompiler/IML/IMLRegisterAllocator.cpp
  Cafe/HW/Espresso/Recompiler/IML/IMLRegisterAllocatorRanges.cpp
  Cafe/HW/Espresso/Recompiler/PPCRecompilerImlGen.cpp
  Cafe/HW/Espresso/Recompiler/PPCRecompilerImlGenFPU.cpp
  Cafe/HW/Espresso/Recompiler/PPCRecompilerIntermediate.cpp
  Cafe/HW/Espresso/Recompiler/BackendX64/BackendX64AVX.cpp
  Cafe/HW/Espresso/Recompiler/BackendX64/BackendX64BMI.cpp
  Cafe/HW/Espresso/Recompiler/BackendX64/BackendX64.cpp
  Cafe/HW/Espresso/Recompiler/BackendX64/BackendX64FPU.cpp
  Cafe/HW/Espresso/Recompiler/BackendX64/BackendX64Gen.cpp
  Cafe/HW/Espresso/Recompiler/BackendX64/BackendX64GenFPU.cpp
  Cafe/HW/Latte/Common/RegisterSerializer.cpp
  Cafe/HW/Latte/Common/ShaderSerializer.cpp
  Cafe/HW/Latte/Core/FetchShader.cpp
  Cafe/HW/Latte/Core/LatteAsyncCommands.cpp
  Cafe/HW/Latte/Core/LatteBufferCache.cpp
  Cafe/HW/Latte/Core/LatteBufferData.cpp
  Cafe/HW/Latte/Core/LatteCommandProcessor.cpp
  Cafe/HW/Latte/Core/LatteGSCopyShaderParser.cpp
  Cafe/HW/Latte/Core/LatteIndices.cpp
  Cafe/HW/Latte/Core/LatteOverlay.cpp
  Cafe/HW/Latte/Core/LattePerformanceMonitor.cpp
  Cafe/HW/Latte/Core/LatteQuery.cpp
  Cafe/HW/Latte/Core/LatteRenderTarget.cpp
  Cafe/HW/Latte/Core/LatteRingBuffer.cpp
  Cafe/HW/Latte/Core/LatteShaderCache.cpp
  Cafe/HW/Latte/Core/LatteShader.cpp
  Cafe/HW/Latte/Core/LatteSoftware.cpp
  Cafe/HW/Latte/Core/LatteStreamoutGPU.cpp
  Cafe/HW/Latte/Core/LatteSurfaceCopy.cpp
  Cafe/HW/Latte/Core/LatteTextureCache.cpp
  Cafe/HW/Latte/Core/LatteTexture.cpp
  Cafe/HW/Latte/Core/LatteTextureLegacy.cpp
  Cafe/HW/Latte/Core/LatteTextureLoader.cpp
  Cafe/HW/Latte/Core/LatteTextureReadback.cpp
  Cafe/HW/Latte/Core/LatteTextureView.cpp
  Cafe/HW/Latte/Core/LatteThread.cpp
  Cafe/HW/Latte/Core/LatteTiming.cpp
  Cafe/HW/Latte/LatteAddrLib/LatteAddrLib_Coord.cpp
  Cafe/HW/Latte/LatteAddrLib/LatteAddrLib.cpp
  Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerAnalyzer.cpp
  Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompiler.cpp
  Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerEmitGLSLAttrDecoder.cpp
  Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerEmitGLSL.cpp
  Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerRegisterDataTypeTracker.cpp
  Cafe/HW/Latte/Renderer/Renderer.cpp
  Cafe/HW/Latte/Renderer/RendererCore.cpp
  Cafe/HW/Latte/Renderer/RendererOuputShader.cpp
  Cafe/HW/Latte/Renderer/RendererShader.cpp
  Cafe/HW/Latte/ShaderInfo/ShaderDescription.cpp
  Cafe/HW/Latte/ShaderInfo/ShaderInstanceInfo.cpp
  Cafe/HW/Latte/Transcompiler/LatteTC.cpp
  Cafe/HW/Latte/Transcompiler/LatteTCGenIR.cpp
  Cafe/HW/MMU/MMU.cpp
  Cafe/HW/SI/SI.cpp
  Cafe/HW/VI/VI.cpp
  Cafe/IOSU/ccr_nfc/iosu_ccr_nfc.cpp
  Cafe/IOSU/fsa/iosu_fsa.cpp
  Cafe/IOSU/kernel/iosu_kernel.cpp
  Cafe/IOSU/legacy/iosu_acp.cpp
  Cafe/IOSU/legacy/iosu_act.cpp
  Cafe/IOSU/legacy/iosu_crypto.cpp
  Cafe/IOSU/legacy/iosu_fpd.cpp
  Cafe/IOSU/legacy/iosu_ioctl.cpp
  Cafe/IOSU/legacy/iosu_mcp.cpp
  Cafe/IOSU/legacy/iosu_nim.cpp
  Cafe/IOSU/nn/iosu_nn_service.cpp
  Cafe/IOSU/nn/boss/boss_common.cpp
  Cafe/IOSU/nn/boss/boss_service.cpp
  Cafe/IOSU/PDM/iosu_pdm.cpp
  Cafe/IOSU/ODM/iosu_odm.cpp
  Cafe/OS/common/OSCommon.cpp
  Cafe/OS/libs/avm/avm.cpp
  Cafe/OS/libs/camera/camera.cpp
  Cafe/OS/libs/coreinit/coreinit_Alarm.cpp
  Cafe/OS/libs/coreinit/coreinit_Atomic.cpp
  Cafe/OS/libs/coreinit/coreinit_BSP.cpp
  Cafe/OS/libs/coreinit/coreinit_Callbacks.cpp
  Cafe/OS/libs/coreinit/coreinit_CodeGen.cpp
  Cafe/OS/libs/coreinit/coreinit_Coroutine.cpp
  Cafe/OS/libs/coreinit/coreinit.cpp
  Cafe/OS/libs/coreinit/coreinit_DynLoad.cpp
  Cafe/OS/libs/coreinit/coreinit_FG.cpp
  Cafe/OS/libs/coreinit/coreinit_FS.cpp
  Cafe/OS/libs/coreinit/coreinit_GHS.cpp
  Cafe/OS/libs/coreinit/coreinit_HWInterface.cpp
  Cafe/OS/libs/coreinit/coreinit_IM.cpp
  Cafe/OS/libs/coreinit/coreinit_Init.cpp
  Cafe/OS/libs/coreinit/coreinit_IOS.cpp
  Cafe/OS/libs/coreinit/coreinit_IPCBuf.cpp
  Cafe/OS/libs/coreinit/coreinit_IPC.cpp
  Cafe/OS/libs/coreinit/coreinit_LockedCache.cpp
  Cafe/OS/libs/coreinit/coreinit_MCP.cpp
  Cafe/OS/libs/coreinit/coreinit_MEM_BlockHeap.cpp
  Cafe/OS/libs/coreinit/coreinit_MEM.cpp
  Cafe/OS/libs/coreinit/coreinit_MEM_ExpHeap.cpp
  Cafe/OS/libs/coreinit/coreinit_MEM_FrmHeap.cpp
  Cafe/OS/libs/coreinit/coreinit_Memory.cpp
  Cafe/OS/libs/coreinit/coreinit_MemoryMapping.cpp
  Cafe/OS/libs/coreinit/coreinit_MEM_UnitHeap.cpp
  Cafe/OS/libs/coreinit/coreinit_MessageQueue.cpp
  Cafe/OS/libs/coreinit/coreinit_Misc.cpp
  Cafe/OS/libs/coreinit/coreinit_MPQueue.cpp
  Cafe/OS/libs/coreinit/coreinit_OSScreen.cpp
  Cafe/OS/libs/coreinit/coreinit_OverlayArena.cpp
  Cafe/OS/libs/coreinit/coreinit_Scheduler.cpp
  Cafe/OS/libs/coreinit/coreinit_Spinlock.cpp
  Cafe/OS/libs/coreinit/coreinit_Synchronization.cpp
  Cafe/OS/libs/coreinit/coreinit_SysHeap.cpp
  Cafe/OS/libs/coreinit/coreinit_SystemInfo.cpp
  Cafe/OS/libs/coreinit/coreinit_Thread.cpp
  Cafe/OS/libs/coreinit/coreinit_ThreadQueue.cpp
  Cafe/OS/libs/coreinit/coreinit_Time.cpp
  Cafe/OS/libs/dmae/dmae.cpp
  Cafe/OS/libs/drmapp/drmapp.cpp
  Cafe/OS/libs/erreula/erreula.cpp
  Cafe/OS/libs/gx2/GX2_AddrTest.cpp
  Cafe/OS/libs/gx2/GX2_Blit.cpp
  Cafe/OS/libs/gx2/GX2_Command.cpp
  Cafe/OS/libs/gx2/GX2_ContextState.cpp
  Cafe/OS/libs/gx2/GX2.cpp
  Cafe/OS/libs/gx2/GX2_Draw.cpp
  Cafe/OS/libs/gx2/GX2_Event.cpp
  Cafe/OS/libs/gx2/GX2_Memory.cpp
  Cafe/OS/libs/gx2/GX2_Misc.cpp
  Cafe/OS/libs/gx2/GX2_Query.cpp
  Cafe/OS/libs/gx2/GX2_RenderTarget.cpp
  Cafe/OS/libs/gx2/GX2_Resource.cpp
  Cafe/OS/libs/gx2/GX2_Shader.cpp
  Cafe/OS/libs/gx2/GX2_shader_legacy.cpp
  Cafe/OS/libs/gx2/GX2_State.cpp
  Cafe/OS/libs/gx2/GX2_Streamout.cpp
  Cafe/OS/libs/gx2/GX2_Surface_Copy.cpp
  Cafe/OS/libs/gx2/GX2_Surface.cpp
  Cafe/OS/libs/gx2/GX2_Texture.cpp
  Cafe/OS/libs/gx2/GX2_TilingAperture.cpp
  Cafe/OS/libs/h264_avc/H264Dec.cpp
  Cafe/OS/libs/h264_avc/H264DecBackendAVC.cpp
  Cafe/OS/libs/h264_avc/parser/H264Parser.cpp
  Cafe/OS/libs/mic/mic.cpp
  Cafe/OS/libs/nfc/ndef.cpp
  Cafe/OS/libs/nfc/nfc.cpp
  Cafe/OS/libs/nfc/stream.cpp
  Cafe/OS/libs/nfc/TagV0.cpp
  Cafe/OS/libs/nfc/TLV.cpp
  Cafe/OS/libs/nlibcurl/nlibcurl.cpp
  Cafe/OS/libs/nlibnss/nlibnss.cpp
  Cafe/OS/libs/nn_ac/nn_ac.cpp
  Cafe/OS/libs/nn_acp/nn_acp.cpp
  Cafe/OS/libs/nn_act/nn_act.cpp
  Cafe/OS/libs/nn_aoc/nn_aoc.cpp
  Cafe/OS/libs/nn_boss/nn_boss.cpp
  Cafe/OS/libs/nn_ccr/nn_ccr.cpp
  Cafe/OS/libs/nn_cmpt/nn_cmpt.cpp
  Cafe/OS/libs/nn_ec/nn_ec.cpp
  Cafe/OS/libs/nn_fp/nn_fp.cpp
  Cafe/OS/libs/nn_idbe/nn_idbe.cpp
  Cafe/OS/libs/nn_ndm/nn_ndm.cpp
  Cafe/OS/libs/nn_spm/nn_spm.cpp
  Cafe/OS/libs/nn_sl/nn_sl.cpp
  Cafe/OS/libs/nn_nfp/nn_nfp.cpp
  Cafe/OS/libs/nn_nim/nn_nim.cpp
  Cafe/OS/libs/nn_olv/nn_olv.cpp
  Cafe/OS/libs/nn_olv/nn_olv_Common.cpp
  Cafe/OS/libs/nn_olv/nn_olv_InitializeTypes.cpp
  Cafe/OS/libs/nn_olv/nn_olv_DownloadCommunityTypes.cpp
  Cafe/OS/libs/nn_olv/nn_olv_UploadCommunityTypes.cpp
  Cafe/OS/libs/nn_olv/nn_olv_UploadFavoriteTypes.cpp
  Cafe/OS/libs/nn_olv/nn_olv_PostTypes.cpp
  Cafe/OS/libs/nn_olv/nn_olv_OfflineDB.cpp
  Cafe/OS/libs/nn_pdm/nn_pdm.cpp
  Cafe/OS/libs/nn_save/nn_save.cpp
  Cafe/OS/libs/nn_temp/nn_temp.cpp
  Cafe/OS/libs/nn_uds/nn_uds.cpp
  Cafe/OS/libs/nsyshid/nsyshid.cpp
  Cafe/OS/libs/nsyshid/AttachDefaultBackends.cpp
  Cafe/OS/libs/nsyshid/Whitelist.cpp
  Cafe/OS/libs/nsyshid/BackendEmulated.cpp
  Cafe/OS/libs/nsyshid/Dimensions.cpp
  Cafe/OS/libs/nsyshid/Infinity.cpp
  Cafe/OS/libs/nsyshid/Skylander.cpp
  Cafe/OS/libs/nsyshid/SkylanderXbox360.cpp
  Cafe/OS/libs/nsyshid/g721/g721.cpp
  Cafe/OS/libs/nsyskbd/nsyskbd.cpp
  Cafe/OS/libs/nsysnet/nsysnet.cpp
  Cafe/OS/libs/ntag/ntag.cpp
  Cafe/OS/libs/padscore/padscore.cpp
  Cafe/OS/libs/proc_ui/proc_ui.cpp
  Cafe/OS/libs/snd_core/ax_aux.cpp
  Cafe/OS/libs/snd_core/ax_exports.cpp
  Cafe/OS/libs/snd_core/ax_ist.cpp
  Cafe/OS/libs/snd_core/ax_mix.cpp
  Cafe/OS/libs/snd_core/ax_multivoice.cpp
  Cafe/OS/libs/snd_core/ax_out.cpp
  Cafe/OS/libs/snd_core/ax_voice.cpp
  Cafe/OS/libs/snd_user/snd_user.cpp
  Cafe/OS/libs/swkbd/swkbd.cpp
  Cafe/OS/libs/sysapp/sysapp.cpp
  Cafe/OS/libs/TCL/TCL.cpp
  Cafe/OS/libs/vpad/vpad.cpp
  Cafe/OS/libs/zlib125/zlib125.cpp
  Cafe/OS/RPL/COSModule.cpp
  Cafe/OS/RPL/elf.cpp
  Cafe/OS/RPL/rpl.cpp
  Cafe/OS/RPL/rpl_debug_symbols.cpp
  Cafe/OS/RPL/rpl_symbol_storage.cpp
  Cafe/TitleList/GameInfo.cpp
  Cafe/TitleList/SaveInfo.cpp
  Cafe/TitleList/SaveList.cpp
  Cafe/TitleList/TitleInfo.cpp
  Cafe/TitleList/TitleList.cpp
  Cafe/HW/Latte/Renderer/OpenGL/LatteShaderGL.cpp
  Cafe/HW/Latte/Renderer/OpenGL/LatteTextureGL.cpp
  Cafe/HW/Latte/Renderer/OpenGL/LatteTextureViewGL.cpp
  Cafe/HW/Latte/Renderer/OpenGL/OpenGLQuery.cpp
  Cafe/HW/Latte/Renderer/OpenGL/OpenGLRendererCore.cpp
  Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.cpp
  Cafe/HW/Latte/Renderer/OpenGL/OpenGLRendererStreamout.cpp
  Cafe/HW/Latte/Renderer/OpenGL/OpenGLRendererUniformData.cpp
  Cafe/HW/Latte/Renderer/OpenGL/OpenGLSurfaceCopy.cpp
  Cafe/HW/Latte/Renderer/OpenGL/RendererShaderGL.cpp
  Cafe/HW/Latte/Renderer/OpenGL/TextureReadbackGL.cpp
)
# left out: Cafe/HW/Latte/Renderer/Vulkan/CachedFBOVk.cpp - the Vulkan renderer (not built yet)
# left out: Cafe/HW/Latte/Renderer/Vulkan/LatteTextureViewVk.cpp - the Vulkan renderer (not built yet)
# left out: Cafe/HW/Latte/Renderer/Vulkan/LatteTextureVk.cpp - the Vulkan renderer (not built yet)
# left out: Cafe/HW/Latte/Renderer/Vulkan/RendererShaderVk.cpp - the Vulkan renderer (not built yet)
# left out: Cafe/HW/Latte/Renderer/Vulkan/SwapchainInfoVk.cpp - the Vulkan renderer (not built yet)
# left out: Cafe/HW/Latte/Renderer/Vulkan/TextureReadbackVk.cpp - the Vulkan renderer (not built yet)
# left out: Cafe/HW/Latte/Renderer/Vulkan/VKRMemoryManager.cpp - the Vulkan renderer (not built yet)
# left out: Cafe/HW/Latte/Renderer/Vulkan/VKRPipelineInfo.cpp - the Vulkan renderer (not built yet)
# left out: Cafe/HW/Latte/Renderer/Vulkan/VsyncDriver.cpp - the Vulkan renderer (not built yet)
# left out: Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.cpp - the Vulkan renderer (not built yet)
# left out: Cafe/HW/Latte/Renderer/Vulkan/VulkanPipelineCompiler.cpp - the Vulkan renderer (not built yet)
# left out: Cafe/HW/Latte/Renderer/Vulkan/VulkanPipelineStableCache.cpp - the Vulkan renderer (not built yet)
# left out: Cafe/HW/Latte/Renderer/Vulkan/VulkanQuery.cpp - the Vulkan renderer (not built yet)
# left out: Cafe/HW/Latte/Renderer/Vulkan/VulkanRendererCore.cpp - the Vulkan renderer (not built yet)
# left out: Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.cpp - the Vulkan renderer (not built yet)
# left out: Cafe/HW/Latte/Renderer/Vulkan/VulkanSurfaceCopy.cpp - the Vulkan renderer (not built yet)
# left out: Cafe/HW/Latte/Renderer/Metal/CachedFBOMtl.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/LatteTextureMtl.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/LatteTextureReadbackMtl.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/LatteTextureViewMtl.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/LatteToMtl.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/MetalAttachmentsInfo.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/MetalBufferAllocator.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/MetalCppImpl.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/MetalDepthStencilCache.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/MetalLayerHandle.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/MetalMemoryManager.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/MetalOutputShaderCache.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/MetalPipelineCache.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/MetalPipelineCompiler.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/MetalQuery.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/MetalRenderer.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/MetalSamplerCache.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/MetalVoidVertexPipeline.cpp - macOS and Metal
# left out: Cafe/HW/Latte/Renderer/Metal/RendererShaderMtl.cpp - macOS and Metal
# left out: Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerEmitMSLAttrDecoder.cpp - macOS and Metal
# left out: Cafe/HW/Latte/LegacyShaderDecompiler/LatteDecompilerEmitMSL.cpp - macOS and Metal
# left out: Cafe/HW/Espresso/Recompiler/BackendAArch64/BackendAArch64.cpp - ARM hosts
# left out: Cafe/OS/libs/nsyshid/BackendLibusb.cpp - USB devices

set(CEMU_COMPONENTS_SOURCES
  Cemu/ExpressionParser/ExpressionParser.cpp
  Cemu/FileCache/FileCache.cpp
  Cemu/Logging/CemuLogging.cpp
  Cemu/napi/napi_act.cpp
  Cemu/napi/napi_ec.cpp
  Cemu/napi/napi.cpp
  Cemu/napi/napi_helper.cpp
  Cemu/napi/napi_idbe.cpp
  Cemu/napi/napi_version.cpp
  Cemu/ncrypto/ncrypto.cpp
  Cemu/nex/nex.cpp
  Cemu/nex/nexFriends.cpp
  Cemu/nex/nexThread.cpp
  Cemu/nex/prudp.cpp
  Cemu/PPCAssembler/ppcAssembler.cpp
  Cemu/Tools/DownloadManager/DownloadManager.cpp
)
# left out: Cemu/DiscordPresence/DiscordPresence.cpp - Discord
# left out: Cemu/DiscordPresence/DiscordRPCLite.cpp - Discord

set(CEMU_CONFIG_SOURCES
  config/ActiveSettings.cpp
  config/CemuConfig.cpp
  config/NetworkSettings.cpp
)
# left out: config/LaunchSettings.cpp - the command line (waterbox/driver/launch_settings.cpp instead)

set(CEMU_UTIL_SOURCES
  util/bootSound/BootSoundReader.cpp
  util/crypto/aes128.cpp
  util/crypto/crc32.cpp
  util/crypto/md5.cpp
  util/helpers/helpers.cpp
  util/helpers/Serializer.cpp
  util/highresolutiontimer/HighResolutionTimer.cpp
  util/IniParser/IniParser.cpp
  util/SystemInfo/SystemInfo.cpp
  util/tinyxml2/tinyxml2.cpp
  util/VirtualHeap/VirtualHeap.cpp
  util/Zir/Core/IR.cpp
  util/Zir/EmitterGLSL/ZpIREmitGLSL.cpp
  util/Zir/Passes/RegisterAllocatorForGLSL.cpp
  util/Zir/Passes/ZpIRRegisterAllocator.cpp
  util/MemMapper/MemMapperUnix.cpp
)
# left out: util/Fiber/FiberWin.cpp - Windows
# left out: util/MemMapper/MemMapperWin.cpp - Windows
# left out: util/SystemInfo/SystemInfoWin.cpp - Windows
# left out: util/Fiber/FiberUnix.cpp - fibers on ucontext (waterbox/driver/fiber_chimera.cpp instead)
# left out: util/SystemInfo/SystemInfoUnix.cpp - the host's processor and memory, for the log
# left out: util/SystemInfo/SystemInfoLinux.cpp - the host's processor and memory, for the log
# left out: util/SystemInfo/SystemInfoMac.cpp - macOS and Metal
# left out: util/SystemInfo/SystemInfoStub.cpp - the host's processor and memory, for the log
# left out: util/libusbWrapper/libusbWrapper.cpp - USB devices

set(CEMU_INPUT_SOURCES
  input/InputManager.cpp
  input/ControllerFactory.cpp
  input/api/Controller.cpp
  input/api/ControllerState.cpp
  input/api/Keyboard/KeyboardControllerProvider.cpp
  input/api/Keyboard/KeyboardController.cpp
  input/emulated/ProController.cpp
  input/emulated/EmulatedController.cpp
  input/emulated/WPADController.cpp
  input/emulated/VPADController.cpp
  input/emulated/WiimoteController.cpp
  input/emulated/ClassicController.cpp
)
# left out: input/api/DSU/DSUControllerProvider.cpp - the host's controllers (the frontend sends the input)
# left out: input/api/DSU/DSUController.cpp - the host's controllers (the frontend sends the input)
# left out: input/api/DSU/DSUMessages.cpp - the host's controllers (the frontend sends the input)
# left out: input/api/GameCube/GameCubeController.cpp - USB devices
# left out: input/api/GameCube/GameCubeControllerProvider.cpp - USB devices
# left out: input/api/XInput/XInputControllerProvider.cpp - Windows
# left out: input/api/XInput/XInputController.cpp - Windows
# left out: input/api/DirectInput/DirectInputControllerProvider.cpp - Windows
# left out: input/api/DirectInput/DirectInputController.cpp - Windows
# left out: input/api/Wiimote/WiimoteControllerProvider.cpp - the host's controllers (the frontend sends the input)
# left out: input/api/Wiimote/NativeWiimoteController.cpp - the host's controllers (the frontend sends the input)
# left out: input/api/Wiimote/hidapi/HidapiWiimote.cpp - the host's controllers (the frontend sends the input)
# left out: input/api/Wiimote/l2cap/L2CapWiimote.cpp - the host's controllers (the frontend sends the input)
# left out: input/api/SDL/SDLController.cpp - the host's controllers (the frontend sends the input)
# left out: input/api/SDL/SDLControllerProvider.cpp - the host's controllers (the frontend sends the input)

set(CEMU_AUDIO_SOURCES
  audio/IAudioAPI.cpp
  audio/IAudioInputAPI.cpp
)
# left out: audio/DirectSoundAPI.cpp - Windows
# left out: audio/XAudio2API.cpp - Windows
# left out: audio/XAudio27API.cpp - Windows
# left out: audio/CubebAPI.cpp - the host's sound devices (the frontend takes the sound)
# left out: audio/CubebInputAPI.cpp - the host's sound devices (the frontend takes the sound)

set(CEMU_IMGUI_SOURCES
  imgui/imgui_extension.cpp
  imgui/imgui_impl_opengl3.cpp
  ../dependencies/imgui/imgui.cpp
  ../dependencies/imgui/imgui_draw.cpp
  ../dependencies/imgui/imgui_tables.cpp
  ../dependencies/imgui/imgui_widgets.cpp
)
# left out: imgui/imgui_impl_vulkan.cpp - the Vulkan renderer (not built yet)

set(CEMU_RESOURCE_SOURCES
  resource/embedded/resources.cpp
  resource/embedded/fontawesome.S
  resource/CafeDefaultFont.cpp
)
# left out: resource/embedded/fontawesome_macos.S - macOS and Metal
