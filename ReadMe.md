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

	                                                                    
	                                                                    
	                                                                    
	                                                                    
	                        ,~      >                                   
	                                                                    
	                                                                    
	                 .             '    '                               
	                        \        . [ \    \                         
	                   '            _,.|  L                             
	                    H |  [     ' .                                  
	                      M  ]`\  \  _g@@@@r     ]                      
	                     | \  ggg_    `"C"`    /N]                      
	                    )'   <`%@T            ]$[q&                     
	                          ^               @@gH\\                    
	                       .G. ".     `  _g@  @@@L@@@@L_                
	                    '   >-._ "rwg__.@&@   @@@@^@@@*".               
	                        d   *v @@@@ Q#`   @@@@ *#                   
	                        p     @ @@@@@@mg  @@@%Qc     |              
	                        F  ^  @@d@@@@@@@  @@&Q@      |(             
	                        \U    W$ @@@@@@@  %M@@%      |@             
	                          [ g@g@g#g@@@@%  @@@C     /  @A            
	                          L@@@@@@@@@@@@@p @@"         k%L           
	                     g@@@@@@@@@@@@@@@@@%" |    y7       )           
	                   _@@@@@@@%@@@@@@@@@@@         '     [             
	                  ,@@@@@@@@M@@@@@@@@@@@@_ \'     _ `  '             
	                 g@@@@@@@@@@@@@@@@@@@@@@@g \ ,  *P~L`               
	             ___g@@@@@@@@@@@d@@@@@@@@@@@@@gg@p[] @p    h            
	         _g@@@@@@@@@@@@@@@@@ @@@@@@@@@@@@@@@@@@@@@@         _____   
	     _,g@@@@@@@@@@@@@@@@@@@@ M@@@@@@@@@@@@@@@g@@@@@g]%|  @@@@@@@@@@g
	  ,g@@@@@@@@@@@@@@@@@@@@@@@@  %@@@@@@@@@@@@@@%@@@@@@@_   @@@@@&P_@@h
	g@@@@@@@@@@@@@@@@@@@@@@@@@@@L  @@@@@@@@@@@@@" Y%@@@@@@_  @@@@#,@@@@ 
	@@@@@@@@@@@@@@@@@@@@C*@@@@@@&  %@@@@@@@@@@P     M@@@@@H. %@@[_@@@@$d
	@@@@@@@@@@@@@@@@@@@@h           @@@@@@@M*        @@@@@@   "%@@@@@@@@
	@@@@@@@@@@@@@@@@@@@WL        "  @@@@@@          '@@@@@@_     "M@@@@@
	@@@@@@@@@@@@@@@@@@@@[           M@@@@@           Y%@@@@@g    ,W`g@@@
	%@@@@@@@g@@@@@@@@@@@|            @@@@@            C@@@@@@@@+  ,@@@@@
	@@@@@@@@@@@@@@@@@@@@@            @@@@@             q@@@@@@@@@@@@@@@@
	@@@@@@@@@@@@@@@@@@@@@L        hg__@@@@W            @@@@@@Q@@@@["""""
	@@@@@@@@@@@@@"%M%%%@@@,       q@@@@@@@@           g@@@@@@@@@@@@@    
	@@@@@@@@@@@@h        "*h,     #""*M@@@@          d%**"    `  `      
	@@@%%P"@@@@@           _  q       Q@@@@@,__,.~r"                    
	@@@@gg_@@@@@ggggg[      [w.\,L_^  Q@@@@@p                           
	@@@@@@@@@@@@@@@@@&           ``   @@@@@@@                           
	%@@@@@@@@@@@@@@@@@___   _         @@@@@@@@          ~+--+---     __g
	@@@@@@@@@@@@@@@@@@@@N&M@@@@@@@@@@@@@@@@@@@@@@~@@g@@@g__,g@@g_#@@@@@@
	@@@@@@@@@@@@@@&@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ @@@@@@@@@@@@@@$]@@@@@@
	@M%@@@@@@@g@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@@@@@@@@@&@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@@@@@g@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

![Source Picture](./demo.jpg)
