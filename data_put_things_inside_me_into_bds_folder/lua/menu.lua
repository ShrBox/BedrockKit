local menu={}
local I18N_SHOP1="项目"
local I18N_BUY_SUCCESS="成功购买 "
local I18N_MONEY_NOT_ENOUGH="你钱不够！"
local I18N_ITEM_NOT_ENOUGH="物品不足！"
local I18N_BUY="购买 "
local I18N_SELL="卖出 "
local I18N_SELL_SECUESS="成功卖出 "
local function sendMenu(name,mn)
    if menu[mn]==nil then
        sendText(name,"wrong menu name")
        return
    end
    if menu[mn]["OP"]~=nil and isOP(name)==false then
        sendText(name,"op only")
        return
    end
    GUIR(name,menu[mn]["S"])
end
function GUIFORWARD(name,a,b,extra)
    local isop=menu[extra]["OP"]
    if isop~=nil then
        if isOP(name)==false then
            sendText(name,"op only")
            return
        end
    end
    menu[extra]["F"](name,a,b,extra) 
end
local function TBL2STR(T)
    local rv="["
    local i=0
    repeat
        local v=T[i]
        if v==nil then break end
        rv=rv..'"'..tostring(v).."\","
        i=i+1
    until false
    if i~=0 then
        rv=rv:sub(1,-2)
    end
    return rv..']'
end
local function loadAll()
    local mname=nil
    local data={}
    local type
    local serial=""
    local shop_data={}
    for line in io.lines("config/menus.txt") do
       if line:match("([^%s]+)")~=nil then
        if line=="end" then
            if mname==nil then
                error("unexcepted end")
            end
            if type=="shop" then
                serial=serial.."type=dropdown,text="..I18N_SHOP1..",args="..TBL2STR(shop_data)
            end
            data["S"]=serial
            -- print(serial,"\nis\n",mname)
            menu[mname]=data
            mname=nil
	        data={}
        else
            if mname==nil then
                -- shop 
                local title,content
                type,mname,title,content=line:match("(%a+)%s+([^%s]+)%s+(%b<>)%s+(%b<>)")
                title=title:sub(2,-2)
                content=content:sub(2,-2)
                if type:sub(1,4)=="menu" then
                    local isopMenu= (type=="menuop")
                    local mnamec=mname
                    data["F"]=function(name,idx) 
                        menu[mnamec]["D"][idx](name)
                    end
                    data["D"]={}
                    if isopMenu then
                        data["OP"]=1
                    end
                    serial=string.format("type=simple,title=\"%s\",cb=GUIFORWARD,content=\"%s\",extra=%s\n",title,content,mname)
                    type="menu"
                end
                if type=="shop" then
                    local mnamec=mname
                    data["F"]=function(name,raw) 
                        menu[mnamec]["D"][raw[2]](name) --label dropdown
                    end
                    data["D"]={}
                    serial=string.format("type=full,title=\"%s\",cb=GUIFORWARD,extra=%s\ntype=label,text=\"%s\"\n",title,mname,content)
                    shop_data={}
                end
                if type=="select" then
                    data["tp"]="select"
                    local cmd=line:match("%a+%s+[^%s]+%s+%b<>%s+%b<>%s+(%b())")
                    cmd=string.sub(cmd,2,-2)
                    data["F"]=function (name,raw,dat)
			-- print(cmd:gsub("%%dst%%",dat[1]):gsub("%%name%%",name))
                        runCmdAs(name,cmd:gsub("%%dst%%",dat[1]):gsub("%%name%%",name))
                    end
                    serial=string.format("type=full,title=\"%s\",cb=GUIFORWARD,extra=%s\ntype=label,text=\"%s\"\ntype=dropdown,text=target,args=%%0",title,mname,content)
                end
            else
                if type=="menu" then
                    -- type(run,runas,exit,lua,sub) <text> <img> (args)
                    -- chain <text> <img> (cmdchain) (execute)
                    local tp2,text,img,arg=line:match("(%a+)%s+(%b<>)%s+(%b<>)%s+(%b())")
                    text=text:sub(2,-2)
                    img=img:sub(2,-2)
                    serial=serial.."text=\""..text.."\""
                    if #img~=0 then
                        serial=serial..",img=\""..img.."\""
                    end
                    serial=serial.."\n"
                    arg=arg:sub(2,-2)
                    if tp2=="chain" then
                        local succ=line:match("%a+%s+%b<>%s+%b<>%s+%b()%s+(%b())")
			succ=succ:sub(2,-2)
                        append(data["D"],function(name)
                            if runCmdS(arg:gsub("%%name%%",name),true) then
                                runCmdS(succ:gsub("%%name%%",name))
                            end
                        end)
                    end
                    if tp2=="run" or tp2=="runas" then
                        append(data["D"],function(name)
                            if tp2=="run" then
                                runCmd(arg:gsub("%%name%%",name))
                            else
                                runCmdAs(name,arg:gsub("%%name%%",name))
                            end
                        end)
                    end
                    if tp2=="exit" then
                        append(data["D"],function() end)
                    end
                    if tp2=="lua" then
                        -- local FN=_G[arg]
                        -- if FN==nil then error("func "..arg.." not found") end
                        append(data["D"],function(name)
				local FN=_G[arg]                         
				if FN==nil then error("func "..arg.." not found") end
                            FN(name)
                        end)
                    end
                    if tp2=="sub" then
                        append(data["D"],function(name)
                            sendMenu(name,arg)
                        end)
                    end
                else
                    -- shop
                    -- sell <name> <ite> price count
                    -- buy <name> <cmd> price
                    local tp2,nam,ite,price=line:match("(%a+)%s+(%b<>)%s+(%b<>)%s+(%d+)")
                    price=tonumber(price)
                    nam=nam:sub(2,-2)
                    ite=ite:sub(2,-2)
                    if tp2=="buy" then
                        append(data["D"],function(name)
                            if rdMoney(name,price) then
                                runCmdS(ite:gsub("%%name%%",name))
                                sendText(name,I18N_BUY_SUCCESS..nam)
                            else
                                sendText(name,I18N_MONEY_NOT_ENOUGH)
                            end
                        end)
                        append(shop_data,I18N_BUY..nam)
                    else
                        local cnt
                        cnt=line:match("%a+%s+%b<>%s+%b<>%s+%d+%s+(%d+)")
                        cnt=tonumber(cnt)
                        append(data["D"],function(name)
                            if safe_clear(name,ite,cnt) then
                                addMoney(name,price)
                                sendText(name,I18N_SELL_SECUESS..nam)
                            else
                                sendText(name,I18N_ITEM_NOT_ENOUGH)
                            end
                        end)
                        append(shop_data,(I18N_SELL..nam))
                    end
                end
            end
        end
    end
  end
end
loadAll()
Listen("onLCMD",function(name,c,a)
    if c=="m" then
        sendMenu(name,a)
	    return -1
    end
    if c=="mreload" then
    	loadAll()
	    return -1
	end
end)
