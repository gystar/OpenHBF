#include "openhbf/controller/base_die_controller.h"
namespace openhbf::controller {
BaseDieController::BaseDieController(uint16_t c,size_t m,Cycle t,IHostResponsePort& h,IFtlControllerPort& f,IMediaControllerPort& x):accumulator_(c,m,t),host_(h),ftl_(f),media_(x){}
Status BaseDieController::accept(ChannelCommand c,Cycle now){
 if(c.segment.generation!=generation_)return Status::Invalid;
 ControllerTxn txn{TxnId(c.segment.host.value()),c.segment.channel,c.segment.host,c.segment.generation,ControllerOp::WriteSegment,ControllerState::Accumulating};
 txns_.emplace(txn.id.value(),txn);
 std::optional<ReadyDlu> ready; auto s=accumulator_.accept(c.segment,now,&ready);
 if(s!=Status::Accepted && s!=Status::Duplicate){txns_.erase(txn.id.value()); return s;}
 if(ready){auto ir=ftl_.issue(*ready,now); if(ir.status!=Status::Accepted)return ir.status; auto mr=media_.issue(*ready,now); if(mr.status!=Status::Accepted)return mr.status;}
 return s;
}
void BaseDieController::complete(const ControllerTxn& t,Status s){if(host_.enqueue(t.host,s))txns_.erase(t.id.value());}
void BaseDieController::on_event(const ControllerEvent& e,Cycle){auto it=txns_.find(e.txn.value());if(it==txns_.end()||it->second.generation!=e.generation)return;complete(it->second,e.status);}
void BaseDieController::request_reset(Generation g,Cycle){const Generation old=generation_; accumulator_.cancel(old); ftl_.cancel(old); media_.cancel(old); for(auto& p:txns_)host_.enqueue(p.second.host,Status::Aborted);txns_.clear(); generation_=g;}
}
