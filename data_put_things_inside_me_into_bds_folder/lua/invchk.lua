Listen("onLCMD",function (name,c,a)
	if c=="invchk" then
		local a,b=dumpInv(a,0),dumpInv(a,1)
		print(a,b)
		sendText(name,a)
		sendText(name,b)
		return -1
	end
end
)
