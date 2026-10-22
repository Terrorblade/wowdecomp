#ifndef HOT_STREAMING_HTTPTRANSPORT_H
#define HOT_STREAMING_HTTPTRANSPORT_H

#include <string>
#include "HoT/Streaming/Sha256.h"

// Shared by HttpClient.cpp and the libcurl transport in HttpTransport.cpp.
namespace HoT::Streaming {

// Feeds the first count bytes of a partial download to hash before a resume.
bool HashPrefix(const std::wstring& path, long long count, Sha256Stream& hash);

}

#endif
