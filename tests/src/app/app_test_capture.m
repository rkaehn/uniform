#include "app_tests_internal.h"

#if cr_macos

#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <QuartzCore/QuartzCore.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <mach/mach_time.h>

@class AppTestCaptureOutput;

struct app_test_track {
    bool active;
    bool transparent;
    int2 pos;
};

struct app_test_capture {
    CGRect region;
    double scale;
    SCStream* stream;
    SCContentFilter* filter;
    AppTestCaptureOutput* output;
    dispatch_queue_t queue;
    pthread_mutex_t mutex;
    struct app_test_track tracks[app_test_track_max];
    struct app_test_result* results;
    uint32_t result_count;
};

struct app_test_image {
    const uint8_t* bgra;
    size_t bytes_per_row;
    int2 size;
};

@interface AppTestCaptureOutput: NSObject<SCStreamOutput, SCStreamDelegate>
- (instancetype)initWithCapture:(struct app_test_capture*)capture;
@end

static double app_test_mach_seconds(uint64_t ticks) {
    mach_timebase_info_data_t timebase;
    mach_timebase_info(&timebase);
    return (double)ticks * timebase.numer / timebase.denom * 1e-9;
}

static int2 app_test_capture_px(const struct app_test_capture* capture, double2 pos_pt) {
    double2 px = (pos_pt - make_double2(capture->region.origin.x, capture->region.origin.y)) * capture->scale;
    return to_int2_from_double2(round_double2(px));
}

static bool app_test_image_in(const struct app_test_image* image, int2 p) {
    return all_int2(p >= 0) && all_int2(p < image->size);
}

static const uint8_t* app_test_image_px(const struct app_test_image* image, int2 p) {
    return image->bgra + (size_t)p.y * image->bytes_per_row + (size_t)p.x * 4;
}

static bool app_test_px_is_dark(const uint8_t* px) {
    return px[0] < 60 && px[1] < 60 && px[2] < 60;
}

static bool app_test_px_is_magenta(const uint8_t* px, int32_t mag, int32_t lo) {
    return px[2] > mag && px[0] > mag && px[1] < lo;
}

static bool app_test_image_dark_or_outside(const struct app_test_image* image, int2 p) {
    return app_test_image_in(image, p) == false || app_test_px_is_dark(app_test_image_px(image, p));
}

static int32_t app_test_image_distance(int2 origin, int2 p, int2 step) {
    int2 d = (p - origin) * step;
    return d.x + d.y;
}

static int32_t app_test_image_extent(const struct app_test_image* image, int2 origin, int2 start, int2 step, int32_t mag, int32_t lo) {
    int2 p = start;
    while (app_test_image_in(image, p + step) && app_test_image_distance(origin, p, step) < 8192 && app_test_px_is_magenta(app_test_image_px(image, p), mag, lo) == false) {
        p += step;
    }
    if (app_test_image_in(image, p) && app_test_px_is_magenta(app_test_image_px(image, p), mag, lo)) {
        return app_test_image_distance(origin, p, step) + 4;
    }
    return 0;
}

static struct app_test_result app_test_capture_analyze(const struct app_test_image* image, const struct app_test_track* track, uint32_t index, double display_time) {
    struct app_test_result r = {
        .time = CACurrentMediaTime(),
        .display_time = display_time,
        .track = index,
    };
    int32_t hi = track->transparent ? 100 : 200;
    int32_t lo = track->transparent ? 40 : 60;
    int32_t mag = track->transparent ? 90 : 180;

    uint32_t dec[app_test_barcode_row_count] = { 0 };
    bool valid = true;
    bool any_bright = false;
    for (uint32_t row = 0; row < app_test_barcode_row_count; row++) {
        for (uint32_t bit = 0; bit < app_test_barcode_bits; bit++) {
            int2 cell = make_int2(app_test_barcode_x + (int32_t)bit * app_test_barcode_cell, app_test_barcode_y + (int32_t)row * app_test_barcode_cell);
            int2 p = track->pos + cell + app_test_barcode_cell / 2;
            if (app_test_image_in(image, p) == false) {
                valid = false;
                continue;
            }
            int32_t g = app_test_image_px(image, p)[1];
            if (g > hi) {
                dec[row] |= 1u << bit;
                any_bright = true;
            } else if (g > lo) {
                valid = false;
            }
        }
    }
    r.valid = valid && dec[app_test_barcode_row_sync] == app_test_barcode_sync;
    r.dec_size = make_int2((int32_t)dec[app_test_barcode_row_width], (int32_t)dec[app_test_barcode_row_height]);
    r.dec_frame = (int32_t)dec[app_test_barcode_row_frame];

    bool all_dark = any_bright == false;
    for (int32_t i = 0; i < 12 && all_dark; i++) {
        int2 p = track->pos + make_int2(24 + i * 23, 100 + i * 17);
        if (app_test_image_in(image, p) && app_test_px_is_dark(app_test_image_px(image, p)) == false) {
            all_dark = false;
        }
    }
    r.absent = all_dark;

