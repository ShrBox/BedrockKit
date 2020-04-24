require("pl.stringx").import()
local CMD_CMD={}
local CHAT_CMD={}
local CMD_COND={}
local function loadAll()
    for line in io.lines("config/cmds.txt") do
        -- alias <from> <to>
        -- cond <cmd> <condition>
            if line:sub(1,1)~='#' then
                local tp,from,to=line:match('(%a+)%s+(%b<>)%s+(%b<>)')
                from=from:sub(2,-2)
                to=to:sub(2,-2)
                if tp=="alias" then
                    if from:sub(1,1)=='/' then
                        CMD_CMD[from:sub(2)]=to
                    else
                        CHAT_CMD[from]=to
                    end
                else
                    -- cond
                    CMD_COND[from]=to
                end
            end
    end
end
loadAll()
Listen('onChat',function(name,chat)
    local to=CHAT_CMD[chat]
    if to==nil then return end
    if to:sub(1,1)=='!' then
        _G[to:sub(2)](name)
    else
        runCmdAs(name,to)
    end
    return -1
end
)
Listen('onCMD',function(name,cmd)
    for prefix,cond in pairs(CMD_COND) do
       if (cmd):startswith(prefix) then
    	local res=runCmdS(cond:gsub("%%name%%",name),true)
        if res~=true then
            sendText(name,"cmd cond failed")
            return -1
        end
       end
    end
    local to=CMD_CMD[cmd]
    if to==nil then return end
    if to:sub(1,1)=='!' then
        _G[to:sub(2)](name)
    else
        runCmdAs(name,to)
    end
    return -1
end
)
-- Listen('onLCMD',function()return -1 end)
