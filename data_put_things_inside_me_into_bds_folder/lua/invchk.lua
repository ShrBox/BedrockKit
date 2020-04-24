Listen("onLCMD",function (name,c,a)
	if c=="invchk" then
		if isOP(name)==false then
			sendText(name,"op only")
			return -1
		end
		local a,b=invapi.dumpInv(a,0),invapi.dumpInv(a,1)
		print(a,b)
		sendText(name,a)
		sendText(name,b)
		return -1
	end
end
)
fs.mkdir("invdump")
Listen("onJoin",function(name)
	local a,b=invapi.dumpInv(name,0),invapi.dumpInv(name,1)
	local f=io.open(string.format("invdump/%s.txt",name),"w")
	f:write(a,"\n",b)
	f:close()
end)
