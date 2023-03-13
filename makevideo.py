#!/usr/bin/env python3
# -*- coding: utf-8 -*
import os
import sys
import glob
import subprocess

def usage():
	print(f'Usage: {os.path.basename(sys.argv[0])} <inputfile.mp4> [output.mp4]')
	print('You need FFmpeg installed in your PATH')
	exit(1)

if __name__ == '__main__':
	try:
		ifile = sys.argv[1]
	except IndexError:
		usage()

	try:
		ofile = sys.argv[2]
	except IndexError:
		try:
			iname, iext = ifile.rsplit('.', 1)
		except ValueError:
			iname = ifile
			iext = 'mp4'
		ofile = f'{iname}_o.{iext}'
		if os.path.exists(ofile): ofile = 'output.mp4'
		oname = ofile.rsplit('.', 1)[0]
	print(f'Output file is `{ofile}`')

	if not os.path.exists('temp'):
		print('Make subdirectory `temp`')
		os.makedirs('temp')

	if os.name == 'nt':
		fexec = 'FontVideo.exe'
	elif os.path.exists('FontVideo.exe'):
		fexec = './FontVideo.exe'
	elif os.path.exists('fontvideo'):
		fexec = './fontvideo'
	else:
		fexec = 'fontvideo'

	tfile = os.path.join('temp', f'{iname}_t.m4v')
	afile = os.path.join('temp', f'{iname}_a.m4a')
	if os.path.exists(tfile): os.remove(tfile)
	if os.path.exists(afile): os.remove(afile)
	xfile = f'{oname}_t.txt'
	targs = ['ffmpeg', '-i', ifile, '-filter:v', 'fps=30', '-an', '-shortest', tfile]
	aargs = ['ffmpeg', '-i', ifile, '-c:a', 'aac', '-vn', '-shortest', afile]
	fargs = [fexec,
		'-i', tfile,
		'-b',
		'-m',
		'-h', '70',
		'--assets-meta', os.path.join('assets','meta_gb2312_16.ini'),
		'--no-avoid-repetition',
		'--output-frame-image-sequence', os.path.join('temp', 'f_'),
		'-o', xfile]
	tproc = subprocess.Popen(targs)
	aproc = subprocess.Popen(aargs)
	tproc.wait()
	fproc = subprocess.Popen(fargs)
	aproc.wait()
	fproc.wait()

	jargs = ['ffmpeg',
		'-framerate', '30',
		'-i', os.path.join('temp', 'f_%08d.bmp')]
	if os.path.exists(afile): jargs += ['-i', afile]
	jargs += ['-c:v', 'libx264',
		'-filter:v', 'fps=30',
		'-c:a', 'copy',
		'-shortest',
		ofile]
	subprocess.run(jargs)
	for df in glob.glob(os.path.join('temp', 'f_*.bmp')):
		try:
			os.remove(df)
		except Exception as e:
			print(f'Failed to delete `{df}`: {str(e)}')
	os.remove(tfile)
	os.remove(afile)
	os.rmdir('temp')


