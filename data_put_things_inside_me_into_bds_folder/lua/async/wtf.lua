print("xxx",123)
print(_MYID)
SEND("kksk")
function handle(pstr,pint)
	SEND(pstr..tostring(pint*114514))
end
local requests=require("requests")
function cha(name)
	local to="http://aly-2.bugmc.com:2222/BlackBE/check.php"
	local rep=requests.get({to,params={["id"]=name},timeout=2})
	SEND(rep.status_code,rep.text)
end
