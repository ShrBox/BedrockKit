function tostringex(v, len)
	if len == nil then len = 0 end
	local pre = string.rep('\t', len)
	local ret = ""
	if type(v) == "table" then
	   if len > 5 then return "\t{ ... }" end
	   local t = ""
	   for k, v1 in pairs(v) do
		t = t .. "\n\t" .. pre .. tostring(k) .. ":"
		t = t .. tostringex(v1, len + 1)
	   end
	   if t == "" then
		ret = ret .. pre .. "{ }\t(" .. tostring(v) .. ")"
	   else
		if len > 0 then
		 ret = ret .. "\t(" .. tostring(v) .. ")\n"
		end
		ret = ret .. pre .. "{" .. t .. "\n" .. pre .. "}"
	   end
	else
	   ret = ret .. pre .. tostring(v) .. "\t(" .. type(v) .. ")"
	end
	return ret
end
local function tracebackex()
	local ret = "stack traceback:\n"
	local level = 3
	while true do
	   --get stack info
	   local info = debug.getinfo(level, "Slnu")
	   if not info then break end
	   if info.what == "C" then                -- C function
		ret = ret .. tostring(level-2) .. string.format("\tC function `%s`\n",info.name)
	   else           -- Lua function
		ret = ret .. tostring(level-2) .. string.format("\t%s:%d in function `%s`\n", info.short_src, info.currentline, info.name or "")
	   end
	   local szarg=info.nparams
	   --get local vars
	   local i = 1
	   while true do
		local name, value = debug.getlocal(level, i)
		if not name then break end
		if i<= szarg then name='<'..name..'>' end
		ret = ret .. "\t\t" .. name .. " =\t" .. tostringex(value, 3) .. "\n"
		i = i + 1
	   end
	   level = level + 1
	end
	return ret
end

function EXCEPTION(e)
	return e.."\n"..tracebackex()
end

local G_EVENTS={["onJoin"]=1,["onLeft"]=2,["onChat"]=3,["onCMD"]=4,["onPlayerKillMob"]=5,["onLCMD"]=6}
function Listen(ename,cb)
	if G_EVENTS[ename]==nil then
		error("cant find event")
	end
	-- local x=_G["EH_"..ename]
	-- print(ename,cb)
	if _G["EH_"..ename]==nil then
		_G["EH_"..ename]={}
	end
	if type(cb)=="string" then
		cb=_G[cb]
	end
	if cb==nil then
		error("cant find cb,did you `Listen` before declaring the function???")
	end
	append(_G["EH_"..ename],cb)
end

function get_item_count(name,item)
        if string.find(item,' ')==nil then
                -- item = name + auxVal
                item=item.." 0"
        end
        local suc,res=runCmdEx(string.format("clear \"%s\" %s 0",name,item))
        if suc==false then
                return 0
        end
        return tonumber(string.match(res,"has (%d+) items"))
end
function safe_clear(name,item,count)
        -- legacy API,use sClear instead
        -- return bool (success)
        if string.find(item,' ')==nil then
                -- item = name + auxVal
                item=item.." 0"
        end
        local cnt=get_item_count(name,item)
        if count>cnt then
                return false
        end
        local suc,res=runCmdEx(string.format("clear \"%s\" %s %d",name,item,count))
        return suc
end
function append(tbl,v,i)
	tbl[TSize(tbl)+(i or 0)]=v
end
function runCmdS(cmd,break_on_error)
	local ret,res,tres
	res=""
	for i in cmd:gmatch('([^$]+)') do
		ret,tres=runCmdEx(i)
		res=res.."\n"..tres
		if ret==false and break_on_error then
			return false,res
		end
	end
	return true,res
end
print("LuaInit loaded!!")