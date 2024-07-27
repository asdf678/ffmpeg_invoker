/*
 * Copyright (c) 2010 Nicolas George
 * Copyright (c) 2011 Stefano Sabatini
 * Copyright (c) 2012 Clément Bœsch
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file audio decoding and filtering usage example
 * @example decode_filter_audio.c
 *
 * Demux, decode and filter audio input file, generate a raw audio
 * file to be played with ffplay.
 */

// #include <unistd.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>

static const char *filter_descr = "aresample=8000,aformat=sample_fmts=s16:channel_layouts=mono";
static const char *player = "ffplay -f s16le -ar 8000 -ac 1 -";
// /home/tzy/source/ffmpeg-5.1.2/build/ffplay -f s16le -ar 44100 -ac 1 ./out.mp3
// .\decode_filter_mix_audio.exe .\test_mix0.aac .\test_mix1.aac test_mix.pcm
//  ffplay -f s16le -ar 8000 -ac 1 ./test_mix.pcm
static int open_input_file(const char *filename, AVFormatContext **fmt_ctx,
                           AVCodecContext **dec_ctx,
                           int *audio_stream_index)
{
    const AVCodec *dec;
    int ret;

    if ((ret = avformat_open_input(fmt_ctx, filename, NULL, NULL)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(*fmt_ctx, NULL)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    /* select the audio stream */
    ret = av_find_best_stream(*fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &dec, 0);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot find an audio stream in the input file\n");
        return ret;
    }
    *audio_stream_index = ret;

    /* create decoding context */
    *dec_ctx = avcodec_alloc_context3(dec);
    if (!(*dec_ctx))
        return AVERROR(ENOMEM);
    avcodec_parameters_to_context(*dec_ctx, (*fmt_ctx)->streams[*audio_stream_index]->codecpar);

    /* init the audio decoder */
    if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot open audio decoder\n");
        return ret;
    }

    AVStream *stream = (*fmt_ctx)->streams[*audio_stream_index];

    av_log(NULL, AV_LOG_INFO, "time:%lf\n", stream->duration * av_q2d(stream->time_base) * 1000);

    return 0;
}

static int init_volume_filters_sink(AVFilterContext **volume_buffersink_ctx,
                                    AVFilterGraph *filter_graph, double value)
{

    int ret = 0;
    const AVFilter *volume_abuffersink = avfilter_get_by_name("volume");
    char args[512];
    ret = snprintf(args, sizeof(args),
                   "volume=%lf", value);

    //    'volume=0.0

    ret = avfilter_graph_create_filter(volume_buffersink_ctx, volume_abuffersink, "ffvolume",
                                       args, NULL, filter_graph);
    return ret;
}
static int init_mix_filters_sink(AVFilterContext **mix_buffersink_ctx,
                                 AVFilterGraph *filter_graph)
{

    int ret = 0;
    const AVFilter *mix_abuffersink = avfilter_get_by_name("amix");
    char args[512];
    const char *duration = "longest";
    ret = snprintf(args, sizeof(args),
                   "inputs=%d:duration=%s:dropout_transition=%d", 2, duration, 0);

    //    inputs=2:duration=longest:dropout_transition=0

    ret = avfilter_graph_create_filter(mix_buffersink_ctx, mix_abuffersink, "ffamix",
                                       args, NULL, filter_graph);
    return ret;
}

static int init_pad_filters_sink(AVFilterContext **delay_buffersink_ctx,
                                 AVFilterGraph *filter_graph, int value)
{
    int ret = 0;
    const AVFilter *delay_abuffersink = avfilter_get_by_name("apad");
    char args[512];
    ret = snprintf(args, sizeof(args),
                   "whole_dur=%d", value);
    //    inputs=2:duration=longest:dropout_transition=0
    // adelay=delays=10s:all=1
    ret = avfilter_graph_create_filter(delay_buffersink_ctx, delay_abuffersink, "ffapad",
                                       args, NULL, filter_graph);
    return ret;
}

