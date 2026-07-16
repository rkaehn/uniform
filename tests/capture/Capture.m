#import <XCTest/XCTest.h>
#import <UIKit/UIKit.h>

static const CGFloat beacon_cell_pt = 2.0;

@interface Capture : XCTestCase
@end

@implementation Capture {
    uint32_t _sample_log_count;
}

static const uint8_t beacon_pattern[8][3] = {
    { 255, 0, 0 }, { 0, 255, 0 }, { 0, 0, 255 }, { 0, 255, 255 },
    { 255, 0, 255 }, { 255, 255, 0 }, { 0, 0, 0 }, { 255, 255, 255 },
};

- (bool)sampleBeaconRow:(uint8_t[8][3])out_rgb {
    @autoreleasepool {
    UIImage* image = XCUIScreen.mainScreen.screenshot.image;
    UIGraphicsImageRendererFormat* format = [UIGraphicsImageRendererFormat new];
    format.scale = 1.0;
    UIGraphicsImageRenderer* renderer = [[UIGraphicsImageRenderer alloc] initWithSize:CGSizeMake(8.0, 1.0) format:format];
    UIImage* row = [renderer imageWithActions:^(UIGraphicsImageRendererContext* context) {
        CGContextSetInterpolationQuality(context.CGContext, kCGInterpolationNone);
        [image drawInRect:CGRectMake(0.0, 0.0, image.size.width / beacon_cell_pt, image.size.height / beacon_cell_pt)];
    }];
    CGImageRef cg = row.CGImage;
    CFDataRef data = cg != NULL ? CGDataProviderCopyData(CGImageGetDataProvider(cg)) : NULL;
    if (data == NULL || CFDataGetLength(data) < 32) {
        if (data != NULL) {
            CFRelease(data);
        }
        NSLog(@"capture sample: readback failed");
        return false;
    }
    const uint8_t* px = CFDataGetBytePtr(data);
    CGImageAlphaInfo alpha = CGImageGetAlphaInfo(cg);
    bool alpha_first = alpha == kCGImageAlphaPremultipliedFirst || alpha == kCGImageAlphaFirst || alpha == kCGImageAlphaNoneSkipFirst;
    bool little = (CGImageGetBitmapInfo(cg) & kCGBitmapByteOrderMask) == kCGBitmapByteOrder32Little;
    size_t stride = (size_t)CGImageGetBitsPerPixel(cg) / 8;
    for (uint32_t i = 0; i < 8; i++) {
        const uint8_t* p = px + i * stride;
        if (little) {
            out_rgb[i][0] = alpha_first ? p[2] : p[3];
            out_rgb[i][1] = alpha_first ? p[1] : p[2];
            out_rgb[i][2] = alpha_first ? p[0] : p[1];
        } else {
            out_rgb[i][0] = alpha_first ? p[1] : p[0];
            out_rgb[i][1] = alpha_first ? p[2] : p[1];
            out_rgb[i][2] = alpha_first ? p[3] : p[2];
        }
    }
    CFRelease(data);
    if (_sample_log_count < 8) {
        _sample_log_count += 1;
        NSLog(@"capture sample %u: (%u,%u,%u) (%u,%u,%u) .. (%u,%u,%u)", _sample_log_count,
              out_rgb[0][0], out_rgb[0][1], out_rgb[0][2],
              out_rgb[1][0], out_rgb[1][1], out_rgb[1][2],
              out_rgb[7][0], out_rgb[7][1], out_rgb[7][2]);
    }
    return true;
    }
}

static bool is_beacon(const uint8_t rgb[8][3]) {
    for (uint32_t i = 0; i < 8; i++) {
        for (uint32_t ch = 0; ch < 3; ch++) {
            int32_t d = (int32_t)rgb[i][ch] - (int32_t)beacon_pattern[i][ch];
            if (d < -60 || d > 60) {
                return false;
            }
        }
    }
    return true;
}

