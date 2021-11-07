# FontVideo

Play video by fonts in a console window by composing characters.

Using FFmpeg API to decode the input file, then the video stream is rendered to the specified output, and the audio stream is played via `libsoundio`.

Default is output to `stdout`, but you can specify its output to a file.

## Usage:

	FontVideo.exe -i <input> [-o output.txt] [-v] [-p seconds] [-m] [-w width] [-h height] [-s width height] [-S from_sec] [-b] [--no-opengl] [--assets-meta metafile.ini]
	Or: FontVideo.exe <input>
        -i: Specify the input video file name.
        -o: [Optional] Specify the output text file name.
        -v: Verbose mode, output debug informations.
        -p: [Optional] Specify pre-render seconds, Longer value results longer delay but better quality.
        -m: Mute sound output.
        -w: [Optional] Width of the output.
        -h: [Optional] Height of the output.
        -s: [Optional] Size of the output, default is to detect the size of the console window, or 80x25 if failed.
        -b: Only do white-black output.
        -S: [Optional] Set the playback start time of seconds.
        --no-opengl: [Optional] Do not use OpenGL to accelerate rendering.
        --assets-meta: [Optional] Use specified meta file, default is to use 'assets\meta.ini'.
        --log: [Optional] Specify the log file.

## Demo

	                                                    
	                                                    
	                                                    
	                                                    
	                                                    
	                                                    
	                                                    
	                                 '                  
	                             _                      
	                          m#%%                      
	                    M%M                             
	                  '              g_                 
	                             y   ggC%gw_            
	                        Ymw %    ggg'7/             
	                       { Qg;ww_  %%7 '   '          
	                         ]gQgmg  g7''               
	                        ! OQUQg  _W#                
	                    !_ggggQQg_;  '#                 
	                 ggg_ggggggQgkAk '   '              
	               _gggggYggggQgg%            [         
	              ggggggg ggggg%ggg_                    
	             gggggggg[%ggggg%gg%y _   '             
	        _gggggggggg%g]'gggggQggggWgQ__g             
	    __gggggggggggggggg Qgggggggggg%_%ggg     mW    q
	 _gggg%%gggQgggggggggg  gggggggggg#%QWggg_  ]gyA''-M
	ggggggggggggggggQggggQ  ]gggggggg#   'gggg  ?%      
	gggggggggQQgggg   ''''   gggg%%#'     %gggC  ' g_gg}
	%QggggggCYggggQ          %ggg[        g-%Qg    'M#gg
	qggggg%#[QggQgQ        [ 3ggg[         %gQgy        
	]ggggggg%ggQ%gQ        !  ggg}          ]gMg_       
	ggg%%%%QgggQggg9k      !  ]gg]          ggg_Mggc  ''
	ggggggggggMMMM'         %%%ggg         /   'Mw[#g7  
	MM%Q%Mggg[              '''gggy                     
	M;    ggg                  gggQ                     
	   7mggggE                 Q%ggk                    
	     %ggg%gmw             ]ggggg                   _
	    ;MM#%% _w'/YY'_7''7YMg%ggggkmmw wgggg    gg M]  
	 ;-'     ;K   '''y'    MgggggQ''   ;Umgggggggg#_ q  
	'      w'       '      y'' '' /     qWQ%#%%%%#Y%WM  
	     Y        /       /      y      /  ''''['''  M  

![Source Picture](./demo.jpg)
