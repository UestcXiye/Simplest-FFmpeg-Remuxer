// Simplest FFmpeg Remuxer.cpp : 定义控制台应用程序的入口点。
//


/*
* 最简单的基于FFmpeg的封装格式转换器
* Simplest FFmpeg Remuxer
*
* 源程序：
* 雷霄骅 Lei Xiaohua
* leixiaohua1020@126.com
* 中国传媒大学/数字电视技术
* Communication University of China / Digital TV Technology
* http://blog.csdn.net/leixiaohua1020
*
* 修改：
* 刘文晨 Liu Wenchen
* 812288728@qq.com
* 电子科技大学/电子信息
* University of Electronic Science and Technology of China / Electronic and Information Science
* https://blog.csdn.net/ProgramNovice
*
* 本程序实现了视频封装格式之间的转换。
* 需要注意的是本程序并不改变视音频的编码格式。
*
* This software converts a media file from one container format
* to another container format without encoding/decoding video files.
*/

#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>

// 解决报错：无法解析的外部符号 __imp__fprintf，该符号在函数 _ShowError 中被引用
#pragma comment(lib, "legacy_stdio_definitions.lib")
extern "C"
{
	// 解决报错：无法解析的外部符号 __imp____iob_func，该符号在函数 _ShowError 中被引用
	FILE __iob_func[3] = { *stdin, *stdout, *stderr };
}

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
// Windows
extern "C"
{
#include "libavformat/avformat.h"
};
#else
// Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#ifdef __cplusplus
};
#endif
#endif

int main(int argc, char* argv[])
{
	AVOutputFormat *ofmt = NULL;
	// 输入对应一个 AVFormatContext，输出对应一个 AVFormatContext
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
	AVPacket pkt;

	const char *in_filename = "cuc_ieschool.flv"; // 输入文件名
	const char *out_filename = "cuc_ieschool.mp4"; // 输出文件名

	int ret;
	int frame_index;
	int i;

	//if (argc < 3)
	//{
	//	printf("usage: %s input output\n"
	//		"Remux a media file with libavformat and libavcodec.\n"
	//		"The output format is guessed according to the file extension.\n"
	//		, argv[0]);
	//	return 1;
	//}

	av_register_all();

	// 输入
	ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0);
	if (ret < 0)
	{
		printf("Could not open input file.\n");
		goto end;
	}

	ret = avformat_find_stream_info(ifmt_ctx, 0);
	if (ret < 0)
	{
		printf("Failed to retrieve input stream information.\n");
		goto end;
	}

	// Print some input information
	av_dump_format(ifmt_ctx, 0, in_filename, 0);

	// 输出
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
	if (ofmt_ctx == NULL)
	{
		printf("Could not create output context.\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	ofmt = ofmt_ctx->oformat;

	for (i = 0; i < ifmt_ctx->nb_streams; i++)
	{
		// 根据输入流创建输出流
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
		if (out_stream == NULL)
		{
			printf("Failed allocating output stream.\n");
			ret = AVERROR_UNKNOWN;
			goto end;
		}

		// 拷贝输入视频码流的 AVCodecContex 的数值 t 到输出视频的 AVCodecContext
		ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
		if (ret < 0)
		{
			printf("Failed to copy context from input to output stream codec context.\n");
			goto end;
		}
		out_stream->codec->codec_tag = 0;
		if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
		{
			out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}
	}

	// Print some output information
	av_dump_format(ofmt_ctx, 0, out_filename, 1);

	// Open output file
	if (!(ofmt->flags & AVFMT_NOFILE))
	{
		ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0)
		{
			printf("Could not open output file '%s'.\n", out_filename);
			goto end;
		}
	}
	// Write file header
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0)
	{
		printf("Error occurred when opening output file.\n");
		goto end;
	}

	frame_index = 0;

	while (1)
	{
		AVStream *in_stream, *out_stream;
		// 获取一个 AVPacket
		ret = av_read_frame(ifmt_ctx, &pkt);
		if (ret < 0)
		{
			break;
		}

		in_stream = ifmt_ctx->streams[pkt.stream_index];
		out_stream = ofmt_ctx->streams[pkt.stream_index];
		// copy packet
		// 转换 PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;

		// 将 AVPacket 写入文件
		ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
		if (ret < 0)
		{
			printf("Error muxing packet.\n");
			break;
		}
		printf("Write %8d frames to output file.\n", frame_index);
		av_free_packet(&pkt);
		frame_index++;
	}

	// Write file trailer
	av_write_trailer(ofmt_ctx);


end:
	avformat_close_input(&ifmt_ctx);
	// close output
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
	{
		avio_close(ofmt_ctx->pb);
	}
	avformat_free_context(ofmt_ctx);
	if (ret < 0 && ret != AVERROR_EOF)
	{
		printf("Error occurred.\n");
		return -1;
	}

	system("pause");
	return 0;
}