static int init_delay_filters_sink(AVFilterContext **delay_buffersink_ctx,
                                   AVFilterGraph *filter_graph, int value)
{

    int ret = 0;
    const AVFilter *delay_abuffersink = avfilter_get_by_name("adelay");
    char args[512];
    ret = snprintf(args, sizeof(args),
                   "delays=%d:all=1", value);
    //    inputs=2:duration=longest:dropout_transition=0
    // adelay=delays=10s:all=1
    ret = avfilter_graph_create_filter(delay_buffersink_ctx, delay_abuffersink, "ffadelay",
                                       args, NULL, filter_graph);
    return ret;
}

static int init_filters_sink(AVFilterContext **buffersink_ctx,
                             AVFilterGraph *filter_graph)
{
    int ret = 0;

    const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
    // AVFilterInOut *outputs = avfilter_inout_alloc();
    // AVFilterInOut *inputs = avfilter_inout_alloc();
    static const enum AVSampleFormat out_sample_fmts[] = {AV_SAMPLE_FMT_S16, -1};
    static const int out_sample_rates[] = {44100, -1};
    const AVFilterLink *outlink;

    if (!filter_graph)
    {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* buffer audio sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(buffersink_ctx, abuffersink, "out",
                                       NULL, NULL, filter_graph);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
        goto end;
    }

    ret = av_opt_set_int_list(*buffersink_ctx, "sample_fmts", out_sample_fmts, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
        goto end;
    }

    ret = av_opt_set(*buffersink_ctx, "ch_layouts", "mono",
                     AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
        goto end;
    }

    ret = av_opt_set_int_list(*buffersink_ctx, "sample_rates", out_sample_rates, -1,
                              AV_OPT_SEARCH_CHILDREN);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
        goto end;
    }

    //     /*
    //      * Set the endpoints for the filter graph. The filter_graph will
    //      * be linked to the graph described by filters_descr.
    //      */

    //     /*
    //      * The buffer source output must be connected to the input pad of
    //      * the first filter described by filters_descr; since the first
    //      * filter input label is not specified, it is set to "in" by
    //      * default.
    //      */
    //     outputs->name = av_strdup("in");
    //     outputs->filter_ctx = *buffersrc_ctx;
    //     outputs->pad_idx = 0;
    //     outputs->next = NULL;

    //     /*
    //      * The buffer sink input must be connected to the output pad of
    //      * the last filter described by filters_descr; since the last
    //      * filter output label is not specified, it is set to "out" by
    //      * default.
    //      */
    //     inputs->name = av_strdup("out");
    //     inputs->filter_ctx = *buffersink_ctx;
    //     inputs->pad_idx = 0;
    //     inputs->next = NULL;

    //     if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
    //                                         &inputs, &outputs, NULL)) < 0)
    //         goto end;

    //     if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
    //         goto end;

    //     /* Print summary of the sink buffer
    //      * Note: args buffer is reused to store channel layout string */
    //     outlink = (*buffersink_ctx)->inputs[0];
    //     av_channel_layout_describe(&outlink->ch_layout, args, sizeof(args));
    //     av_log(NULL, AV_LOG_INFO, "Output: srate:%dHz fmt:%s chlayout:%s\n",
    //            (int)outlink->sample_rate,
    //            (char *)av_x_if_null(av_get_sample_fmt_name(outlink->format), "?"),
    //            args);

end:
    //     avfilter_inout_free(&inputs);
    //     avfilter_inout_free(&outputs);
    return ret;
}

static int init_filters(const char *filters_descr, AVFormatContext *fmt_ctx, int audio_stream_index, AVCodecContext *dec_ctx, AVFilterContext **buffersrc_ctx, AVFilterGraph *filter_graph)
{
    char args[512];
    int ret = 0;
    const AVFilter *abuffersrc = avfilter_get_by_name("abuffer");

    AVRational time_base = fmt_ctx->streams[audio_stream_index]->time_base;

    /* buffer audio source: the decoded frames from the decoder will be inserted here. */
    if (dec_ctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
        av_channel_layout_default(&dec_ctx->ch_layout, dec_ctx->ch_layout.nb_channels);
    ret = snprintf(args, sizeof(args),
                   "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=",
                   time_base.num, time_base.den, dec_ctx->sample_rate,
                   av_get_sample_fmt_name(dec_ctx->sample_fmt));
    av_channel_layout_describe(&dec_ctx->ch_layout, args + ret, sizeof(args) - ret);
    ret = avfilter_graph_create_filter(buffersrc_ctx, abuffersrc, "in",
                                       args, NULL, filter_graph);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        goto end;
    }
end:

    return ret;
}

