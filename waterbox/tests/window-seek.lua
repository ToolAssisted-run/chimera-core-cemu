-- For run-frontend.sh's window:program test: a Lua script of Chimera's window
-- (run headless), which puts the machine on a frame through TAStudio's seek
-- and writes the SHA-256 of the console's MEM2 there. Job file (named by the
-- CHIMERA_JOB environment variable): to=<frame>, meta=<path>.
local function writeAll(p, d) local f = assert(io.open(p, "wb")) f:write(d) f:close() end
local job = {}
local jobPath = os.getenv("CHIMERA_JOB")
if jobPath == nil then error("CHIMERA_JOB is not set") end
for line in io.lines(jobPath) do
	local k, v = line:match("^([^=]+)=(.*)$")
	if k then job[k] = v end
end
local function finish(status, detail, extra)
	writeAll(job.meta, "status=" .. status .. "\ndetail=" .. (detail or "") .. "\n" .. (extra or ""))
	client.exit()
end
local ok, err = pcall(function()
	local waited = 0
	while not movie.isloaded() and waited < 120 do emu.frameadvance() waited = waited + 1 end
	if not movie.isloaded() then finish("ERROR", "no movie is loaded") return end
	if not tastudio.engaged() then client.opentasstudio() end
	if not tastudio.engaged() then finish("ERROR", "TAStudio would not open") return end
	local to = tonumber(job.to)
	-- the seek is handed to the machine; it ends on the frame, the machine stopped
	tastudio.setplayback(to)
	local turns = 0
	while (client.isseeking() or emu.framecount() ~= to) and turns < 1000000 do emu.yield() turns = turns + 1 end
	local h = memory.hash_region(0, memory.getmemorydomainsize("MEM2"), "MEM2")
	finish("OK", "", "frame=" .. emu.framecount() .. "\nmem2=" .. h .. "\n")
end)
if not ok then finish("ERROR", tostring(err)) end
