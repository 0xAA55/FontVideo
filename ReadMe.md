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

![Source Picture](./demo.jpg)

	gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg
	gggggggggggggggggggggggggggggg%%%Qgggggggggggggggggggggggggggg
	ggggggggggggggggggggggggQ%ggggQggggggg%g%ggggggggggggggggggggg
	gggggggggggggggggggggQgQgggggggg%gggggggQggggggggggggggggggggg
	ggggggggggggggggggggggQggggggggg%gggg%gggQQggggggggggggggggggg
	ggggggggggggggggggggggQggggggQgg%g%gggQggggggggggggggggggggggg
	ggggggggggggggggggQg%gQggggg%g%%ggggggg%gggQgggggggggggggggggg
	gggggggggggggggggggQggg%ggggQg%gggggggggggQg%ggggggggggggggggg
	gggggggggggggggggggQgggg%%ggggggggQ%QgggQg%gg%gggggggggggggggg
	gggggggggggggggggggQggggggQggggC___ggggg%%Qggg%ggggggggggggggg
	ggggggggggggggggggggggg%g__ggggggggggggQ%gQ%gQg%gggggggggggggg
	ggggggggggggggggggggg%QggQgggggggQgggggC%%gg%%%QQggggggggggggg
	gggggggggggggggggggggggg%g%Qgggggg#Qggg   g ']Q%gggggggggggggg
	gggggggggggggggggggggggggQmQg-'%%_ggggg   3gQQgggQgggggggggggg
	gggggggggggggggggggggggggggg%g  #M[M%gg   y%gggggQgggggggggggg
	gggggggggggggggggggggQggggggUQ_  5% Mgg _/Kggggggg%Qgggggggggg
	gggggggggggggggggggggggg%gggggg]_Q;/ ggWW'g%ggggQgg%gggggggggg
	gggggggggggggggggggggQggQM'   '{'-u%=QgA_gggQggQgg%gQggggggggg
	ggggggggggggggggggg%M'7M#        'sMgggggggg%ggggggggggggggggg
	gggggggggggggggggg%      W         Mgg%ggg%gg%gg%gWggggggggggg
	gggggggggggggggg#'       k          '%gQgQgg%MggQggQgggggggggg
	ggggggggggggggg%         MW           Mgg%ggggQg%gg%ggQggggggg
	ggggggggg#M%'''           g               K%#'Mg%gggg%%%%%%%%%
	ggggg%#%                  gy             mw_   ?ggggg%%Qg%%Qg%
	g#% '                     #g_            ]_+q    %ggg%%gQgg#%g
	                   _      ]gg          _ggggg'    %gggggg#gggg
	            /     gggggggggQgy     ___gggggg%Q    %gggg%%%%%%M
	[]       qm'      %ggggggggQgg     ggggggggggMCg   QQggggggg%%
	_K/     _w/       ggggggggggggy    gggggggggggg_YK 'ggQg%gg%gg
	K'?M#M?'          g%ggggggggggg    ]gggggggggggg 'y_%QgQ%%gggg
	               -  _Qggggggggg%gW   ]gggggggggggK  'Wg_'7MQgggg
	           _ggggggggQggggggg[;     Mgggggggggg%gggggg]]m;I_ggg
	g_____gC   ]%ggggggggg%%gggggggg    %ggggggg#Qgggggggggggggggg
	g%%ggg%    gggg%QggggggggQQQ%%gg     %gg%gggggggggggggggggggg%
	ggggg[     #gggg%gggggQggggQggg%     ?%%%%%%%%%%%%%%%%%%%%gggg
	%gggg#      ']%QM%%%gggggggggggE      gggggggggggggggggggggg%%
	ggggggg__gggg%Mg%Q%ggQ#gggggg%'      {%%%%g%%?%%[%%#g%%%%ggggg
	g%QggggggggMgQgggggggggggggg_      _gggggMM[_  aM'   ' ]#%g[gg
	gggggggg#%gggggggg#gggggggg#ggggggggggggg%%C'______ ____C]g]gg
	ggggg#%ggggggggg#ggggggggg%ggggggggQgggggggQggggggg[ggggggg]gg