static void print_frame(const AVFrame *frame, FILE *outfile)
{
    const int n = frame->nb_samples * frame->ch_layout.nb_channels;
    const uint16_t *p = (uint16_t *)frame->data[0];
    const uint16_t *p_end = p + n;

    while (p < p_end)
    {
        // fputc(*p & 0xff, stdout);
        // fputc(*p >> 8 & 0xff, stdout);
        fwrite(p, 2, 1, outfile);
        p++;
    }
    // fflush(stdout);

    // int data_size = av_get_bytes_per_sample(dec_ctx->sample_fmt);
    // if (data_size < 0)
    // {
    //     /* This should not occur, checking just for paranoia */
    //     fprintf(stderr, "Failed to calculate data size\n");
    //     exit(1);
    // }
    // for (int i = 0; i < frame->nb_samples; i++)
    //     for (int ch = 0; ch < dec_ctx->ch_layout.nb_channels; ch++)
    //         fwrite(frame->data[ch] + data_size * i, 1, data_size, outfile);
}

// int gen_silence_frame(AVFrame **in_frame)
// {
//     int ret;
//     if (!silence_frame)
//     {
//         return NULL;
//     }

//     silence_frame->sample_rate = DEFAULT_SR;
//     silence_frame->format = DEFAULT_SF;
//     silence_frame->channel_layout =
//         av_get_default_channel_layout(DEFAULT_CHANNEL);
//     silence_frame->channels = DEFAULT_CHANNEL;
//     silence_frame->nb_samples =
//         SILENCE_BUFF; // 默认一帧数据大小为1024，可通过输入流里的一帧frame.nb_samples查看

//     ret = av_frame_get_buffer(silence_frame, 0);

//     if (ret < 0)
//     {
//         av_frame_unref(silence_frame);
//         E_LOG("Failed to create silence frame!");
//         *in_frame = NULL;
//         return -1;
//     }
//     av_samples_set_silence(silence_frame->data, 0, silence_frame->nb_samples,
//                            silence_frame->channels,
//                            (enum AVSampleFormat)silence_frame->format);

//     *in_frame = silence_frame;

//     return 0;
// }

int generate_silence_frame(AVFrame *frame, AVCodecContext *dec_ctx)
{

    int nb_samples = 1024;                         // 静音帧的样本数
    int channels = dec_ctx->ch_layout.nb_channels; // 音频帧的通道数
    int sample_rate = dec_ctx->sample_rate;        // 音频帧的采样率
    // int format = AV_SAMPLE_FMT_FLT;               // 静音帧的样本格式

    av_channel_layout_copy(&frame->ch_layout, &dec_ctx->ch_layout);

    frame->nb_samples = nb_samples;
    frame->format = dec_ctx->sample_fmt;
    // frame->ch_layout = dec_ctx->ch_layout;
    frame->sample_rate = sample_rate;

    int ret = av_frame_get_buffer(frame, 0);

    if (ret < 0)
    {

        return ret;
    }

    ret = av_samples_set_silence(frame->data, 0, frame->nb_samples,
                                 frame->ch_layout.nb_channels,
                                 (enum AVSampleFormat)frame->format);

    return ret;
}

