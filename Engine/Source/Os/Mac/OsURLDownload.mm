#include <cstdint>
#include <cstring>
#import <Foundation/Foundation.h>
#include "Engine/Source/Os/OsURLDownload.h"
#include "Engine/Source/Storm/H/storm.h"

// NSURLSession keeps the WinINet contract and also follows http to https redirects, which the POSIX socket client can't.

@interface OsURLDownloadDelegate : NSObject <NSURLSessionDataDelegate>
@end

static NSMutableDictionary<NSNumber*, NSValue*>* s_downloads;

static OsUrlDownload* DownloadForTask(NSURLSessionTask* task) {
    @synchronized(s_downloads) {
        return static_cast<OsUrlDownload*>([s_downloads[@(task.taskIdentifier)] pointerValue]);
    }
}

@implementation OsURLDownloadDelegate

- (void)URLSession:(NSURLSession*)session dataTask:(NSURLSessionDataTask*)dataTask didReceiveResponse:(NSURLResponse*)response completionHandler:(void (^)(NSURLSessionResponseDisposition))completionHandler {
    OsUrlDownload* download = DownloadForTask(dataTask);

    if (!download) {
        completionHandler(NSURLSessionResponseCancel);
        return;
    }

    NSHTTPURLResponse* http = [response isKindOfClass:[NSHTTPURLResponse class]] ? static_cast<NSHTTPURLResponse*>(response) : nil;
    download->m_statusCode = http ? static_cast<uint32_t>(http.statusCode) : 0;

    if (download->m_statusCode >= 200 && download->m_statusCode < 300) {
        completionHandler(NSURLSessionResponseAllow);
        return;
    }

    download->m_errorCode = download->m_statusCode == 404 ? HTTPError::NotFound : HTTPError::Failed;
    download->m_done = true;
    completionHandler(NSURLSessionResponseCancel);
}

- (void)URLSession:(NSURLSession*)session dataTask:(NSURLSessionDataTask*)dataTask didReceiveData:(NSData*)data {
    OsUrlDownload* download = DownloadForTask(dataTask);

    if (!download || download->m_done) {
        return;
    }

    [data enumerateByteRangesUsingBlock:^(const void* bytes, NSRange range, BOOL* stop) {
        if (download->m_callback && !download->m_callback(download->m_param, static_cast<const char*>(bytes), static_cast<uint32_t>(range.length), false, HTTPError::NoError)) {
            download->m_done = true;
            *stop = YES;
        }
    }];

    if (download->m_done) {
        [dataTask cancel];
    }
}

- (void)URLSession:(NSURLSession*)session task:(NSURLSessionTask*)task didCompleteWithError:(NSError*)error {
    OsUrlDownload* download = DownloadForTask(task);

    if (!download) {
        return;
    }

    @synchronized(s_downloads) {
        [s_downloads removeObjectForKey:@(task.taskIdentifier)];
    }

    if (error && !download->m_done) {
        download->m_errorCode = error.code == NSURLErrorTimedOut ? HTTPError::Timeout : HTTPError::Failed;
    }

    OsURLDownloadCallback callback = download->m_callback;
    void* param = download->m_param;
    HTTPError::ErrorCode errorCode = download->m_errorCode;

    if (download->m_request) {
        CFRelease(download->m_request);
    }

    delete download;

    if (callback) {
        callback(param, nullptr, 0, true, errorCode);
    }
}

@end

OsUrlDownload::OsUrlDownload() {
    this->m_internet = nullptr;
    this->m_connect = nullptr;
    this->m_request = nullptr;
    this->m_connectEvent = nullptr;
    this->m_requestEvent = nullptr;
    this->m_completeEvent = nullptr;
    this->m_dataEvent = nullptr;
    this->m_state = 0;
    this->m_done = false;
    this->m_statusCode = 0;
    this->m_callback = nullptr;
    this->m_error = 0;
    this->m_errorCode = HTTPError::NoError;
}

static NSURLSession* Session() {
    static NSURLSession* s_session;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        NSURLSessionConfiguration* configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
        configuration.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
        configuration.HTTPAdditionalHeaders = @{ @"User-Agent": @"Blizzard Web Client" };

        NSOperationQueue* queue = [[NSOperationQueue alloc] init];
        queue.maxConcurrentOperationCount = 1;
        queue.name = @"OsUrlDownload";

        s_downloads = [[NSMutableDictionary alloc] init];
        s_session = [NSURLSession sessionWithConfiguration:configuration delegate:[[OsURLDownloadDelegate alloc] init] delegateQueue:queue];
    });

    return s_session;
}

bool OsUrlDownload::DownloadString(const char* url, OsURLDownloadCallback callback, void* param, int32_t timeout) {
    @autoreleasepool {
        NSURL* target = url ? [NSURL URLWithString:[NSString stringWithUTF8String:url]] : nil;

        if (!target || !target.host) {
            return false;
        }

        NSMutableURLRequest* request = [NSMutableURLRequest requestWithURL:target];
        request.HTTPMethod = @"GET";

        if (timeout > 0) {
            request.timeoutInterval = timeout / 1000.0;
        }

        OsUrlDownload* download = new OsUrlDownload();
        download->m_callback = callback;
        download->m_param = param;
        download->m_state = 1;

        NSURLSessionDataTask* task = [Session() dataTaskWithRequest:request];
        download->m_request = const_cast<void*>(CFBridgingRetain(task));

        @synchronized(s_downloads) {
            s_downloads[@(task.taskIdentifier)] = [NSValue valueWithPointer:download];
        }

        [task resume];

        return true;
    }
}

bool OsURLDownload(const char* url, OsURLDownloadCallback callback, void* param) {
    return OsUrlDownload::DownloadString(url, callback, param, 5000);
}
