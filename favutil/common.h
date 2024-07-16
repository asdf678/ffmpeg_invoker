#ifndef AVPRO_COMMON_H
#define AVPRO_COMMON_H

#include <string_view>
#include <functional>
#include <memory>
#include <string>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
}

namespace avpro
{
    struct CommonMediaContext
    {
        int stream_index{-1};
        const AVCodec *dec{nullptr};
        AVStream *stream{nullptr};
        AVCodecContext *dec_ctx{nullptr};

        ~CommonMediaContext();
    };

    struct CommonFilterContext
    {
        AVFilterContext *buffersink_ctx{nullptr};
        AVFilterContext *buffersrc_ctx{nullptr};
        AVFilterGraph *filter_graph{nullptr};
        AVFrame *filt_frame{nullptr};

        int
        filter(const AVFrame *frame, std::function<void(const AVFrame *)> filter_callback) const;

        ~CommonFilterContext();
    };

    class CommonMedia
    {
        AVFormatContext *fmt_ctx{nullptr};
        int sample_rate{-1};
        std::string sample_fmt{};
        std::string channel_layout{};
        int decoded_duration{-1};

    protected:
        std::unique_ptr<CommonMediaContext> audio_context{nullptr};
        std::unique_ptr<CommonFilterContext> audio_filters{nullptr};

        std::unique_ptr<CommonMediaContext> find_stream(AVMediaType type);

        int open_codec(std::unique_ptr<CommonMediaContext> &context);

        int decode(std::unique_ptr<CommonMediaContext> &context,
                   std::unique_ptr<CommonFilterContext> &filter_context,
                   std::function<int(const AVFrame *,
                                     const CommonFilterContext *filter_context_ptr)>
                       decode_callback);

        int init_audio_filters(const std::string_view filters_descr,
                               std::unique_ptr<CommonMediaContext> &context);

    public:
        AVFormatContext &get_format_context()
        {
            return *fmt_ctx;
        }

        CommonMediaContext &get_media_context()
        {
            return *audio_context;
        }

        int open_input(std::string_view url);

        int open_audio_stream()
        {
            audio_context = find_stream(AVMediaType::AVMEDIA_TYPE_AUDIO);
            if (!audio_context)
            {
                return -1;
            }
            return 0;
        }

        int open_audio_codec()
        {
            int ret = open_codec(audio_context);
            if (ret < 0)
            {
                sample_rate = -1;
                sample_fmt = "";
                channel_layout = "";
            }
            else
            {
                sample_rate = audio_context->dec_ctx->sample_rate;
                sample_fmt = av_get_sample_fmt_name(audio_context->dec_ctx->sample_fmt);
                char ch_layout_args[128]{};
                av_channel_layout_describe(&audio_context->dec_ctx->ch_layout, ch_layout_args,
                                           sizeof(ch_layout_args));
                channel_layout = ch_layout_args;
            }
            return ret;
        }

        int init_audio_filters(std::string_view filters_descr)
        {
            return init_audio_filters(filters_descr, audio_context);
        }

        int decode_audio(std::function<int(const AVFrame *,
                                           const CommonFilterContext *filter_context_ptr)>
                             frame_callback)
        {
            return decode(audio_context, audio_filters, std::move(frame_callback));
        }

        int64_t get_decoded_duration()
        {
            return decoded_duration;
        }

        int get_sample_rate()
        {
            return sample_rate;
        }

        std::string get_sample_fmt()
        {
            return sample_fmt;
        }

        std::string get_channel_layout()
        {
            return channel_layout;
        }

    public:
        virtual ~CommonMedia();
    };
}

#endif