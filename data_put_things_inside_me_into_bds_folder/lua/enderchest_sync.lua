local b64=require("base64")
fs.mkdir("chests")
local PATH="chests/%s.dump"
Listen("onCMD",function(name,cmd)
	if cmd=="pushc" then
		local fn=string.format(PATH,b64.encode(name))
		invapi.MoveEnderChestTo(name,fn)
		sendText(name,"done")
		return -1
	end
	if cmd=="popc" then
		local fn=string.format(PATH,b64.encode(name))
		if fs.exists(fn) then
			invapi.LoadEnderChestFrom(name,fn)
			fs.del(fn)
			sendText(name,"done")
		else
			sendText(name,"cant find")
		end
		return -1
	end
end
)