    if (r.valid) {
        int32_t y_mid = track->pos.y + r.dec_size.y / 2;
        int32_t x_mid = track->pos.x + r.dec_size.x / 2;
        r.ext_size.x = app_test_image_extent(image, track->pos, make_int2(track->pos.x + app_test_barcode_x, y_mid), make_int2(1, 0), mag, lo);
        r.ext_size.y = app_test_image_extent(image, track->pos, make_int2(x_mid, track->pos.y + app_test_barcode_y + 40), make_int2(0, 1), mag, lo);
        int2 outside = track->pos + r.dec_size + 4;
        r.outside_dark = app_test_image_dark_or_outside(image, make_int2(outside.x, y_mid)) && app_test_image_dark_or_outside(image, make_int2(x_mid, outside.y));
    }
    return r;
}

static void app_test_capture_frame(struct app_test_capture* capture, CMSampleBufferRef sample_buffer) {
    CVPixelBufferRef pixels = CMSampleBufferGetImageBuffer(sample_buffer);
    if (pixels == NULL) {
        return;
    }
    NSArray* attachments = (__bridge NSArray*)CMSampleBufferGetSampleAttachmentsArray(sample_buffer, false);
    NSDictionary* attachment = attachments.firstObject;
    NSNumber* status = attachment[SCStreamFrameInfoStatus];
    if (status && status.intValue != SCFrameStatusComplete) {
        return;
    }
    NSNumber* display_ticks = attachment[SCStreamFrameInfoDisplayTime];
    double display_time = display_ticks ? app_test_mach_seconds(display_ticks.unsignedLongLongValue) : 0.0;

    struct app_test_track tracks[app_test_track_max];
    pthread_mutex_lock(&capture->mutex);
    memcpy(tracks, capture->tracks, sizeof(tracks));
    pthread_mutex_unlock(&capture->mutex);

    CVPixelBufferLockBaseAddress(pixels, kCVPixelBufferLock_ReadOnly);
    struct app_test_image image = {
        .bgra = CVPixelBufferGetBaseAddress(pixels),
        .bytes_per_row = CVPixelBufferGetBytesPerRow(pixels),
        .size = make_int2((int32_t)CVPixelBufferGetWidth(pixels), (int32_t)CVPixelBufferGetHeight(pixels)),
    };
    for (uint32_t t = 0; t < app_test_track_max; t++) {
        if (tracks[t].active == false) {
            continue;
        }
        struct app_test_result r = app_test_capture_analyze(&image, &tracks[t], t, display_time);
        pthread_mutex_lock(&capture->mutex);
        if (capture->result_count < app_test_result_max) {
            capture->results[capture->result_count++] = r;
        }
        pthread_mutex_unlock(&capture->mutex);
    }
    CVPixelBufferUnlockBaseAddress(pixels, kCVPixelBufferLock_ReadOnly);
}

@implementation AppTestCaptureOutput {
    struct app_test_capture* _capture;
}

- (instancetype)initWithCapture:(struct app_test_capture*)capture {
    self = [super init];
    _capture = capture;
    return self;
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error {
    printf("app window capture: stream stopped: %s\n", error.localizedDescription.UTF8String);
}

- (void)stream:(SCStream*)stream didOutputSampleBuffer:(CMSampleBufferRef)sample_buffer ofType:(SCStreamOutputType)type {
    if (type == SCStreamOutputTypeScreen) {
        app_test_capture_frame(_capture, sample_buffer);
    }
}
@end

static SCShareableContent* app_test_capture_content(void) {
    __block SCShareableContent* content = nil;
    __block bool done = false;
    [SCShareableContent getCurrentProcessShareableContentWithCompletionHandler:^(SCShareableContent* result, NSError* error) {
        content = [result retain];
        done = true;
    }];
    while (done == false) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
    }
    return content;
}

static SCContentFilter* app_test_capture_filter(SCShareableContent* content) {
    SCRunningApplication* ours = nil;
    for (SCRunningApplication* application in content.applications) {
        if (application.processID == getpid()) {
            ours = application;
        }
    }
    SCDisplay* display = nil;
    for (SCDisplay* candidate in content.displays) {
        if (candidate.displayID == CGMainDisplayID()) {
            display = candidate;
        }
    }
    if (display == nil) {
        display = content.displays.firstObject;
    }
    if (ours == nil || display == nil) {
        printf("app window capture: %s\n", ours == nil ? "own process not shareable" : "no display");
        return nil;
    }
    return [[SCContentFilter alloc] initWithDisplay:display includingApplications:@[ours] exceptingWindows:@[]];
}

struct app_test_capture* app_test_capture_create_(CGRect region, double scale) {
    struct app_test_capture* capture = cr_malloc(cr_mem_libc, sizeof(*capture), 0, cr_loc);
    memset(capture, 0, sizeof(*capture));
    capture->region = region;
    capture->scale = scale;
    capture->output = [[AppTestCaptureOutput alloc] initWithCapture:capture];
    capture->queue = dispatch_queue_create("app test capture", DISPATCH_QUEUE_SERIAL);
    capture->results = cr_malloc(cr_mem_libc, app_test_result_max * sizeof(*capture->results), 0, cr_loc);
    pthread_mutex_init(&capture->mutex, NULL);
    return capture;
}

