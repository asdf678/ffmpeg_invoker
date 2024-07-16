#include "common.h"
#include <assert.h>
int avpro::CommonMedia::open_input(std::string_view url)
{
    int ret;

    if ((ret = avformat_open_input(&fmt_ctx, url.data(), NULL, NULL)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }
    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    return 0;
}

std::unique_ptr<avpro::CommonMediaContext> avpro::CommonMedia::find_stream(AVMediaType type)
{
    const AVCodec *dec;
    int ret;
    /* select the audio stream */
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, &dec, 0);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot find an audio stream in the input file\n");
        return nullptr;
    }
    auto context = new avpro::CommonMediaContext;
    context->stream_index = ret;
    context->stream = fmt_ctx->streams[ret];
    context->dec = dec;
    return std::unique_ptr<avpro::CommonMediaContext>(context);
}

int avpro::CommonMedia::open_codec(std::unique_ptr<avpro::CommonMediaContext> &context)
{
    int ret;
    context->dec_ctx = avcodec_alloc_context3(context->dec);
    if (!context->dec)
        return AVERROR(ENOMEM);
    avcodec_parameters_to_context(context->dec_ctx, context->stream->codecpar);

    /* init the audio decoder */
    if ((ret = avcodec_open2(context->dec_ctx, context->dec, NULL)) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot open audio decoder\n");
        return ret;
    }

    return 0;
}

avpro::CommonMediaContext::~CommonMediaContext()
{
    avcodec_free_context(&dec_ctx);
}

// int avpro::CommonMedia::find_best_stream_and_open_codec(AVMediaType type)
// {

//     const AVCodec *dec;

//     audio_stream_index = find_best_stream(type, dec);

//     /* create decoding context */
//     dec_ctx = avcodec_alloc_context3(dec);
//     if (!dec_ctx)
//         return AVERROR(ENOMEM);
//     avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[audio_stream_index]->codecpar);

//     /* init the audio decoder */
//     if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0)
//     {
//         av_log(NULL, AV_LOG_ERROR, "Cannot open audio decoder\n");
//         return ret;
//     }

//     return 0;
// }

int avpro::CommonFilterContext::filter(const AVFrame *frame, std::function<void(const AVFrame *)> filter_callback) const
{
    int ret = av_buffersrc_add_frame_flags(buffersrc_ctx, const_cast<AVFrame *>(frame),
                                           AV_BUFFERSRC_FLAG_KEEP_REF);
    /* push the audio data from decoded frame into the filtergraph */
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Error while feeding the audio filtergraph\n");
        return -1;
    }

    /* pull filtered audio from the filtergraph */
    while (1)
    {
        ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;
        if (ret < 0)
            return -1;
        filter_callback(filt_frame);
        av_frame_unref(filt_frame);
    }
    return 1;
}
avpro::CommonFilterContext::~CommonFilterContext()
{
    av_frame_free(&filt_frame);
    avfilter_graph_free(&filter_graph);
}

