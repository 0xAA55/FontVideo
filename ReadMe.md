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

丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶冫丶彡丶一丶丶丶丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丫彳丶丶丶伞榫叮丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶勿咚丶丶丶丶丶寸丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶泌勺丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丶丶丶心刂嫒龙命灬丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶冫全公号忄飞嫌闪半丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶囗账弘廴门勇仄丶丶讠丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶旷摄荏运刂防圹丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶丶丬寅劬崽丶拉匚丶丶匕丶丶丶丶丶
丶丶丶丶丶丶丶丶丶丶丶後嗡韭意坚丶脊丶丶丶丫丶丶丶丶丶
丶丶丶丶丶丶丶丶灬圣占恻黔寥骇哔刂丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶丶偎勇郎园媲褚揿亻丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶证氨黔筲喂晦刷影虬丶丶丶丶丶丶丶丶丶丶丶
丶丶丶丶丶丶丶惯勇蚋嘹垅勇春憋团吣冫丶亠丶丶丶丶丶丶丶
丶丶丶丶凵尜谣勇惯锚剩闵彀嗦惝彰韧惯龙谣丶丶丶丶丶丶丶
丶丶灬卣描戮馔剿岫氨佣闪兽彀抻数卺堡褒酎犷一丶凶叱忐认
丶冲减傩塑蜂兼喏憎檠帕匚顿崃唪勐彰筲嘭辜恤扌刂肚旷仫钋
备抽凄恤奎锦彀啸雩彀婺吖译剿唪噔囵阝冖褡堞钇卩贮穴芋旷
幽逸鲨晶咖鹄崃萝艹节严丶刈剿勐酐严丶丶讹哺轮丶屯讪由召
骛匍痛啊劾值媲呻丶丶丶丶刂勇嗦丶丶丶丶肀拽值丶冖邝鸣庙
崆提厣胛轶倨襁呻丶丶丶丶丶憷晌丶丶丶丶冖地凋仝忄饣勹烈
音层轿啾嶷孰最律讠丶丶丶丶袅劈丶丶丶丶丶沣韵贮力丶亏天
爬葶恿凳姨嗪傅害圹丶丶丶丶译崂阝丶丶丶丶沾赃恃表旷亢岑
酐影抽威春哼拜咚丸丶丶丶师喹替匚丶丶丶丶怂开烈钲拓尸丶
赍荨萨宣岬丶丶丶冖冫丶丶广肀崂矿丶丶丶一冖丶丶丶冖丶丶
吣产乙倡隹彡丶丶丶一刂丶丶勹剿角丶一丶丶丶丶丶丶丶丶丶
节孕咩俺莒沁众丶丶丶冖斗一川黔嘧冫丶丶丶丶丶丶丶丶丶一
杰丐诮喝娉凼劣阝丶丶丶丶丶沣啮寮旷丶丶丶丶冫彡丶丶丶匕
亏气伊粹茕饺末诀竺峦内并凶亮拽跸孙头长皂凼辶从诠本老写
乇扩斤歹孔糸矛俨完夺芍汽咱遥凄药区仪来灌黔恤酋鼻吣非朽
巾孑穴必芍讧壬汽吟王沪讥苻卯苏孕邝证迢抻赚囔瘿鲸亭肜诀
计穴岁它方讧议米污芍沪孙旷节识队苄芊迷甲穹荠莜秆梦申沾

![Source Picture](./demo.jpg)
