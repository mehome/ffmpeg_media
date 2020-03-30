﻿#pragma once

#include "media.h"

#define QmBaseTimeToSecondTime(value, base) (value * double(base.num) )/(base.den)
#define QmSecondTimeToBaseTime(value, base) (int64_t)((value * double(base.den) )/(base.num))
#define QmBaseTimeToMSTime(value, base) int(1000 * (value * double(base.num) )/(base.den))
#define QmMSTimeToBaseTime(value, base) (int64_t)((value * double(base.den) )/(base.num * 1000))

struct AVRational;

class FFmpegUtils
{
public:
    static void init();
	static const AVRational* contextBaseTime();
    static int fourccToFFmpegFormat(int);
    static int ffmpegFormatToFourcc(int);
	static AVPacketPtr allocAVPacket();
	static int currentMilliSecsSinceEpoch();
};
 