int avpro::CommonMedia::init_audio_filters(const std::string_view filters_descr, std::unique_ptr<CommonMediaContext> &context)
{
    AVCodecContext *dec_ctx = context->dec_ctx;

    char args[512];
    int ret = 0;
    const AVFilter *abuffersrc = avfilter_get_by_name("abuffer");
    const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();
    static const enum AVSampleFormat out_sample_fmts[] = {AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_NONE};
    static const int out_sample_rates[] = {8000, -1};
    const AVFilterLink *outlink;
    AVRational time_base = context->stream->time_base;
    AVFrame *filt_frame;

    std::unique_ptr<avpro::CommonFilterContext> filter_context = std::make_unique<avpro::CommonFilterContext>();

    filter_context->filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_context->filter_graph)
    {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* buffer audio source: the decoded frames from the decoder will be inserted here. */
    if (dec_ctx->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
        av_channel_layout_default(&dec_ctx->ch_layout, dec_ctx->ch_layout.nb_channels);
    ret = snprintf(args, sizeof(args),
                   "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=",
                   time_base.num, time_base.den, dec_ctx->sample_rate,
                   av_get_sample_fmt_name(dec_ctx->sample_fmt));
    av_channel_layout_describe(&dec_ctx->ch_layout, args + ret, sizeof(args) - ret);
    ret = avfilter_graph_create_filter(&filter_context->buffersrc_ctx, abuffersrc, "in",
                                       args, NULL, filter_context->filter_graph);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer source\n");
        goto end;
    }

    /* buffer audio sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&filter_context->buffersink_ctx, abuffersink, "out",
                                       NULL, NULL, filter_context->filter_graph);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Cannot create audio buffer sink\n");
        goto end;
    }

    // ret = av_opt_set_int_list(filter_context->buffersink_ctx, "sample_fmts", out_sample_fmts, -1,
    //                           AV_OPT_SEARCH_CHILDREN);
    // if (ret < 0)
    // {
    //     av_log(NULL, AV_LOG_ERROR, "Cannot set output sample format\n");
    //     goto end;
    // }

    // ret = av_opt_set(filter_context->buffersink_ctx, "ch_layouts", "mono",
    //                  AV_OPT_SEARCH_CHILDREN);
    // if (ret < 0)
    // {
    //     av_log(NULL, AV_LOG_ERROR, "Cannot set output channel layout\n");
    //     goto end;
    // }

    // ret = av_opt_set_int_list(filter_context->buffersink_ctx, "sample_rates", out_sample_rates, -1,
    //                           AV_OPT_SEARCH_CHILDREN);
    // if (ret < 0)
    // {
    //     av_log(NULL, AV_LOG_ERROR, "Cannot set output sample rate\n");
    //     goto end;
    // }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name = av_strdup("in");
    outputs->filter_ctx = filter_context->buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name = av_strdup("out");
    inputs->filter_ctx = filter_context->buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if ((ret = avfilter_graph_parse_ptr(filter_context->filter_graph, filters_descr.data(),
                                        &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_context->filter_graph, NULL)) < 0)
        goto end;

    /* Print summary of the sink buffer
     * Note: args buffer is reused to store channel layout string */
    outlink = filter_context->buffersink_ctx->inputs[0];
    av_channel_layout_describe(&outlink->ch_layout, args, sizeof(args));
    av_log(NULL, AV_LOG_INFO, "Output: srate:%dHz fmt:%s chlayout:%s\n",
           (int)outlink->sample_rate,
           (char *)av_x_if_null(
               av_get_sample_fmt_name(static_cast<AVSampleFormat>(outlink->format)), "?"),
           args);
    filt_frame = av_frame_alloc();
    if (!filt_frame)
    {
        goto end;
    }

    filter_context->filt_frame = filt_frame;

    this->audio_filters = std::move(filter_context);

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

int avpro::CommonMedia::decode(std::unique_ptr<CommonMediaContext> &context, std::unique_ptr<CommonFilterContext> &filter_context,
                               std::function<int(const AVFrame *, const CommonFilterContext *filter_context_ptr)> decode_callback)
{
    int audio_stream_index = context->stream_index;

    AVCodecContext *dec_ctx = context->dec_ctx;
    CommonFilterContext *filter_context_ptr = filter_context ? filter_context.get() : nullptr;
    AVFilterContext *buffersink_ctx = filter_context->buffersink_ctx;
    AVFilterContext *buffersrc_ctx = filter_context->buffersrc_ctx;

    int ret;
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    if (!packet || !frame)
    {
        fprintf(stderr, "Could not allocate frame or packet\n");
        return -1;
    }

    /* read all packets */
    int64_t total_samples = 0;
    while (1)
    {
        if ((ret = av_read_frame(fmt_ctx, packet)) < 0)
            break;

        if (packet->stream_index == audio_stream_index)
        {
            ret = avcodec_send_packet(dec_ctx, packet);
            if (ret < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
                break;
            }

            while (ret >= 0)
            {
                ret = avcodec_receive_frame(dec_ctx, frame);
                total_samples += frame->nb_samples;
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
                else if (ret < 0)
                {
                    av_log(NULL, AV_LOG_ERROR, "Error while receiving a frame from the decoder\n");
                    goto end;
                }

                if (ret >= 0)
                {
                    ret = decode_callback(frame, filter_context_ptr);
                    if (ret < 0)
                    {
                        goto end;
                    }

                    av_frame_unref(frame);
                }
            }
        }
        av_packet_unref(packet);
    }
end:
    av_packet_free(&packet);
    av_frame_free(&frame);

    const int64_t sample_rate = static_cast<int64_t>(dec_ctx->sample_rate);
    decoded_duration = av_rescale(total_samples, 1000, sample_rate);

    if (ret < 0 && ret != AVERROR_EOF)
    {
        return -1;
    }
    return 0;
}

avpro::CommonMedia::~CommonMedia()
{
    avformat_close_input(&fmt_ctx);
}