#!/bin/bash

pushd lib
if [ -e libavcodec.so.60    ]; then rm -f libavcodec.so.60   ; ln -s libavcodec.so.60.10.100    libavcodec.so.60   ; fi
if [ -e libavdevice.so.60   ]; then rm -f libavdevice.so.60  ; ln -s libavdevice.so.60.2.100    libavdevice.so.60  ; fi
if [ -e libavfilter.so.9    ]; then rm -f libavfilter.so.9   ; ln -s libavfilter.so.9.6.100     libavfilter.so.9   ; fi
if [ -e libavformat.so.60   ]; then rm -f libavformat.so.60  ; ln -s libavformat.so.60.5.100    libavformat.so.60  ; fi
if [ -e libavutil.so.58     ]; then rm -f libavutil.so.58    ; ln -s libavutil.so.58.6.100      libavutil.so.58    ; fi
if [ -e libswresample.so.4  ]; then rm -f libswresample.so.4 ; ln -s libswresample.so.4.11.100  libswresample.so.4 ; fi
if [ -e libswscale.so.7     ]; then rm -f libswscale.so.7    ; ln -s libswscale.so.7.2.100      libswscale.so.7    ; fi

if [ -e libavcodec.so    ]; then rm -f libavcodec.so   ; ln -s libavcodec.so.60    libavcodec.so   ; fi
if [ -e libavdevice.so   ]; then rm -f libavdevice.so  ; ln -s libavdevice.so.60   libavdevice.so  ; fi
if [ -e libavfilter.so   ]; then rm -f libavfilter.so  ; ln -s libavfilter.so.9    libavfilter.so  ; fi
if [ -e libavformat.so   ]; then rm -f libavformat.so  ; ln -s libavformat.so.60   libavformat.so  ; fi
if [ -e libavutil.so     ]; then rm -f libavutil.so    ; ln -s libavutil.so.58     libavutil.so    ; fi
if [ -e libswresample.so ]; then rm -f libswresample.so; ln -s libswresample.so.4  libswresample.so; fi
if [ -e libswscale.so    ]; then rm -f libswscale.so   ; ln -s libswscale.so.7     libswscale.so   ; fi
popd

make
