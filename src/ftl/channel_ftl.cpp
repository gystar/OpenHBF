#include "openhbf/ftl/channel_ftl.h"
namespace openhbf::ftl {
namespace {
media::PageClass page_class_for(const GeometryMapper& mapper,
                                std::uint32_t page) {
  // NAND timing requires an explicit TLC wordline class.  The mapper owns
  // this vendor-neutral layout policy; Host and FTL callers need not know it.
  if (mapper.profile().cell_mode != media::CellMode::Tlc) {
    return media::PageClass::Default;
  }
  switch (page % 3U) {
    case 0: return media::PageClass::Lsb;
    case 1: return media::PageClass::Csb;
    default: return media::PageClass::Msb;
  }
}
}  // namespace

media::SubmitState ChannelFtl::issue_program(const ProgramDluRequest& r, Cycle now){
 if(!r.payload || r.channel.value() >= mapper_.profile().channels || !is_valid_token(r.token)) return media::SubmitState::Rejected;
 auto m=mapper_.map(r.channel,r.address); if(!m) return media::SubmitState::Rejected;
 media::MediaCommand c; c.op=media::MediaOp::Program; c.address=m.value().address; c.generation=generation_; c.origin=r.token; c.payload=std::make_shared<const media::PageData>(*r.payload);
 c.page_class = page_class_for(mapper_, m.value().address.page);
 auto s=media_.issue(c,now); if(s.state==media::SubmitState::Accepted) pending_[s.token.value()]={r.token,generation_,s.token,c.op,c.address,false}; return s.state;
}
media::SubmitState ChannelFtl::issue_read(const ReadDluRequest& r, Cycle now){
 if (!is_valid_token(r.token) || r.channel.value() >= mapper_.profile().channels)
   return media::SubmitState::Rejected;
 auto m=mapper_.map(r.channel,r.address); if(!m) return media::SubmitState::Rejected;
 media::MediaCommand c; c.op=media::MediaOp::Read; c.address=m.value().address; c.generation=generation_; c.origin=r.token;
 c.page_class = page_class_for(mapper_, m.value().address.page);
 auto s=media_.issue(c,now); if(s.state==media::SubmitState::Accepted) pending_[s.token.value()]={r.token,generation_,s.token,c.op,c.address,false}; return s.state;
}
void ChannelFtl::on_completion(const media::MediaCompletion& c){ auto it=pending_.find(c.token.value()); if(it==pending_.end()||it->second.done||it->second.generation!=generation_||it->second.address!=c.address||it->second.op!=c.op) return; it->second.done=true; sink_.complete(it->second.host,c.status); pending_.erase(it); }
void ChannelFtl::reset(Generation next_generation){
 if (next_generation <= generation_) return;
 for (const auto& entry : pending_) sink_.complete(entry.second.host, media::MediaStatus::Aborted);
 media_.cancel_generation(generation_);
 generation_ = next_generation;
 pending_.clear();
}
}