int main(int argc, char **argv)
{
    // int argc = 4;
    // char *argv[4] = {"decode_filter_audio.exe", ".\\test_mix1.aac", ".\\test_mix0.aac", "test_mix.pcm"};

    int ret;

    AVFilterGraph *filter_graph = NULL;
    AVFilterContext *buffersink_ctx = NULL;

    AVFilterContext *volume_buffersink_ctx = NULL;
    AVFilterContext *volume_buffersink_ctx1 = NULL;

    AVFilterContext *delay_buffersink_ctx = NULL;
    AVFilterContext *delay_buffersink_ctx1 = NULL;

    AVFilterContext *pad_buffersink_ctx = NULL;

    AVFilterContext *mix_buffersink_ctx = NULL;

    AVFilterContext *buffersrc_ctx = NULL;
    // AVFilterGraph *filter_graph;

    // AVFilterContext *buffersink_ctx1 = NULL;
    AVFilterContext *buffersrc_ctx1 = NULL;
    // AVFilterGraph *filter_graph1;

    AVFrame *filt_frame = av_frame_alloc();

    AVFrame *resend_frame = av_frame_alloc();

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *dec_ctx;

    int audio_stream_index = -1;

    AVPacket *packet1 = av_packet_alloc();
    AVFrame *frame1 = av_frame_alloc();
    AVFormatContext *fmt_ctx1 = NULL;
    AVCodecContext *dec_ctx1;

    int audio_stream_index1 = -1;

    FILE *outfile;

    const char *outfilename;
    if (!packet || !frame || !filt_frame || !packet1 || !frame1)
    {
        fprintf(stderr, "Could not allocate frame or packet\n");
        exit(1);
    }

    if (argc != 4)
    {
        argc = 4;

        char *argvx[4] = {"./decode_filter_mix_audio", "./2.m4a", "./6.m4a", "out2.pcm"};
        argv = argvx;
    }

    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s file | %s\n", argv[0], player);
        exit(1);
    }

    outfilename = argv[3];

    if ((ret = open_input_file(argv[1], &fmt_ctx, &dec_ctx, &audio_stream_index)) < 0)
        goto end;

    av_dump_format(fmt_ctx, 0, argv[1], 0);

    {
        int64_t seek_pos = av_rescale(4000, AV_TIME_BASE, 1000);

        int64_t seek_target = seek_pos;
        int64_t seek_min = INT64_MIN;
        int64_t seek_max = INT64_MAX;

        ret = avformat_seek_file(fmt_ctx, -1, seek_min, seek_target,
                                 seek_max,
                                 0);
        av_log(NULL, AV_LOG_INFO, "avformat_seek_file ret:%d\n", ret);
    }

    if ((ret = open_input_file(argv[2], &fmt_ctx1, &dec_ctx1, &audio_stream_index1)) < 0)
        goto end;
    av_dump_format(fmt_ctx1, 0, argv[2], 0);
    {
        int64_t seek_pos = av_rescale(4000, AV_TIME_BASE, 1000);

        int64_t seek_target = seek_pos;
        int64_t seek_min = INT64_MIN;
        int64_t seek_max = INT64_MAX;

        ret = avformat_seek_file(fmt_ctx1, -1, seek_min, seek_target,
                                 seek_max,
                                 0);
        av_log(NULL, AV_LOG_INFO, "avformat_seek_file ret:%d\n", ret);
    }

    // int64_t seek_pos = av_rescale(4000, AV_TIME_BASE, 1000);

    // int64_t seek_target = seek_pos;
    // int64_t seek_min = INT64_MIN;
    // int64_t seek_max = INT64_MAX;

    // ret = avformat_seek_file(fmt_ctx, -1, seek_min, seek_target,
    //                          seek_max,
    //                          0);
    // av_log(NULL, AV_LOG_INFO, "avformat_seek_file ret:%d\n", ret);

#if defined(_WIN32)
    fopen_s(&outfile, outfilename, "wb");
#else
    outfile = fopen(outfilename, "wb");
