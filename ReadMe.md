# FontVideo

Play video by fonts in a console window by composing characters.

Using FFmpeg API to decode the input file, then the video stream is rendered to the specified output, and the audio stream is played via `libsoundio`.

Default is output to `stdout`, but you can specify its output to a file.

## Usage:

	Usage: FontVideo.exe -i <input> [-o <output.txt>] [-v] [-p <seconds>] [-m] [-w <width>] [-h <height>] [-s <width> <height>] [-S <from_sec>] [-b] [--invert-color] [--no-opengl] [--no-frameskip] [--opengl-threads <number>] [--assets-meta <metafile.ini>] [--output-frame-image-sequence <prefix>]
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
		--no-opengl: [Optional] Do not use OpenGL to accelerate rendering.
		--no-frameskip: [Optional] Do not skip frames, which may cause video and audio could not sync.
		--opengl-threads: [Optional] Set the OpenGL Renderer's thread number, default to your CPU thread number divide 4.
		--assets-meta: [Optional] Use specified meta file, default is to use 'assets\meta.ini'.
		--output-frame-image-sequence: [Optional] Output each frame image to a directory. The format of the image is `bmp`.

## Demo

丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶扌丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶一丶丶丶讠丶刂丶丿丶丶饣丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶讠刂丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶艹丫丶十丶丶丶丶灬远耸丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丁刂丶丶丶丶丶一赆呷旷丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶艹丶丶详岫仁丶丶丶丶丶丶丶厶阝丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丌丶丶丶丶丶丶丶丶汁小穴丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶冫丫丶丶丶丶丶丶丶丶丶垂汁工忄丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶勺丶丶丶丶丶丶丶丶丫丶崤隹息泌辶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶一丶灬丶丶尘钌丶刂塍昌冻芪夼丫丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丿舛恤十亨寸丶刁堀屙丶均丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶儿闩傀旷兰彡丶门慕骱讠忄丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶饣丶丶刁阝器酐荪莅纟几毂趴爷丶丶丶卜丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丿讠丶丶旷售蜘唿倒阝刂峥忆圹丶丶丶丫丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶亻丶一牛序雹忡颜圹丶芡谜阝丶丶丶丬丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶土尘门凭葑婢扩丶讲钗丶丶丶丶刂么丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丿谣啬晏趾臬踅卤圣丶狩产丶丶丶丶丶干丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶尘辶丶倨佛彀敫套矾彼肽丶扩丶丶忄丶丶丶冖丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶诂喊剿缸崴剿恿敫冒臬肛丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶灬酐兼酊蚪青彀揎嵇辜偾告丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶减氨椎娠倮带巢榆骱恿彰尉讠丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶廿催寓勇绱酎沌勇榫勇剐鲛峻龟丶丶丶丶彐匚丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶鲛勇喏喹谭剩耖啻剩劈剃唪彰呷牝丶辶丶丶讠丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶凵头法喵剿懋嗤雾暗钊鬲桐剩剃惝凯帙唠战胩们冂库丶丶丶丶丶丶丶丶丶
丶丶丶丶丶仑勐唬愉彘晶彀偕婉酐感纩想幽崃垦悯剐峻雀惰蜘剩暇雀丶丶丶丶丶彡辶二丶
丶丶丶灬汕嗳晶幽僭倨兼彀媒憧黔姜扒徉勘撩晏昌候彰亮减较隽蕲剩必父丶卩科炉乩汕讪
丶灬乩追嚓樟僮晰隳僧剿崴兢戡喏敞朱川廊彀揍伽剧揿彰度鸫影恿崞臬匚丶予卣金光尹识
仝谥庙抽嗌蓼恿懋膂榫懋媲瑾噗感勐脊丶奄咖晏囹抛影彰栩冖饧恸鲶喈抡丶术砰町匕冰苛
凯减偷恤惫愉勐城崂彀崴剩萼庵春婺虽丶沔勘啬惝晷勉届纩丶丶戊鲶遍挽丶术秆矿飞究戌
鲨奎域值倨幽咖谓怫嗤害矾丶肀烀甲严丶刈愚嘏奎着鲶町丶丶丶川酐煸剧丶丶韦扎见诒由
鲈噙庵嵬煽曼贩莱惯崴剿钊丶丶丶丶丶讠刂愚励棘药丌丶丶丶丶勹莨嫌樽匚丶冖译兹釜亩
喝催畴痿嘁暇凫宣倨劈奢铲丶丶丶丶丶丶丶茜彀嗦犷丶丶丶丶丶刂街馈瑾矿丶丶冖节吼减
剌跖戛妻畴募戒嘧崃强帻护丶丶丶丶丶丶丶驽喹岫犷丶丶丶丶丶丶两硷莴胙辶忄宀厶冖苁
穹倮嗑荸呱阽崴惯彀遽晷眇丶丶丶丶丶丶丶译嘏啻朴丶丶丶丶丶丶丫净倬愠水匕丶丶土头
诲揞唉抵岫寨傲暝僧蟒愚眇讠丶丶丶丶丶丶邝俑孰朴丶丶丶丶丶丶丶沌影亮轧丈阝卩穴闩
悃啊塬勇恿恿崞剩驷蝉嘧鲶旷丶丶丶丶丶丶门掰剩矿丶丶丶丶丶丶丶读境岚龟奋拦戈尤圬
事檠零墚孤埋惜剩酎硒墅耔头丶丶丶丶丶谷注匪剩布丶丶丶丶丶丶丶亭押嗥宓寿谥名匚饣
晷勐嗌偷尾喝鲨芗甲梦芍气字丶丶丶丶丶垣俚鞋毂虬丶丶丶丶丶丶刂岁严汁艿知荭冠旷丶
荸蛊勐俺羟偕僧扌丶丶丶丶刂令丶丶丶丶什气趸困患丶丶丶丶丶丶夂亍饣丶丶丶冖冖丶丶
纩肀罗页训捭费阝丶丶丶丶丶冖广丶丶丶丶丶谅崃晌阝丶丶丶丶一丶丶丶丶丶丶丶丶丶丶
卦必饣冫音巽悯宀宀忄丶丶亠丶忄勺丶丶丶丶寻剿彀此丶亠冖丶丶丶丶丶丶丶丶丶丶丶丶
节邝社吣敝惫抻夫让止彳丶丶丶宀宀弋仁饣丶诮懋喵者丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
兑圮沃诂垌恿栀叵严孑丁丶丶丶丶丶一丶丶丶蛮唪喝锷纟丶丶丶丶丶丶丶丶丶丶丶丶丶一
乔岑岁译埸寮搿影阼针让冫丶丶丶丶丶丶丶丶娠寥埸敞仄丶丶丶冫丶丶丬一丬丶丶丶丶辶
歹严刈谅峥寐呷蛉钋戊钆串串叫讳号会寺夫刽傻掇凄拿仫山仁伞马企名丶刂父企冫吐估秆
孑孑廿乔刈节苄忑诀吣谅命必沌杓节廿识迫敫掇指娲粹叩谷衤屯魂髻雯吆缶豇嵬陟巨来铲
允米仄计口亏允宋灭斥忑肀甲系比王闪沉谐逸剐凄婴芏邝名杉邱锞墼鹆晶琶矗蝶坯瓜炸芏
仟九木计亏冗泸巾讧刈刈斥迟泛王齐廿瓦秆芮赏芰咒孕辛讪浮坞险瞿鼬鳍儡墼鄱黄轮柞纶
尔穴为穴忐乔乍讨讧刈冗伏忑比芍芍汽芄扎闪苄苄咛矢邝苎申谛亭黍锣惯檠黄愁牮声且钍
孑孑孑步守卢彐讨讧刈汝灾芍芍芍沪寺吣节闪评纩吉苄苄邝芊浓苎芋甲妒田帛苛铲芹诛状

![Source Picture](./demo.jpg)
