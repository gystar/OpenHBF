#pragma once
#include "controller_types.h"
namespace openhbf::controller {
struct IssueResult { Status status; CommandToken token{}; };
class IFtlControllerPort { public: virtual ~IFtlControllerPort()=default; virtual IssueResult issue(const ReadyDlu&, Cycle)=0; virtual void cancel(Generation)=0; };
class IMediaControllerPort { public: virtual ~IMediaControllerPort()=default; virtual IssueResult issue(const ReadyDlu&, Cycle)=0; virtual void cancel(Generation)=0; };
class IHostResponsePort { public: virtual ~IHostResponsePort()=default; virtual bool enqueue(HostToken, Status)=0; };
}