- (bool)confirmScreenshotDeletion:(XCUIApplication*)springboard {
    for (uint32_t attempt = 0; attempt < 5; attempt++) {
        for (NSString* title in @[ @"Delete", @"Allow" ]) {
            XCUIElement* button = springboard.buttons[title];
            if ([button waitForExistenceWithTimeout:2.0] == false) {
                continue;
            }
            [NSThread sleepForTimeInterval:1.0];
            if (button.exists == false || button.isHittable == false) {
                continue;
            }
            [button tap];
            [NSThread sleepForTimeInterval:1.0];
            if (button.exists == false) {
                NSLog(@"capture: confirmed screenshot deletion (%@, attempt %u)", title, attempt + 1);
                [NSThread sleepForTimeInterval:1.0];
                return true;
            }
            NSLog(@"capture: deletion alert still up after tapping %@ (attempt %u)", title, attempt + 1);
        }
    }
    NSLog(@"capture: deletion alert never confirmed");
    return false;
}

- (void)testCapture {
    XCUIApplication* tests = [[XCUIApplication alloc] initWithBundleIdentifier:@"com.raffaelkaehn.tests"];
    NSString* ref = NSProcessInfo.processInfo.environment[@"CAPTURE_REF"];
    NSString* only = NSProcessInfo.processInfo.environment[@"CAPTURE_ONLY"];
    if (ref.length == 0) {
        XCTFail(@"the capture needs CAPTURE_REF, the reference folder to write");
        return;
    }
    tests.launchArguments = only.length > 0 ? @[ @"--capture", only, @"--ref", ref ] : @[ @"--capture", @"--ref", ref ];
    [tests launch];

    XCUIApplication* springboard = [[XCUIApplication alloc] initWithBundleIdentifier:@"com.apple.springboard"];
    XCUIElement* allow = springboard.buttons[@"Allow Full Access"];

    uint32_t shots = 0;
    bool deletion_confirmed = false;
    NSTimeInterval min_shot_spacing = 2.0;
    NSDate* last_shot = NSDate.distantPast;
    NSDate* overall_deadline = [NSDate dateWithTimeIntervalSinceNow:6.0 * 3600.0];
    while (tests.state == XCUIApplicationStateRunningForeground && [overall_deadline timeIntervalSinceNow] > 0.0) {
        bool saw_beacon = false;
        NSDate* beacon_deadline = [NSDate dateWithTimeIntervalSinceNow:25.0];
        NSDate* alert_check_at = [NSDate dateWithTimeIntervalSinceNow:4.0];
        while ([beacon_deadline timeIntervalSinceNow] > 0.0 && tests.state == XCUIApplicationStateRunningForeground) {
            uint8_t rgb[8][3];
            if ([self sampleBeaconRow:rgb] && is_beacon(rgb)) {
                saw_beacon = true;
                break;
            }
            if ([alert_check_at timeIntervalSinceNow] < 0.0) {
                alert_check_at = [NSDate dateWithTimeIntervalSinceNow:2.0];
                if (allow.exists) {
                    [allow tap];
                    NSLog(@"capture photo permission granted");
                    beacon_deadline = [NSDate dateWithTimeIntervalSinceNow:25.0];
                    continue;
                }
                if (springboard.buttons[@"Delete"].exists || springboard.buttons[@"Allow"].exists) {
                    deletion_confirmed = [self confirmScreenshotDeletion:springboard];
                    if (deletion_confirmed) {
                        break;
                    }
                }
            }
        }
        if (saw_beacon == false) {
            if (deletion_confirmed == false && tests.state == XCUIApplicationStateRunningForeground) {
                [self confirmScreenshotDeletion:springboard];
            }
            break;
        }
        bool saw_content = false;
        NSDate* content_deadline = [NSDate dateWithTimeIntervalSinceNow:10.0];
        while ([content_deadline timeIntervalSinceNow] > 0.0 && tests.state == XCUIApplicationStateRunningForeground) {
            uint8_t rgb[8][3];
            if ([self sampleBeaconRow:rgb] && is_beacon(rgb) == false) {
                saw_content = true;
                break;
            }
        }
        if (saw_content == false) {
            break;
        }
        NSTimeInterval since_last = [NSDate.date timeIntervalSinceDate:last_shot];
        if (since_last < min_shot_spacing) {
            [NSThread sleepForTimeInterval:min_shot_spacing - since_last];
        }
        [tests typeKey:@"3" modifierFlags:XCUIKeyModifierCommand | XCUIKeyModifierShift];
        last_shot = NSDate.date;
        shots += 1;
        NSLog(@"capture %u fired", shots);
        [NSThread sleepForTimeInterval:0.5];
    }
    if (tests.state != XCUIApplicationStateNotRunning) {
        [tests terminate];
    }
    NSLog(@"capture total %u", shots);
}

@end
