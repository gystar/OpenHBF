#include "openhbf/controller/dlu_accumulator.h"
namespace openhbf::controller {
Status DluAccumulator::accept(const WriteSegment& s, Cycle now,
                              std::optional<ReadyDlu>* out) {
  if (out == nullptr || s.channel.value() >= channels_ || s.segment >= 64 ||
      s.host.value() == 0) {
    return Status::Invalid;
  }
  const Key k{s.channel, s.dlu};
  auto it = entries_.find(k);
  if (it == entries_.end()) {
    if (entries_.size() >= max_entries_) return Status::Mpdlu;
    it = entries_.emplace(k, Entry{}).first;
    // Saturate rather than wrapping an unsigned cycle counter.
    const auto n = now.value();
    const auto delta = timeout_.value();
    const auto max = std::numeric_limits<uint64_t>::max();
    it->second.deadline = Cycle(delta > max - n ? max : n + delta);
    it->second.generation = s.generation;
  }
  auto& e = it->second;
  if (e.generation != s.generation) return Status::Invalid;
  if (e.mask.test(s.segment)) return Status::Duplicate;
  e.mask.set(s.segment);
  e.data[s.segment] = s.data;
  e.waiters.push_back(s.host);
  if (e.mask.all()) {
    ReadyDlu r;
    r.channel = s.channel;
    r.dlu = s.dlu;
    r.generation = e.generation;
    r.waiters = std::move(e.waiters);
    for (size_t i = 0; i < 64; ++i)
      for (size_t j = 0; j < 64; ++j) r.data[i * 64 + j] = e.data[i][j];
    entries_.erase(it);
    *out = std::move(r);
  }
  return Status::Accepted;
}

HazardResult DluAccumulator::probe(ChannelId c, DluIndex d,
                                   SegmentIndex seg) const {
  if (c.value() >= channels_ || seg >= 64) return {Hazard::Absent, {}};
  const auto it = entries_.find(Key{c, d});
  if (it == entries_.end()) return {Hazard::Absent, {}};
  if (it->second.mask.test(seg)) return {Hazard::Forward, it->second.data[seg]};
  return {Hazard::Pending, {}};
}
std::vector<DluAccumulator::ExpiredDlu> DluAccumulator::expire(Cycle now){
 std::vector<ExpiredDlu> result;
 for(auto it=entries_.begin();it!=entries_.end();){
  if(it->second.deadline<=now){
   ExpiredDlu e;
   e.channel=it->first.channel;
   e.dlu=it->first.dlu; e.generation=it->second.generation;
   e.waiters=std::move(it->second.waiters); result.push_back(std::move(e)); it=entries_.erase(it);
  } else ++it;
 }
 return result;
}
std::vector<DluAccumulator::ExpiredDlu> DluAccumulator::cancel(Generation g) {
  std::vector<ExpiredDlu> result;
  for (auto it = entries_.begin(); it != entries_.end();) {
    if (it->second.generation != g) { ++it; continue; }
    ExpiredDlu e;
    e.channel = it->first.channel;
    e.dlu = it->first.dlu;
    e.generation = g;
    e.waiters = std::move(it->second.waiters);
    result.push_back(std::move(e));
    it = entries_.erase(it);
  }
  return result;
}
}
