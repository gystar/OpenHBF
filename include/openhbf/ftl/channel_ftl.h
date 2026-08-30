#pragma once
#include "openhbf/ftl/geometry_mapper.h"
#include "openhbf/media/nand_media.h"
#include <unordered_map>
namespace openhbf::ftl {
struct MediaSubmitResult { media::SubmitState state{media::SubmitState::Rejected}; media::MediaToken token{}; media::MediaStatus status{media::MediaStatus::InvalidCommand}; };
class IMediaPort {
 public:
  virtual ~IMediaPort() = default;
  virtual MediaSubmitResult issue(const media::MediaCommand&, Cycle) = 0;
  virtual void cancel_generation(Generation) {}
};

// Thin adapter used by integrations that own a NandMedia instance.  Event
// delivery remains external: the controller/event engine calls NandMedia's
// on_event(), while this adapter only forwards issue/cancel operations.
class NandMediaPort final : public IMediaPort {
 public:
  explicit NandMediaPort(media::NandMedia& media) : media_(media) {}
  MediaSubmitResult issue(const media::MediaCommand& command,
                          Cycle now) override {
    const auto result = media_.try_issue(command, now);
    return {result.state, result.token, result.status};
  }
  void cancel_generation(Generation generation) override {
    media_.cancel_generation(generation, Cycle{});
  }

 private:
  media::NandMedia& media_;
};
class IFtlCompletionSink { public: virtual ~IFtlCompletionSink()=default; virtual void complete(HostToken, media::MediaStatus)=0; };
class ChannelFtl {
 public:
  ChannelFtl(GeometryMapper mapper, IMediaPort& media, IFtlCompletionSink& sink):mapper_(std::move(mapper)),media_(media),sink_(sink){}
  media::SubmitState issue_program(const ProgramDluRequest&, Cycle);
  media::SubmitState issue_read(const ReadDluRequest&, Cycle);
  void on_completion(const media::MediaCompletion&);
  void reset(Generation next_generation);
  Generation generation() const noexcept{return generation_;}
 private:
  struct Pending { HostToken host{}; Generation generation{}; media::MediaToken media{}; media::MediaOp op{}; media::MediaAddress address{}; bool done=false; };
  GeometryMapper mapper_; IMediaPort& media_; IFtlCompletionSink& sink_; Generation generation_{}; FtlToken next_{FtlToken(1)}; std::unordered_map<std::uint64_t,Pending> pending_;
};
}
