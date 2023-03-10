# FontVideo

Play video by fonts in a console window by composing characters.

Using FFmpeg API to decode the input file, then the video stream is rendered to the specified output, and the audio stream is played via `libsoundio`.

Default is output to `stdout`, but you can specify its output to a file.

## Usage:

    Usage: FontVideo.exe -i <input> [-o <output.txt>] [-v] [-p <seconds>] [-m] [-n] [-w <width>] [-h <height>] [-s <width> <height>] [-S <from_sec>] [-b] [--invert-color] [--no-opengl] [--no-frameskip] [--no-auto-aspect-adjust] [--opengl-threads <number>] [--assets-meta <metafile.ini>] [--output-frame-image-sequence <prefix>]
    Or: FontVideo.exe <input>
        -i: Specify the input video file name.
        -o: [Optional] Specify the output text file name.
        -v: [Optional] Verbose mode, output debug informations.
          Alias: --verbose
        -vt: [Optional] Verbose mode for multithreading, output lock acquirement informations.
          Alias: --verbose-threading
        -p: [Optional] Specify pre-render seconds, Longer value results longer delay but better quality.
          Alias: --pre-render
        -m: [Optional] Mute sound output.
          Aliases: --mute, --no-sound, --no-audio
        -n: [Optional] Normalize input.
        -w: [Optional] Width of the output.
          Alias: --width
        -h: [Optional] Height of the output.
          Alias: --height
        -s: [Optional] Size of the output, default is to detect the size of the console window, or 80x25 if failed.
          Alias: --size
        -b: Only do white-black output.
          Alias: --black-white
        -S: [Optional] Set the playback start time of seconds.
          Alias: --start-time
        --log: [Optional] Specify the log file.
        --invert-color: [Optional] Do color invert.
        --no-opengl: [Optional] Do not use OpenGL to accelerate rendering (default).
        --use-opengl: [Optional] Use OpenGL to accelerate rendering.
        --no-frameskip: [Optional] Do not skip frames, which may cause video and audio could not sync.
        --no-auto-aspect-adjust: [Optional] Do not adjsut aspect ratio by changing output width automatically.
        --opengl-threads: [Optional] Set the OpenGL Renderer's thread number, default to your CPU thread number divide 4.
        --assets-meta: [Optional] Use specified meta file, default is to use 'assets\meta.ini'.
        --output-frame-image-sequence: [Optional] Output each frame image to a directory. The format of the image is `bmp`.

