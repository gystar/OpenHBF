#include "openhbf/controller/backend.h"
// Port contracts are intentionally pure virtual; adapters must not complete
// an Accepted request synchronously or retain Busy/Rejected payloads.
