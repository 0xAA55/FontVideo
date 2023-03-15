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
		oext = 'mp4'
		ofile = f'{iname}_o.{oext}'
	oname = ofile.rsplit('.', 1)[0]
	if os.path.exists(ofile): os.remove(ofile)
	print(f'Output file is `{ofile}`')

	if os.name == 'nt':
		fexec = 'FontVideo.exe'
	elif os.path.exists('FontVideo.exe'):
		fexec = './FontVideo.exe'
	elif os.path.exists('fontvideo'):
		fexec = './fontvideo'
	else:
		fexec = 'fontvideo'

	vfile = f'{iname}_a.avi'
	if os.path.exists(vfile): os.remove(vfile)
	xfile = f'{oname}_t.txt'
	subprocess.run([fexec,
		'-i', ifile,
		'-b',
		'-m',
		'-h', '70',
		'--assets-meta', os.path.join('assets','meta_gb2312_16.ini'),
		'--output-avi-file', vfile,
		'-o', xfile])
	subprocess.run(['ffmpeg',
		'-i', vfile,
		'-c:v', 'libx264',
		'-c:a', 'aac',
		'-shortest',
		ofile])

	os.remove(vfile)
	os.remove(xfile)


