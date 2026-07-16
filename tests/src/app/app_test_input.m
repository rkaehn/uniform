#include "app_tests_internal.h"

#if cr_macos

void app_test_run_for_(struct app* app, double seconds) {
    NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:seconds];
    while (deadline.timeIntervalSinceNow > 0.0) {
        cr_autoreleasepool {
            NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:deadline inMode:NSDefaultRunLoopMode dequeue:YES];
            if (event) {
                [NSApp sendEvent:event];
            }
        }
    }
}

static NSEvent* app_test_mouse_event(struct app_test_win* win, NSEventType type, double2 pos_pt, NSEventModifierFlags modifiers) {
    NSWindow* ns_window = app_test_ns_window_(win);
    double2 local = pos_pt - app_window_get_pos(win->window);
    NSPoint location = NSMakePoint(local.x, ns_window.frame.size.height - local.y);
    return [NSEvent mouseEventWithType:type location:location modifierFlags:modifiers timestamp:NSProcessInfo.processInfo.systemUptime windowNumber:ns_window.windowNumber context:nil eventNumber:0 clickCount:1 pressure:type == NSEventTypeLeftMouseUp ? 0.0f : 1.0f];
}

void app_test_post_mouse_(struct app_test_win* win, NSEventType type, double2 pos_pt, NSEventModifierFlags modifiers) {
    [NSApp postEvent:app_test_mouse_event(win, type, pos_pt, modifiers) atStart:NO];
}

void app_test_drag_(struct app_test_win* win, double2 from_pt, double2 to_pt, uint32_t steps) {
    struct app* app = app_window_get_app(win->window);
    app_test_post_mouse_(win, NSEventTypeLeftMouseDown, from_pt, 0);
    __block uint32_t step = 0;
    NSTimer* timer = [NSTimer timerWithTimeInterval:1.0 / 120.0 repeats:YES block:^(NSTimer* t) {
        step++;
        double2 pos_pt = from_pt + (to_pt - from_pt) * ((double)step / (double)steps);
        if (step < steps) {
            app_test_post_mouse_(win, NSEventTypeLeftMouseDragged, pos_pt, 0);
        } else {
            app_test_post_mouse_(win, NSEventTypeLeftMouseUp, pos_pt, 0);
            [t invalidate];
        }
    }];
    [NSRunLoop.mainRunLoop addTimer:timer forMode:NSRunLoopCommonModes];
    while (timer.valid) {
        app_test_run_for_(app, 0.05);
    }
    app_test_run_for_(app, 0.3);
}

void app_test_hover_(struct app_test_win* win, double2 from_pt, double2 step_pt, uint32_t steps, double interval) {
    __block uint32_t step = 0;
    NSTimer* timer = [NSTimer timerWithTimeInterval:interval repeats:YES block:^(NSTimer* t) {
        step++;
        if (step > steps) {
            [t invalidate];
            return;
        }
        NSEvent* event = app_test_mouse_event(win, NSEventTypeMouseMoved, from_pt + step_pt * (double)step, 0);
        win->pending_event_ts = event.timestamp;
        [NSApp postEvent:event atStart:NO];
    }];
    [NSRunLoop.mainRunLoop addTimer:timer forMode:NSRunLoopCommonModes];
}

void app_test_post_key_(struct app_test_win* win, NSEventType type, const char* characters, uint16_t key_code, NSEventModifierFlags modifiers) {
    NSString* string = [NSString stringWithUTF8String:characters];
    NSEvent* event = [NSEvent keyEventWithType:type location:NSZeroPoint modifierFlags:modifiers timestamp:NSProcessInfo.processInfo.systemUptime windowNumber:app_test_ns_window_(win).windowNumber context:nil characters:string charactersIgnoringModifiers:string isARepeat:NO keyCode:key_code];
    [NSApp postEvent:event atStart:NO];
}

void app_test_scroll_(struct app_test_win* win, double2 pos_pt, int2 lines) {
    double2 size = app_window_get_size(win->window);
    double desktop_height = NSScreen.screens.firstObject.frame.size.height;
    CGEventRef cg_event = CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitLine, 2, lines.y, lines.x);
    CGEventSetLocation(cg_event, CGPointMake(pos_pt.x, desktop_height - (size.y - pos_pt.y)));
    NSEvent* event = [NSEvent eventWithCGEvent:cg_event];
    CFRelease(cg_event);
    [app_test_ns_window_(win).contentView scrollWheel:event];
}

#endif
