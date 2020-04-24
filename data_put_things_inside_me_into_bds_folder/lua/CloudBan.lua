local CBWorker=startThread("lua/async/cloudban.lua",function(name,bad)
	if bad==1 then
		Log("bad player",name)
		runCmd(string.format("/skick \"%s\"",name))
	end
end)
print("cb worker",CBWorker)
Listen("onJoin",function(name)
	TSendMsg(CBWorker,"cha",name)
end)