#endif

    if (!outfile)
    {
        goto end;
    }
    filter_graph = avfilter_graph_alloc();

    if ((ret = init_filters(filter_descr, fmt_ctx, audio_stream_index, dec_ctx, &buffersrc_ctx, filter_graph)) < 0)
        goto end;
    if ((ret = init_filters(filter_descr, fmt_ctx1, audio_stream_index1, dec_ctx1, &buffersrc_ctx1, filter_graph)) < 0)
        goto end;

    if ((ret = init_pad_filters_sink(&pad_buffersink_ctx, filter_graph, 5)) < 0)
        goto end;

    if ((ret = init_delay_filters_sink(&delay_buffersink_ctx, filter_graph, 0)) < 0)
        goto end;
    if ((ret = init_delay_filters_sink(&delay_buffersink_ctx1, filter_graph, 0)) < 0)
        goto end;
    if ((ret = init_volume_filters_sink(&volume_buffersink_ctx, filter_graph, 0.5)) < 0)
        goto end;
    if ((ret = init_volume_filters_sink(&volume_buffersink_ctx1, filter_graph, 1.0)) < 0)
        goto end;

    if ((ret = init_mix_filters_sink(&mix_buffersink_ctx, filter_graph)) < 0)
        goto end;
    if ((ret = init_filters_sink(&buffersink_ctx, filter_graph)) < 0)
        goto end;
    ret = generate_silence_frame(resend_frame, dec_ctx);
    if (ret < 0)
    {
        goto end;
    }

    // ret = avfilter_link(src1FilterCtx, 0, mixFilterCtx, 0);
    // ret = avfilter_link(src2FilterCtx, 0, mixFilterCtx, 1);
    // ret = avfilter_link(mixFilterCtx, 0, sinkFilterCtx, 0);

    // if ((ret = avfilter_link(buffersrc_ctx, 0, mix_buffersink_ctx, 0)) < 0)
    //     goto end;
    // if ((ret = avfilter_link(buffersrc_ctx1, 0, mix_buffersink_ctx, 1)) < 0)
    //     goto end;
    // if ((ret = avfilter_link(buffersrc_ctx, 0, delay_buffersink_ctx, 0)) < 0)
    //     goto end;
    // if ((ret = avfilter_link(delay_buffersink_ctx, 0, buffersink_ctx, 0)) < 0)
    //     goto end;
    if ((ret = avfilter_link(buffersrc_ctx, 0, delay_buffersink_ctx, 0)) < 0)
        goto end;
    if ((ret = avfilter_link(delay_buffersink_ctx, 0, volume_buffersink_ctx, 0)) < 0)
        goto end;
    if ((ret = avfilter_link(volume_buffersink_ctx, 0, pad_buffersink_ctx, 0)) < 0)
        goto end;
    if ((ret = avfilter_link(pad_buffersink_ctx, 0, mix_buffersink_ctx, 0)) < 0)
        goto end;
    // if ((ret = avfilter_link(volume_buffersink_ctx, 0, mix_buffersink_ctx, 0)) < 0)
    //     goto end;

    if ((ret = avfilter_link(buffersrc_ctx1, 0, delay_buffersink_ctx1, 0)) < 0)
        goto end;
    if ((ret = avfilter_link(delay_buffersink_ctx1, 0, volume_buffersink_ctx1, 0)) < 0)
        goto end;
    if ((ret = avfilter_link(volume_buffersink_ctx1, 0, mix_buffersink_ctx, 1)) < 0)
        goto end;
    // if ((ret = avfilter_link(volume_buffersink_ctx1, 0, mix_buffersink_ctx, 1)) < 0)
    //     goto end;

    if ((ret = avfilter_link(mix_buffersink_ctx, 0, buffersink_ctx, 0)) < 0)
        goto end;

    ret = avfilter_graph_config(filter_graph, NULL);

    /* read all packets */
    av_log(NULL, AV_LOG_INFO, "start:\n%s", avfilter_graph_dump(filter_graph, NULL));

    int nb_finished = 0;

    int input_finished = 0;
    int input_finished1 = 0;

    int input_to_read = 1;
    int input_to_read1 = 1;

    while (nb_finished < 2)
    {
        int data_present_in_graph = 0;

        if (input_finished || input_to_read == 0)
        {
        }
        else
        {
            input_to_read = 0;
            int finished = 0;
            int data_present = 0;
            if ((ret = av_read_frame(fmt_ctx, packet)) < 0)
            {
                if (ret == AVERROR_EOF)
                {
                    // av_log(NULL, AV_LOG_INFO, "0 av_read_frame ret AVERROR_EOF\n");
                    finished = 1;
                    // break;
                }
                else
                {
                    // av_log(NULL, AV_LOG_INFO, "0 av_read_frame failed,break\n");
                    break;
                }
            }
            else
            {
                // av_log(NULL, AV_LOG_INFO, "0 av_read_frame success\n");
            }

            if (packet->stream_index == audio_stream_index)
            {
                ret = avcodec_send_packet(dec_ctx, packet);
                if (ret < 0)
                {
                    av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
                    break;
                }
                else
                {
                    // av_log(NULL, AV_LOG_INFO, "0 avcodec_send_packet success\n");
                }

                while (ret >= 0)
                {
                    ret = avcodec_receive_frame(dec_ctx, frame);
                    if (ret == AVERROR(EAGAIN))
                    {
                        // av_log(NULL, AV_LOG_INFO, "0 avcodec_receive_frame ret AVERROR(EAGAIN)\n");
                        break;
                    }
                    else if (ret == AVERROR_EOF)
                    {
                        finished = 1;
                        ++nb_finished;
                        av_log(NULL, AV_LOG_INFO, "AudiosDecoder display pts index:%d,pts:%s\n",
                               0, "eof");
                        // av_log(NULL, AV_LOG_INFO, "0 avcodec_receive_frame ret AVERROR_EOF\n");
                    }
                    else if (ret < 0)
                    {
                        av_log(NULL, AV_LOG_ERROR, "Error while receiving a frame from the decoder\n");
                        goto end;
                    }
                    else
                    {
                        data_present = 1;
                        av_log(NULL, AV_LOG_INFO, "AudiosDecoder display pts index:%d,pts:%ld\n",
                               0, frame->pts);
                        // av_log(NULL, AV_LOG_INFO, "0 avcodec_receive_frame ret success\n");
                    }

                    // av_log(NULL, AV_LOG_INFO, "0 avcodec_receive_frame finished:%d,data_present:%d\n", finished, data_present);

                    // print_frame(frame, outfile);
                    if (finished && !data_present)
                    {
                        input_finished = 1;
                        /* push the audio data from decoded frame into the filtergraph */
                        if (av_buffersrc_add_frame_flags(buffersrc_ctx, NULL, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
                        {
                            av_log(NULL, AV_LOG_ERROR, "Error while feeding the audio filtergraph\n");
                            break;
                        }
                    }
                    else if (data_present)
                    {
                        /* push the audio data from decoded frame into the filtergraph */
                        if (av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
                        {
                            av_log(NULL, AV_LOG_ERROR, "Error while feeding the audio filtergraph\n");
                            break;
                        }

                        // if (av_buffersrc_add_frame_flags(buffersrc_ctx, resend_frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
                        // {
                        //     av_log(NULL, AV_LOG_ERROR, "Error while feeding the audio filtergraph\n");
                        //     break;
                        // }
                    }

                    if (ret >= 0)
                    {

                        av_frame_unref(frame);
                    }
                }
            }
            data_present_in_graph |= data_present;
            av_packet_unref(packet);
        }
        ///----------------------------------------------
        if (input_finished1 || input_to_read1 == 0)
        {
        }
        else
        {
            input_to_read1 = 0;

            int finished = 0;
            int data_present = 0;
            if ((ret = av_read_frame(fmt_ctx1, packet1)) < 0)
            {
                if (ret == AVERROR_EOF)
                {
                    // av_log(NULL, AV_LOG_INFO, "1 av_read_frame ret AVERROR_EOF\n");
                    finished = 1;
                    // break;
                }
                else
                {
                    // av_log(NULL, AV_LOG_INFO, "1 av_read_frame failed,break\n");
                    break;
                }
            }
            else
            {
                // av_log(NULL, AV_LOG_INFO, "1 av_read_frame success\n");
            }

            if (packet1->stream_index == audio_stream_index1)
            {
                ret = avcodec_send_packet(dec_ctx1, packet1);
                if (ret < 0)
                {
                    av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
                    break;
                }
                else
                {
                    // av_log(NULL, AV_LOG_INFO, "1 avcodec_send_packet success\n");
                }

                while (ret >= 0)
                {
                    ret = avcodec_receive_frame(dec_ctx1, frame1);
                    if (ret == AVERROR(EAGAIN))
                    {
                        // av_log(NULL, AV_LOG_INFO, "1 avcodec_receive_frame ret AVERROR(EAGAIN)\n");
                        break;
                    }
                    else if (ret == AVERROR_EOF)
                    {
                        finished = 1;
                        ++nb_finished;
                        av_log(NULL, AV_LOG_INFO, "AudiosDecoder display pts index:%d,pts:%s\n",
                               1, "eof");
                        // av_log(NULL, AV_LOG_INFO, "1 avcodec_receive_frame ret AVERROR_EOF\n");
                    }
                    else if (ret < 0)
                    {
                        av_log(NULL, AV_LOG_ERROR, "Error while receiving a frame from the decoder\n");
                        goto end;
                    }
                    else
                    {
                        data_present = 1;
                        av_log(NULL, AV_LOG_INFO, "AudiosDecoder display pts index:%d,pts:%ld\n",
                               1, frame->pts);
                        // av_log(NULL, AV_LOG_INFO, "1 avcodec_receive_frame ret success\n");
                    }

                    // av_log(NULL, AV_LOG_INFO, "1 avcodec_receive_frame finished:%d,data_present:%d\n", finished, data_present);

                    // print_frame(frame, outfile);
                    if (finished && !data_present)
                    {
                        input_finished1 = 1;

                        /* push the audio data from decoded frame into the filtergraph */
                        if (av_buffersrc_add_frame_flags(buffersrc_ctx1, NULL, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
                        {
                            av_log(NULL, AV_LOG_ERROR, "Error while feeding the audio filtergraph\n");
                            break;
                        }
                    }
                    else if (data_present)
                    {
                        /* push the audio data from decoded frame into the filtergraph */
                        if (av_buffersrc_add_frame_flags(buffersrc_ctx1, frame1, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
                        {
                            av_log(NULL, AV_LOG_ERROR, "Error while feeding the audio filtergraph\n");
                            break;
                        }
                    }

                    if (ret >= 0)
                    {

                        av_frame_unref(frame);
                    }
                }
            }
            data_present_in_graph |= data_present;
            av_packet_unref(packet);
        }
        if (data_present_in_graph)
        { /* pull filtered audio from the filtergraph */
            while (1)
            {
                ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    if (av_buffersrc_get_nb_failed_requests(buffersrc_ctx) > 0)
                    {
                        input_to_read = 1;
                        // av_log(NULL, AV_LOG_INFO, "0 av_buffersrc_get_nb_failed_requests >0\n");
                    }

                    // if (av_buffersrc_get_nb_failed_requests(buffersrc_ctx1) > 0)
                    // {
                    //     input_to_read1 = 1;
                    //     // av_log(NULL, AV_LOG_INFO, "1 av_buffersrc_get_nb_failed_requests >0\n");
                    // }
                    break;
                }
                else
                {
                    AVRational tb = av_buffersink_get_time_base(buffersink_ctx);
                    double pts = (filt_frame->pts == AV_NOPTS_VALUE) ? NAN : filt_frame->pts * av_q2d(tb);
                    av_log(NULL, AV_LOG_INFO, "AudiosDecoder display pts,offset:%ld,pts:%ld,fpts:%lf\n", filt_frame->best_effort_timestamp,
                           filt_frame->pts, pts);
                }
                if (ret < 0)
                    goto end;
                print_frame(filt_frame, outfile);
                av_frame_unref(filt_frame);
            }
        }
        else
        {
            input_to_read = 1;
            input_to_read1 = 1;
        }
    }
end:
    fclose(outfile);

    // avfilter_graph_free(&filter_graph1);
    avcodec_free_context(&dec_ctx1);
    avformat_close_input(&fmt_ctx1);
    av_packet_free(&packet1);
    av_frame_free(&frame1);

    avfilter_graph_free(&filter_graph);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_packet_free(&packet);
    av_frame_free(&frame);
    av_frame_free(&filt_frame);

    if (ret < 0 && ret != AVERROR_EOF)
    {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        exit(1);
    }

    exit(0);
}
