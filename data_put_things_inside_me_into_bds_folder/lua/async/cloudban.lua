local requests=require("requests")
function cha(name)
	print("cha",name)
	local to="http://aly-2.bugmc.com:2222/BlackBE/check.php"
	local rep=requests.get({to,params={["id"]=name},timeout=2})
	SEND(name,tonumber(rep.text))
end
