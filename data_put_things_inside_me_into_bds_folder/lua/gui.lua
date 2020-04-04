guess_game_ans={}
math.randomseed(os.time())

function u_train(name)
last_train_day=dget(name,"last_train_day")
today=os.date('%d')
train_cnt=dget(name,"train_cnt")
train_num=tonumber(train_cnt)
if train_num==nil then
	train_num=0
end
if today==last_train_day then
	if train_num>3 then
		sendText(name,"3 times a day,tomorrow plz")
		return
	end
else
	dput(name,"train_cnt",0)
	train_num=0
end
	dput(name,"last_train_day",today)
	print(tonumber(train_cnt))
	dput(name,"train_cnt",train_num+1)
	GUI(name,"main_menu")
end



function main_menu(name,idx,text)
	if idx==0 then
		add1=math.random(10)
		add2=math.random(10)   -- generate two num in [1,10]
		guess_game_ans[name]=add1+add2
		GUI(name,"guess_gui",add1,add2)
	else
		sendText(name,"You Closed GUI")
	end
end
function guess_gui_cb(name,rawdata,text)
	-- 1:label 2:input
	need_ans=guess_game_ans[name]
	if need_ans==nil then
		sendText(name,"error")	
		return
	end
	myans=tonumber(rawdata[2])
	if need_ans==myans then
		sendText(name,"well done!you get $1")
		L("addMoney",name,1)
	else
		sendText(name,"wrong answer")
	end
end