## Demo

	丶丶丶丶丶丶丶丶丶丶丶丶卩阝丶丶丶丶丶丶丶刂丶丶丶丶丶丶丶丶刂钅丶丶丶钅辶刂丶丶
	丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶厶卩丶丶丶丶丶丶丶丶丶冖勹丶钅刂钅丶入灬
	丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶灬灬丶丶丶丶丶丶丶丶丶丶丶十艹冖艹灬一卩宀
	丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶灬丶丶丶艹丶丶丶丶丶丿丶丶丶丶广艹冖丶灬十冖丶灬
	丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶灬丶丶灬令纟丶丶宀艹丶丶卩丶灬艹艹丶丶灬十灬卩厶厶
	丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶刂厶刁扌丶丶丶丶卩卜丶丶十厂讠二灬刂厶小丶灬厶厶
	丶丶丶丶丶丶丶丶丶丶丶钅丶丶丫艹十钅冖讠卜钅钅丶氵勹冫灬艹阝丶辶灬卩十丶灬丶丶丶
	丶卩阝丶丶丶丶丶丶丶丶丶丿亻犭丶丶丶丶丶丶灬灬卩入丶丶丶丶丶灬小灬丶辶刂辶灬灬刂
	钅丶丶丶丶丶丶丶丶灬丶丶丶丶犭丶冫丶丶丶丶丶刂冖丬讠卞冫丶卩灬卩丶刂卩十丶丶丶丶
	丶丶丶丶丶丶丶丶丶丶丶丶人丶彡二丶灬丶丶八丶氵丶飞丶飞丿卩卩小丶丶卩钅丶丶丶丶丶
	丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶灬丶丶丶丶屮丶廴卩丶凵勹艹丶卩丶丶卩饣丶丶丶丶丶丶
	丶丶丶丶丶丶丶丶丶丶丶卜亻丶飞灬丶丶礻灬斗丶斗阝刂廴氵冖讠丶灬卩艹丶丶丶丶丶丶丶
	丶丶丶丶丶丶丶丶丶丶丶彳忄讠飞丶灬刂心丶刂丶丶勹刂厂讠冖丶灬卩钅丶丶丶丶丶钅勹辶
	丶丶丶丶丶丶丶丶丶丶灬忄心卞凵七氵丶下阝灬由耸丶刂丶刂丶丶丶钅丶丶丶丶灬厶二冖阝
	丶丶丶丶丶丶丶丶灬卩辶忄少飞彳讠久丶丶丶常荞卟阝刂丶灬氵丶冫勹丶丶丶人丶丶丶丶丶
	丶丶丶丶丶丶丶灬二丶丶刂几丶丫详偷廴亻丶灬丶丶丶卩水冂礻丶丶丶丶灬刂丶丶丶丶灬刂
	丶丶丶丶丶丶厶钅丶丶丶卜阝冫刂丶孓丶丶丶冖冖冖厶丶飞才穴丶冫丶丶丶丶丶灬人丶丶丶
	丶丶丶丶丶卩丶丶丶丶丶丶入丶冫兮丶艹丶冖丶丶丶丿丶航让下凵礻丶丶辶灬丶小丶丶丶丶
	丶丶丶灬小丶丶丶丶丶丶丶犭辶尔灬丶冖丶丶宀亻丶讣丶骰胩享乩上丶冫小丶丶丶丶丶丶丶
	丶丶灬小丶丶丶丶丶钅阝丶丶丶灬宀心丶灬丶丶尘叮丬门邀骆戏药含弋冫丶丶丶丶丶丶灬小
	灬卩丶丶丶丶丶钅纟阝灬灬丶丶丶艹义久舛铀士芍牛丬刁瑾黉飞劝冖丶冫丶丶丶灬灬卩丶丶
	卩丶丶丶丶丶丶钅勹卩灬刂丶丶丶丶冖久闩谦诊凵丶卜飞默黔廴疒灬刂卜丶丶辶小丶丶丶丶
	丶丶丶丶丶丶钅厶刂讠冖丶丶讠匕丶丶刁疒龃酎阼莅疒飞黔虬爷丶彡卩卜丶丶扌丶丶丶丶丶
	丶丶丶丶钅厂厶衤冖卩丶丶灬丶儿讠丶丶旷谑瞅铝板廴门骅化圹宀冫冖屮氵丶丶丶丶丶丶丶
	丶丶丶钅阝小丁勹丶丶灬厶刂丿丶亻丶仃永龙傅封载犷丶芡谛廴丶卜丶屮礻丶丶阝丶丶丶丶
	丶丶阝勹一十丶钅灬卩小丶丶丶卜丬丶上企飞旯蓊鲈扩丶试脊丶灬犭丿飞认丶丶丶丶丶丶丶
	个阝勹一丿丶灬厶卩丶丶灬灬丫丶丬耸茜曲涎匙嵬坨巨刂河疒丶丶氵冖刂予氵丶氵丶丶丶丶
	钅厶十卩钅灬灬丶丶辶丶钅丶尘凵飞偕鼾黔跸唤廷莰坠丶圹丶刂匕丿刂丶匀讠丶讠丶丶丶丶
	厶十钅辶厶丶丶灬灬丶灬丶诂龠鼾尬粟辉酊航君匙阡丶丶氵讠丶卞钅冖丶刂钅丶忄丶丶丶丶
	灬灬卩小丶厶二丶灬小忄凵酎酐郫俏骨鲶裥舫挈质牝丶讠礻丶丶刂讠灬丶氵钅丶扌丶丶丶丶
	丶灬灬卩丶丶灬卩艹丶丶虔酎狒骱偈赏晦舭鲨谪朊钺凵丶人丶丶刂灬广丶廴勹丶丶丶丶丶丶
	刂丶丶丶灬钅艹丶丶丶记惯兼略励鲋闵鞒惯略凯顿龄鱼疒纟丶丶飞廴丿丶丿刂刂丶丶丶丶丶
	十十十钅丶丶丶灬辶丶鲛鼾雪鲶瀑裥蚧雪略窬鲆莠朊箩牝丶上丶丶讠亠灬丫刂丶阝丶丶丶丶
	丶丶丶丶丶丶灬凵头诎谑略慌渑看帕铲鬲遮黔舱蛄帆胜谎波胩扩凵胙辶丶丶丶氵丶灬灬丶丶
	丶丶刂辶丶凵禽盒恤舔臭鲶曲械酐垌纩驽鲶舢骱邴哌朊备嗤蜘黔嵴孟厂丶丶丶丶灬午二灬一
	十二丶灬浍韬鲎岫掰偕酐鹄渑濠鲶黔车戍鼾雉骱曾骱乾匙倾财粟蕲裥必疒阝飞囗半必尘壬廴
	灬灬冲洳黾猎窜鞍晶澡鲶鲨棍慕嗥骱豕川媲鲶黔驵鞋彰龄箩教鲆蒎黔唤疒冫飞会句叉尸刈讣
	允铽垡洫胥蓼愍糁翰劈黔猫确嗪婺骱佐丶谫鼾黔闻眚鲛崭桃忄声昏鲶掣轧犭屮铲肀廴廾节才
	乾洫逾逸恤盒奎域略鞋遮鸺喝豁赫骱虽钅译鼾黔航箱骱逾纩冖丶丐鼾寓鬼丬屮旷许飞闩刈冂
	鲶逸逸鲨遮岫鲶媒谑骓遮甙丶节芳芦严丶刈眚舢趑瞀彀趴丶了丶门酎憧阕亻丁讧认必占占三
	黔榆銮銮通胍陂傥雪凋鹞钊卩阝钅阝丶讠门婶舢骱笄疒丶灬八冖刁押缣猬疒卩冖讫谂当沾浍
	啤倨鹪縻缜默向邕俺骨僮轧卩艹阝阝阝丶丶茜繇嗥犷阝氵冖艹刂飞劲锣篷矿丶灬下严司侩由
	胶亮夏暴蝼鼻咣毁黔僧酞护丶艹钅钅阝丶刂星舢鲨犷阝纟辶丶钅丶飧姿亮胙凵疒飞凵吖严芳
	穹掉雏萆箩批埽谑酊遘看牡丶钅钅阝阝丶丶诽毗鲨犷纟纟冖亻厶卩丫昃契飒心上一艹久少尘
	诲鲨谕庙皴啊靠鲶韬蟒眚眇讠艹艹钅阝丶丶邝畲跷杉氵灬丶丶丶犭丶戏脉贯轧少九爿勺歹歹
	铘默鞍懋寮蒺媳黔驷鄙熟鞒圹十冖钅厂丶灬门赔裥纩纟冫丶阝十刂刂读谲哀驹违甘女本元式
	骰酐呷煜剪潦馇刷韪喟赛町认勹十钅阝丶谷凶舸黔车阝纟扌丶丶丶丶声狎喉否秽虼札凵下产
	氨骱逾逾娩啤畲妒甲苛芍气守冫勹宀冖刂肩樊鞒鼓佐钅灬辶纟灬灬飞严严疒芋杩匠翌圹阝艹
	萆匪黏硫轻俺璃疒丶冖灬刂下今丶冖冖丶叶气趸搦胤丶纟辶灬灬丶少子产艹艹艹冖冖冖丶丁
	扩节苛页训裸费匚厶灬冖灬卩疒疒灬丶彡丶卜戏鲶唬凵钅钅丶灬疒艹冖冖艹冫丁艹艹艹阝艹
	芦让疒丶音冒徜疒凵止一亠亠亠爿刁丶灬灬丶浅鼾黔铲丶宀冖艹丶氵艹辶冖钅冖灬八厂灬灬
	节厅失钆舫盒亵头必必廴艹艹冖介寸飞廴仑亠诮郫婺角灬灬灬灬灬灬灬灬灬灬灬灬灬十卩止
	戈圩爪刘炯骨舱阶头产午灬勹钅厂艹尸礻阝阝贾咿啤炯凵小厶辶厶厶厶卩厶阝灬小灬灬刂凵
	爪许气徉谫雯哌梃驴针让凵灬灬丶刂丶灬阝丶舫宿褚敝仄卩灬丶厶丿丶斗一户钅丶艹一上尘
	下产气戏嫔咿钾亭旷谈乌年牟玎如仕台未头询陵黄遑夷尘凵心伞沁让凵丶丶廴凵氵会申叹孙
	下产必旷严严芍严业它戏企凵戌盯气亏天剑钢骆谐炯芪囗戈廴出璃骼芷囗丘瓦电钟饮宋仵页
	飞尹飞疒飞下叉米万气叉厅氏瓜勾卢川它啮窒鬼遑昊沃芍戈引邱腭靥益谧逸憎慊洼点赤页开
	严止卞寸飞凡纩少乍彐办众另论卫彐闩孔伢梦萍萃队凸它讥州汹囚罨鲺鲺唰叠酝黄虻休矿开
	七飞弋凵必严木飞刈刈让空凡川长冗凡矿礼刈它尹仅乌节肀宗侍荠萃昂察雯昂舁漆罗丑仗凶
	卡工下众乍下匀飞刈彐爷歹卟闩乍叶台忑彐刈评幻失节专节芍芏肀罕芋芦何亨芦苎页诖冈讪

![Source Picture](./demo.jpg)