void app_test_capture_destroy_(struct app_test_capture* capture) {
    [capture->stream release];
    [capture->filter release];
    [capture->output release];
    dispatch_release(capture->queue);
    pthread_mutex_destroy(&capture->mutex);
    cr_free(cr_mem_libc, capture->results, cr_loc);
    cr_free(cr_mem_libc, capture, cr_loc);
}

bool app_test_capture_start_(struct app_test_capture* capture) {
    SCShareableContent* content = app_test_capture_content();
    if (content == nil) {
        return false;
    }
    capture->filter = app_test_capture_filter(content);
    [content release];
    if (capture->filter == nil) {
        return false;
    }
    SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
    config.sourceRect = capture->region;
    config.width = (size_t)(capture->region.size.width * capture->scale);
    config.height = (size_t)(capture->region.size.height * capture->scale);
    config.pixelFormat = kCVPixelFormatType_32BGRA;
    config.showsCursor = false;
    config.queueDepth = 8;
    config.minimumFrameInterval = CMTimeMake(1, 240);
    config.colorSpaceName = kCGColorSpaceSRGB;
    capture->stream = [[SCStream alloc] initWithFilter:capture->filter configuration:config delegate:capture->output];
    [config release];
    NSError* error = nil;
    [capture->stream addStreamOutput:capture->output type:SCStreamOutputTypeScreen sampleHandlerQueue:capture->queue error:&error];
    if (error) {
        printf("app window capture: add output failed: %s\n", error.localizedDescription.UTF8String);
        return false;
    }
    __block bool started = false;
    __block bool failed = false;
    [capture->stream startCaptureWithCompletionHandler:^(NSError* start_error) {
        if (start_error) {
            printf("app window capture: start failed: %s\n", start_error.localizedDescription.UTF8String);
        }
        failed = start_error != nil;
        started = true;
    }];
    while (started == false) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
    }
    return failed == false;
}

void app_test_capture_stop_(struct app_test_capture* capture) {
    __block bool stopped = false;
    [capture->stream stopCaptureWithCompletionHandler:^(NSError* error) {
        stopped = true;
    }];
    while (stopped == false) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
    }
    [capture->stream removeStreamOutput:capture->output type:SCStreamOutputTypeScreen error:nil];
    dispatch_sync(capture->queue, ^{});
}

void app_test_capture_set_track_(struct app_test_capture* capture, uint32_t index, double2 pos_pt, bool transparent) {
    struct app_test_track track = { .active = true, .transparent = transparent, .pos = app_test_capture_px(capture, pos_pt) };
    pthread_mutex_lock(&capture->mutex);
    capture->tracks[index] = track;
    pthread_mutex_unlock(&capture->mutex);
}

void app_test_capture_clear_track_(struct app_test_capture* capture, uint32_t index) {
    pthread_mutex_lock(&capture->mutex);
    capture->tracks[index].active = false;
    pthread_mutex_unlock(&capture->mutex);
}

void app_test_capture_clear_results_(struct app_test_capture* capture) {
    pthread_mutex_lock(&capture->mutex);
    capture->result_count = 0;
    pthread_mutex_unlock(&capture->mutex);
}

uint32_t app_test_capture_take_results_(struct app_test_capture* capture, struct app_test_result* out, uint32_t capacity) {
    pthread_mutex_lock(&capture->mutex);
    uint32_t count = min_uint32(capture->result_count, capacity);
    memcpy(out, capture->results, count * sizeof(*out));
    capture->result_count = 0;
    pthread_mutex_unlock(&capture->mutex);
    return count;
}

bool app_test_capture_pixel_(struct app_test_capture* capture, double2 pos_pt, uchar4* out_rgba) {
    assert(capture->filter != nil);
    *out_rgba = make_uchar4(0, 0, 0, 0);
    double2 origin = floor_double2(pos_pt);
    SCStreamConfiguration* config = [[SCStreamConfiguration alloc] init];
    config.sourceRect = CGRectMake(origin.x, origin.y, 4.0, 4.0);
    config.width = (size_t)(4.0 * capture->scale);
    config.height = (size_t)(4.0 * capture->scale);
    config.showsCursor = false;
    config.colorSpaceName = kCGColorSpaceSRGB;
    config.captureResolution = SCCaptureResolutionBest;
    __block CGImageRef image = NULL;
    __block bool captured = false;
    [SCScreenshotManager captureImageWithFilter:capture->filter configuration:config completionHandler:^(CGImageRef result, NSError* error) {
        image = result ? CGImageRetain(result) : NULL;
        captured = true;
    }];
    while (captured == false) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
    }
    [config release];
    if (image == NULL) {
        return false;
    }
    CFDataRef data = CGDataProviderCopyData(CGImageGetDataProvider(image));
    double2 offset = (pos_pt - origin) * capture->scale;
    const uint8_t* px = CFDataGetBytePtr(data) + (size_t)offset.y * CGImageGetBytesPerRow(image) + (size_t)offset.x * 4;
    *out_rgba = make_uchar4(px[2], px[1], px[0], px[3]);
    CFRelease(data);
    CGImageRelease(image);
    return true;
}

#endif
