#include "cr.h"

#if cr_macos
#include "app_apple.h"
#import <AppKit/AppKit.h>

@interface AppView: NSView<CALayerDelegate, NSViewLayerContentScaleDelegate, NSTextInputClient, NSMenuItemValidation>
- (instancetype)initWithWindow:(struct app_window*)window frame:(NSRect)frame;
- (void)attach:(struct app_window*)window;
- (void)detach;
- (void)handleDisplayLink:(CADisplayLink*)link;
- (void)applyCursor:(enum app_cursor)cursor;
- (void)deleteLine:(id)sender;
@end

@interface AppWindow: NSWindow
@end

@interface AppWindowDelegate: NSObject<NSWindowDelegate>
- (instancetype)initWithWindow:(struct app_window*)window;
- (void)attach:(struct app_window*)window;
- (void)detach;
@end

@interface AppApplicationDelegate: NSObject<NSApplicationDelegate>
- (instancetype)initWithApp:(struct app*)app;
@end

struct app_window_native {
    struct app_apple_surface surface;
    AppWindow* ns_window;
    AppView* view;
    AppWindowDelegate* delegate;
    bool occluded;
};

struct app_native {
    struct app_apple_state state;
    AppApplicationDelegate* delegate;
    NSMenu* menu;
};

#define app_macos_device_flag_control_left ((NSEventModifierFlags)0x1)
#define app_macos_device_flag_shift_left ((NSEventModifierFlags)0x2)
#define app_macos_device_flag_shift_right ((NSEventModifierFlags)0x4)
#define app_macos_device_flag_command_left ((NSEventModifierFlags)0x8)
#define app_macos_device_flag_command_right ((NSEventModifierFlags)0x10)
#define app_macos_device_flag_alt_left ((NSEventModifierFlags)0x20)
#define app_macos_device_flag_alt_right ((NSEventModifierFlags)0x40)
#define app_macos_device_flag_control_right ((NSEventModifierFlags)0x2000)
#define app_macos_device_flag_all ((NSEventModifierFlags)0x207f)

static double app_macos_desktop_height(void) {
    NSScreen* screen = NSScreen.screens.firstObject;
    return screen ? screen.frame.size.height : 0.0;
}

static NSRect app_macos_frame(double2 pos_pt, double2 size_pt) {
    return NSMakeRect(pos_pt.x, app_macos_desktop_height() - pos_pt.y - size_pt.y, size_pt.x, size_pt.y);
}

static double2 app_macos_frame_pos(NSRect frame) {
    return make_double2(frame.origin.x, app_macos_desktop_height() - NSMaxY(frame));
}

static double2 app_macos_frame_size(NSRect frame) {
    return make_double2(frame.size.width, frame.size.height);
}

static bool app_macos_is_dark(NSAppearance* appearance) {
    NSAppearanceName name = [appearance bestMatchFromAppearancesWithNames:@[NSAppearanceNameAqua, NSAppearanceNameDarkAqua]];
    return [name isEqualToString:NSAppearanceNameDarkAqua];
}

static NSCursor* app_macos_cursor(enum app_cursor cursor) {
    return cursor == app_cursor_text ? NSCursor.IBeamCursor : NSCursor.arrowCursor;
}

static uint32_t app_macos_modifiers(NSEventModifierFlags flags) {
    return ((flags & NSEventModifierFlagShift) ? app_modifier_shift : 0u)
        | ((flags & NSEventModifierFlagControl) ? app_modifier_control : 0u)
        | ((flags & NSEventModifierFlagOption) ? app_modifier_alt : 0u)
        | ((flags & NSEventModifierFlagCommand) ? app_modifier_command : 0u);
}

static enum app_key app_macos_key(unsigned short key_code) {
    static const uint8_t hid[128] = {
        [0x00] = 0x04, [0x01] = 0x16, [0x02] = 0x07, [0x03] = 0x09, [0x04] = 0x0b, [0x05] = 0x0a, [0x06] = 0x1d, [0x07] = 0x1b,
        [0x08] = 0x06, [0x09] = 0x19, [0x0b] = 0x05, [0x0c] = 0x14, [0x0d] = 0x1a, [0x0e] = 0x08, [0x0f] = 0x15, [0x10] = 0x1c,
        [0x11] = 0x17, [0x12] = 0x1e, [0x13] = 0x1f, [0x14] = 0x20, [0x15] = 0x21, [0x16] = 0x23, [0x17] = 0x22, [0x18] = 0x2e,
        [0x19] = 0x26, [0x1a] = 0x24, [0x1b] = 0x2d, [0x1c] = 0x25, [0x1d] = 0x27, [0x1e] = 0x30, [0x1f] = 0x12, [0x20] = 0x18,
        [0x21] = 0x2f, [0x22] = 0x0c, [0x23] = 0x13, [0x24] = 0x28, [0x25] = 0x0f, [0x26] = 0x0d, [0x27] = 0x34, [0x28] = 0x0e,
        [0x29] = 0x33, [0x2a] = 0x31, [0x2b] = 0x36, [0x2c] = 0x38, [0x2d] = 0x11, [0x2e] = 0x10, [0x2f] = 0x37, [0x30] = 0x2b,
        [0x31] = 0x2c, [0x32] = 0x35, [0x33] = 0x2a, [0x35] = 0x29, [0x36] = 0xe7, [0x37] = 0xe3, [0x38] = 0xe1, [0x39] = 0x39, [0x3a] = 0xe2,
        [0x3b] = 0xe0, [0x3c] = 0xe5, [0x3d] = 0xe6, [0x3e] = 0xe4, [0x41] = 0x63, [0x43] = 0x55, [0x45] = 0x57, [0x4b] = 0x54,
        [0x4c] = 0x58, [0x4e] = 0x56, [0x52] = 0x62, [0x53] = 0x59, [0x54] = 0x5a, [0x55] = 0x5b, [0x56] = 0x5c, [0x57] = 0x5d,
        [0x58] = 0x5e, [0x59] = 0x5f, [0x5b] = 0x60, [0x5c] = 0x61, [0x60] = 0x3e, [0x61] = 0x3f, [0x62] = 0x40, [0x63] = 0x3c,
        [0x64] = 0x41, [0x65] = 0x42, [0x67] = 0x44, [0x6d] = 0x43, [0x6f] = 0x45, [0x73] = 0x4a, [0x74] = 0x4b, [0x75] = 0x4c,
        [0x76] = 0x3d, [0x77] = 0x4d, [0x78] = 0x3b, [0x79] = 0x4e, [0x7a] = 0x3a, [0x7b] = 0x50, [0x7c] = 0x4f, [0x7d] = 0x51,
        [0x7e] = 0x52,
    };
    return key_code < cr_count(hid) ? (enum app_key)hid[key_code] : app_key_none;
}

static bool app_macos_modifier_key_down(unsigned short key_code, NSEventModifierFlags flags) {
    static const struct {
        unsigned short key_code;
        NSEventModifierFlags device;
        NSEventModifierFlags generic;
    } map[] = {
        { 0x38, app_macos_device_flag_shift_left, NSEventModifierFlagShift },
        { 0x3c, app_macos_device_flag_shift_right, NSEventModifierFlagShift },
        { 0x3b, app_macos_device_flag_control_left, NSEventModifierFlagControl },
        { 0x3e, app_macos_device_flag_control_right, NSEventModifierFlagControl },
        { 0x3a, app_macos_device_flag_alt_left, NSEventModifierFlagOption },
        { 0x3d, app_macos_device_flag_alt_right, NSEventModifierFlagOption },
        { 0x37, app_macos_device_flag_command_left, NSEventModifierFlagCommand },
        { 0x36, app_macos_device_flag_command_right, NSEventModifierFlagCommand },
        { 0x39, 0, NSEventModifierFlagCapsLock },
    };
    for (uint32_t i = 0; i < cr_count(map); i++) {
        if (map[i].key_code != key_code) {
            continue;
        }
        if ((flags & map[i].device) != 0) {
            return true;
        }
        return (flags & map[i].generic) != 0 && (flags & app_macos_device_flag_all) == 0;
    }
    return false;
}

static void app_macos_update_paused(struct app_window* window) {
    struct app_window_native* native = window->native;
    native->surface.link.paused = window->hidden || native->occluded;
}

static NSMenu* app_macos_add_menu(NSMenu* main_menu, NSString* title) {
    NSMenuItem* item = [[[NSMenuItem alloc] init] autorelease];
    NSMenu* menu = [[[NSMenu alloc] initWithTitle:title] autorelease];
    item.submenu = menu;
    [main_menu addItem:item];
    return menu;
}

static void app_macos_build_menu(struct app* app) {
    cr_autoreleasepool {
        NSString* app_name = NSProcessInfo.processInfo.processName;
        NSMenu* main_menu = [[NSMenu alloc] init];

        NSMenu* app_menu = app_macos_add_menu(main_menu, @"");
        [app_menu addItemWithTitle:[@"About " stringByAppendingString:app_name] action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
        [app_menu addItem:NSMenuItem.separatorItem];
        [app_menu addItemWithTitle:[@"Hide " stringByAppendingString:app_name] action:@selector(hide:) keyEquivalent:@"h"];
        NSMenuItem* hide_others = [app_menu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
        hide_others.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagOption;
        [app_menu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];
        [app_menu addItem:NSMenuItem.separatorItem];
        [app_menu addItemWithTitle:[@"Quit " stringByAppendingString:app_name] action:@selector(terminate:) keyEquivalent:@"q"];

        NSMenu* file_menu = app_macos_add_menu(main_menu, @"File");
        [file_menu addItemWithTitle:@"Close" action:@selector(performClose:) keyEquivalent:@"w"];

        NSMenu* edit_menu = app_macos_add_menu(main_menu, @"Edit");
        [edit_menu addItemWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
        [edit_menu addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
        [edit_menu addItemWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
        [edit_menu addItem:NSMenuItem.separatorItem];
        [edit_menu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];
        [edit_menu addItem:NSMenuItem.separatorItem];
        [edit_menu addItemWithTitle:@"Delete Line" action:@selector(deleteLine:) keyEquivalent:@"K"];

        NSMenu* window_menu = app_macos_add_menu(main_menu, @"Window");
        [window_menu addItemWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
        [window_menu addItemWithTitle:@"Zoom" action:@selector(performZoom:) keyEquivalent:@""];

        NSApp.mainMenu = main_menu;
        NSApp.windowsMenu = window_menu;
        app->native->menu = main_menu;
    }
}

struct app_apple_state* app_apple_app_state(struct app* app) {
    return &app->native->state;
}

struct app_apple_surface* app_apple_window_surface(struct app_window* window) {
    return &window->native->surface;
}

uint32_t app_apple_window_max_fps(struct app_window* window) {
    return 0;
}

void app_platform_create(struct app* app) {
    struct app_native* native = cr_malloc(app->mem, sizeof(*native), 16, cr_loc);
    memset(native, 0, sizeof(*native));
    app->native = native;
    app->device = gpu_device_create(app->mem);

    [NSUserDefaults.standardUserDefaults setBool:true forKey:@"ApplePersistenceIgnoreState"];
    [NSUserDefaults.standardUserDefaults setBool:false forKey:@"NSQuitAlwaysKeepsWindows"];

    native->delegate = [[AppApplicationDelegate alloc] initWithApp:app];
    NSApplication.sharedApplication.delegate = native->delegate;
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
}

void app_platform_destroy(struct app* app) {
    struct app_native* native = app->native;
    app_apple_state_deinit(&native->state);
    NSApp.delegate = nil;
    [native->delegate release];
    [native->menu release];
    cr_free(app->mem, native, cr_loc);
}

void app_platform_run(struct app* app) {
    app_macos_build_menu(app);
    [NSApp run];
    app_finish(app);
}

void app_quit(struct app* app) {
    app->quit = true;
    [NSApp stop:nil];
    NSEvent* wake = [NSEvent otherEventWithType:NSEventTypeApplicationDefined location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0 context:nil subtype:0 data1:0 data2:0];
    [NSApp postEvent:wake atStart:YES];
}

void app_set_clipboard_text(struct app* app, const char* utf8, uint64_t size) {
    NSString* string = [[[NSString alloc] initWithBytes:utf8 length:size encoding:NSUTF8StringEncoding] autorelease];
    [NSPasteboard.generalPasteboard clearContents];
    [NSPasteboard.generalPasteboard setString:string forType:NSPasteboardTypeString];
}

static void app_macos_window_show(struct app_window* window) {
    app_macos_update_paused(window);
    app_apple_window_frame_sync(window);
    if (window->closed == false) {
        [window->native->ns_window makeKeyAndOrderFront:nil];
    }
}

static void app_macos_take_over(struct app_window* window, struct app_window* vacated, const struct app_window_desc* desc) {
    struct app_window_native* native = window->native;
    [native->view attach:window];
    [native->delegate attach:window];
    native->ns_window.delegate = native->delegate;
    if (desc->title) {
        native->ns_window.title = [NSString stringWithUTF8String:desc->title];
    }
    window->target_desc = vacated->target_desc;
    window->focused = vacated->focused;
    window->is_dark = vacated->is_dark;
    app_apple_surface_configure(&native->surface, window);
    app_platform_window_set_hidden(window, window->hidden);
}

void app_platform_window_create(struct app_window* window, const struct app_window_desc* desc, struct app_window* vacated) {
    struct app_window_native* native = cr_malloc(window->app->mem, sizeof(*native), 16, cr_loc);
    memset(native, 0, sizeof(*native));
    if (vacated) {
        window->native = vacated->native;
        vacated->native = native;
        app_macos_take_over(window, vacated, desc);
        return;
    }
    window->native = native;

    double2 size = all_long2((long2)(desc->size > 0.0)) ? desc->size : make_double2(1280.0, 800.0);

    NSWindowStyleMask style = desc->style == app_window_style_bordered
        ? NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskFullSizeContentView
        : NSWindowStyleMaskBorderless;
    if (desc->resizable) {
        style |= NSWindowStyleMaskResizable;
    }

    AppWindow* ns_window = [[AppWindow alloc] initWithContentRect:app_macos_frame(desc->pos, size) styleMask:style backing:NSBackingStoreBuffered defer:NO];
    if (desc->center) {
        [ns_window center];
    }
    ns_window.releasedWhenClosed = NO;
    ns_window.animationBehavior = NSWindowAnimationBehaviorNone;
    ns_window.restorable = NO;
    ns_window.acceptsMouseMovedEvents = YES;
    if (desc->style == app_window_style_bordered) {
        ns_window.titlebarAppearsTransparent = YES;
        ns_window.titleVisibility = NSWindowTitleHidden;
    }
    if (desc->style == app_window_style_transparent) {
        ns_window.opaque = NO;
        ns_window.backgroundColor = NSColor.clearColor;
        ns_window.hasShadow = NO;
    }
    if (desc->title) {
        ns_window.title = [NSString stringWithUTF8String:desc->title];
    }

    native->ns_window = ns_window;
    native->delegate = [[AppWindowDelegate alloc] initWithWindow:window];
    native->view = [[AppView alloc] initWithWindow:window frame:NSMakeRect(0.0, 0.0, size.x, size.y)];
    ns_window.delegate = native->delegate;
    ns_window.contentView = native->view;
    [ns_window makeFirstResponder:native->view];

    CADisplayLink* link = [native->view displayLinkWithTarget:native->view selector:@selector(handleDisplayLink:)];
    app_apple_surface_init(&native->surface, window, (CAMetalLayer*)native->view.layer, link);
    window->is_dark = app_macos_is_dark(ns_window.effectiveAppearance);

    if (window->hidden == false) {
        app_macos_window_show(window);
    }
}

void app_platform_window_close(struct app_window* window) {
    struct app_window_native* native = window->native;
    if (window->vacated) {
        native->surface.link.paused = YES;
    } else {
        [native->surface.link invalidate];
    }
    native->ns_window.delegate = nil;
}

void app_platform_window_destroy(struct app_window* window) {
    struct app_window_native* native = window->native;
    if (native->ns_window.firstResponder == native->view) {
        [native->ns_window makeFirstResponder:nil];
    }
    app_apple_surface_deinit(&native->surface);
    native->ns_window.delegate = nil;
    [native->delegate detach];
    [native->view detach];
    [native->ns_window orderOut:nil];
    [native->ns_window close];
    native->ns_window.contentView = nil;
    [native->view release];
    [native->delegate release];
    [native->ns_window release];
    cr_free(window->app->mem, native, cr_loc);
}

void* app_window_get_native(struct app_window* window) {
    return window->native->ns_window;
}

void app_window_set_pos(struct app_window* window, double2 pos_pt) {
    AppWindow* ns_window = window->native->ns_window;
    [ns_window setFrameOrigin:app_macos_frame(pos_pt, app_macos_frame_size(ns_window.frame)).origin];
}

double2 app_window_get_pos(struct app_window* window) {
    return app_macos_frame_pos(window->native->ns_window.frame);
}

void app_platform_window_set_size(struct app_window* window, double2 size_pt) {
    AppWindow* ns_window = window->native->ns_window;
    [ns_window setFrame:app_macos_frame(app_macos_frame_pos(ns_window.frame), size_pt) display:NO];
}

double2 app_window_get_size(struct app_window* window) {
    return app_macos_frame_size(window->native->ns_window.frame);
}

struct app_screen_desc app_window_get_screen(struct app_window* window) {
    NSScreen* screen = window->native->ns_window.screen;
    if (screen == nil) {
        screen = NSScreen.screens.firstObject;
    }
    if (screen == nil) {
        return (struct app_screen_desc) { 0 };
    }
    return (struct app_screen_desc) {
        .pos = app_macos_frame_pos(screen.frame),
        .size = app_macos_frame_size(screen.frame),
    };
}

void app_platform_window_set_hidden(struct app_window* window, bool hidden) {
    if (hidden) {
        app_macos_update_paused(window);
        [window->native->ns_window orderOut:nil];
    } else {
        app_macos_window_show(window);
    }
}

void app_window_focus(struct app_window* window) {
    [window->native->ns_window makeKeyAndOrderFront:nil];
}

bool app_window_get_focused(struct app_window* window) {
    return window->focused;
}

void app_window_order_front(struct app_window* window) {
    [window->native->ns_window orderFront:nil];
}

void app_window_order_back(struct app_window* window) {
    [window->native->ns_window orderBack:nil];
}

void app_platform_window_set_title(struct app_window* window, const char* title) {
    window->native->ns_window.title = [NSString stringWithUTF8String:title];
}

bool app_platform_window_get_target_size(struct app_window* window, double2* out_pt_size, uint2* out_px_size, double* out_to_px_from_pt) {
    struct app_window_native* native = window->native;
    if (native->surface.mtl_layer == NULL) {
        return false;
    }
    *out_pt_size = make_double2(native->view.bounds.size.width, native->view.bounds.size.height);
    *out_to_px_from_pt = native->ns_window.backingScaleFactor;
    *out_px_size = app_apple_px_size(*out_pt_size, *out_to_px_from_pt);
    return true;
}

void app_platform_window_set_cursor(struct app_window* window, enum app_cursor cursor) {
    [window->native->view applyCursor:cursor];
}

void app_platform_window_set_text_input_active(struct app_window* window, bool active) {
}

void app_platform_window_set_caret_rect(struct app_window* window, double2 origin_pt, double2 size_pt) {
}

@implementation AppWindow

- (BOOL)canBecomeKeyWindow {
    return YES;
}

- (BOOL)canBecomeMainWindow {
    return YES;
}

@end

@implementation AppWindowDelegate {
    struct app_window* _app_window;
}

- (instancetype)initWithWindow:(struct app_window*)window {
    self = [super init];
    _app_window = window;
    return self;
}

- (void)attach:(struct app_window*)window {
    _app_window = window;
}

- (void)detach {
    _app_window = NULL;
}

- (BOOL)windowShouldClose:(NSWindow*)sender {
    app_window_close_requested(_app_window);
    return NO;
}

- (void)windowDidChangeOcclusionState:(NSNotification*)notification {
    if (_app_window == NULL || _app_window->closed) {
        return;
    }
    NSWindow* ns_window = notification.object;
    _app_window->native->occluded = (ns_window.occlusionState & NSWindowOcclusionStateVisible) == 0;
    app_macos_update_paused(_app_window);
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
    if (_app_window) {
        _app_window->focused = true;
    }
}

- (void)windowDidResignKey:(NSNotification*)notification {
    if (_app_window) {
        _app_window->focused = false;
    }
}

- (void)windowWillStartLiveResize:(NSNotification*)notification {
    app_window_cancel_events(_app_window);
}

@end

@implementation AppApplicationDelegate {
    struct app* _app;
}

- (instancetype)initWithApp:(struct app*)app {
    self = [super init];
    _app = app;
    return self;
}

- (void)applicationDidFinishLaunching:(NSNotification*)notification {
    [NSApp activateIgnoringOtherApps:YES];
    app_ready(_app);
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender {
    app_quit_requested(_app);
    return NSTerminateCancel;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
    return NO;
}

@end

@implementation AppView {
    struct app_window* _app_window;
    NSString* _marked_text;
    NSTrackingArea* _tracking_area;
    bool _pointer_inside;
}

- (instancetype)initWithWindow:(struct app_window*)window frame:(NSRect)frame {
    self = [super initWithFrame:frame];
    _app_window = window;

    CAMetalLayer* mtl_layer = [[CAMetalLayer alloc] init];
    mtl_layer.contentsGravity = kCAGravityTopLeft;
    mtl_layer.delegate = self;
    self.wantsLayer = YES;
    self.layer = mtl_layer;
    self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawNever;
    [mtl_layer release];

    NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveAlways | NSTrackingInVisibleRect | NSTrackingCursorUpdate;
    _tracking_area = [[NSTrackingArea alloc] initWithRect:NSZeroRect options:options owner:self userInfo:nil];
    [self addTrackingArea:_tracking_area];
    return self;
}

- (void)dealloc {
    [_marked_text release];
    [_tracking_area release];
    [super dealloc];
}

- (void)attach:(struct app_window*)window {
    _app_window = window;
}

- (void)detach {
    _app_window = NULL;
}

- (void)handleDisplayLink:(CADisplayLink*)link {
    app_apple_window_frame(_app_window, link);
}

- (void)frameSynchronously {
    if (_app_window == NULL || app_apple_window_surface(_app_window)->mtl_layer == NULL) {
        return;
    }
    app_apple_window_frame_sync(_app_window);
    [CATransaction flush];
}

- (void)applyCursor:(enum app_cursor)cursor {
    if (_pointer_inside) {
        [app_macos_cursor(cursor) set];
    }
}

- (id<CAAction>)actionForLayer:(CALayer*)layer forKey:(NSString*)key {
    return (id<CAAction>)NSNull.null;
}

- (BOOL)layer:(CALayer*)layer shouldInheritContentsScale:(CGFloat)newScale fromWindow:(NSWindow*)window {
    return YES;
}

- (BOOL)wantsUpdateLayer {
    return YES;
}

- (void)updateLayer {
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent*)event {
    return YES;
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    [self frameSynchronously];
}

- (void)viewDidChangeBackingProperties {
    [super viewDidChangeBackingProperties];
    [self frameSynchronously];
}

- (void)viewDidChangeEffectiveAppearance {
    [super viewDidChangeEffectiveAppearance];
    if (_app_window) {
        _app_window->is_dark = app_macos_is_dark(self.effectiveAppearance);
    }
}

- (double2)pointerPosition:(NSEvent*)event {
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    return make_double2(p.x, self.bounds.size.height - p.y);
}

- (void)addPointerEvent:(NSEvent*)event type:(enum app_pointer_event_type)type button:(enum app_pointer_button)button {
    bool is_hover_edge = event.type == NSEventTypeMouseEntered || event.type == NSEventTypeMouseExited;
    bool is_pencil = is_hover_edge == false && event.subtype == NSEventSubtypeTabletPoint;
    bool touching = type == app_pointer_event_type_touch_began || type == app_pointer_event_type_touch_moved;
    struct app_event app_event = {
        .type = app_event_type_pointer,
        .pointer.type = type,
        .pointer.source = is_pencil ? app_pointer_source_pencil : app_pointer_source_indirect,
        .pointer.button = button,
        .pointer.id = 1,
        .pointer.modifiers = app_macos_modifiers(event.modifierFlags),
        .pointer.pressure = is_pencil ? (double)event.pressure : (touching ? 1.0 : 0.0),
        .pointer.pos = [self pointerPosition:event],
        .pointer.timestamp = event.timestamp,
    };
    app_window_add_event(_app_window, &app_event);
}

- (void)mouseEntered:(NSEvent*)event {
    _pointer_inside = true;
    [self addPointerEvent:event type:app_pointer_event_type_hover_began button:app_pointer_button_primary];
}

- (void)mouseMoved:(NSEvent*)event {
    [self addPointerEvent:event type:app_pointer_event_type_hover_moved button:app_pointer_button_primary];
}

- (void)mouseExited:(NSEvent*)event {
    _pointer_inside = false;
    [self addPointerEvent:event type:app_pointer_event_type_hover_ended button:app_pointer_button_primary];
}

- (void)cursorUpdate:(NSEvent*)event {
    [app_macos_cursor(_app_window ? _app_window->cursor : app_cursor_default) set];
}

- (void)mouseDown:(NSEvent*)event {
    [self addPointerEvent:event type:app_pointer_event_type_touch_began button:app_pointer_button_primary];
}

- (void)mouseDragged:(NSEvent*)event {
    [self addPointerEvent:event type:app_pointer_event_type_touch_moved button:app_pointer_button_primary];
}

- (void)mouseUp:(NSEvent*)event {
    [self addPointerEvent:event type:app_pointer_event_type_touch_ended button:app_pointer_button_primary];
}

- (void)rightMouseDown:(NSEvent*)event {
    [self addPointerEvent:event type:app_pointer_event_type_touch_began button:app_pointer_button_secondary];
}

- (void)rightMouseDragged:(NSEvent*)event {
    [self addPointerEvent:event type:app_pointer_event_type_touch_moved button:app_pointer_button_secondary];
}

- (void)rightMouseUp:(NSEvent*)event {
    [self addPointerEvent:event type:app_pointer_event_type_touch_ended button:app_pointer_button_secondary];
}

- (void)otherMouseDown:(NSEvent*)event {
    [self addPointerEvent:event type:app_pointer_event_type_touch_began button:app_pointer_button_middle];
}

- (void)otherMouseDragged:(NSEvent*)event {
    [self addPointerEvent:event type:app_pointer_event_type_touch_moved button:app_pointer_button_middle];
}

- (void)otherMouseUp:(NSEvent*)event {
    [self addPointerEvent:event type:app_pointer_event_type_touch_ended button:app_pointer_button_middle];
}

- (void)scrollWheel:(NSEvent*)event {
    double2 delta = make_double2(-event.scrollingDeltaX, -event.scrollingDeltaY);
    if (event.hasPreciseScrollingDeltas == false) {
        delta *= 13.0;
    }
    struct app_event app_event = {
        .type = app_event_type_scroll,
        .scroll.delta = delta,
        .scroll.pos = [self pointerPosition:event],
        .scroll.timestamp = event.timestamp,
    };
    app_window_add_event(_app_window, &app_event);
}

- (void)keyDown:(NSEvent*)event {
    app_window_add_key_event(_app_window, app_macos_key(event.keyCode), app_macos_modifiers(event.modifierFlags), true, event.isARepeat);
    if (_app_window && _app_window->text_input_active) {
        [self interpretKeyEvents:@[event]];
    }
}

- (void)keyUp:(NSEvent*)event {
    app_window_add_key_event(_app_window, app_macos_key(event.keyCode), app_macos_modifiers(event.modifierFlags), false, false);
}

- (void)flagsChanged:(NSEvent*)event {
    enum app_key key = app_macos_key(event.keyCode);
    if (key == app_key_none) {
        return;
    }
    bool down = app_macos_modifier_key_down(event.keyCode, event.modifierFlags);
    app_window_add_key_event(_app_window, key, app_macos_modifiers(event.modifierFlags), down, false);
}

- (NSString*)plainString:(id)string {
    return [string isKindOfClass:NSAttributedString.class] ? [(NSAttributedString*)string string] : string;
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
    [_marked_text release];
    _marked_text = nil;
    app_apple_window_add_text(_app_window, [self plainString:string]);
}

- (void)doCommandBySelector:(SEL)selector {
    const struct {
        SEL sel;
        enum app_text_command command;
        bool select;
    } map[] = {
        { @selector(moveLeft:), app_text_command_move_left, false },
        { @selector(moveRight:), app_text_command_move_right, false },
        { @selector(moveUp:), app_text_command_move_up, false },
        { @selector(moveDown:), app_text_command_move_down, false },
        { @selector(moveLeftAndModifySelection:), app_text_command_move_left, true },
        { @selector(moveRightAndModifySelection:), app_text_command_move_right, true },
        { @selector(moveUpAndModifySelection:), app_text_command_move_up, true },
        { @selector(moveDownAndModifySelection:), app_text_command_move_down, true },
        { @selector(moveWordLeft:), app_text_command_move_word_left, false },
        { @selector(moveWordRight:), app_text_command_move_word_right, false },
        { @selector(moveWordLeftAndModifySelection:), app_text_command_move_word_left, true },
        { @selector(moveWordRightAndModifySelection:), app_text_command_move_word_right, true },
        { @selector(moveWordBackward:), app_text_command_move_word_left, false },
        { @selector(moveWordForward:), app_text_command_move_word_right, false },
        { @selector(moveWordBackwardAndModifySelection:), app_text_command_move_word_left, true },
        { @selector(moveWordForwardAndModifySelection:), app_text_command_move_word_right, true },
        { @selector(moveToLeftEndOfLine:), app_text_command_move_line_start, false },
        { @selector(moveToRightEndOfLine:), app_text_command_move_line_end, false },
        { @selector(moveToLeftEndOfLineAndModifySelection:), app_text_command_move_line_start, true },
        { @selector(moveToRightEndOfLineAndModifySelection:), app_text_command_move_line_end, true },
        { @selector(moveToBeginningOfLine:), app_text_command_move_line_start, false },
        { @selector(moveToEndOfLine:), app_text_command_move_line_end, false },
        { @selector(moveToBeginningOfLineAndModifySelection:), app_text_command_move_line_start, true },
        { @selector(moveToEndOfLineAndModifySelection:), app_text_command_move_line_end, true },
        { @selector(moveToBeginningOfParagraph:), app_text_command_move_line_start, false },
        { @selector(moveToEndOfParagraph:), app_text_command_move_line_end, false },
        { @selector(moveToBeginningOfParagraphAndModifySelection:), app_text_command_move_line_start, true },
        { @selector(moveToEndOfParagraphAndModifySelection:), app_text_command_move_line_end, true },
        { @selector(moveToBeginningOfDocument:), app_text_command_move_doc_start, false },
        { @selector(moveToEndOfDocument:), app_text_command_move_doc_end, false },
        { @selector(moveToBeginningOfDocumentAndModifySelection:), app_text_command_move_doc_start, true },
        { @selector(moveToEndOfDocumentAndModifySelection:), app_text_command_move_doc_end, true },
        { @selector(deleteBackward:), app_text_command_delete_backward, false },
        { @selector(deleteForward:), app_text_command_delete_forward, false },
        { @selector(deleteWordBackward:), app_text_command_delete_word_backward, false },
        { @selector(deleteWordForward:), app_text_command_delete_word_forward, false },
        { @selector(deleteToBeginningOfLine:), app_text_command_delete_line_start, false },
        { @selector(deleteToEndOfLine:), app_text_command_delete_line_end, false },
        { @selector(deleteToBeginningOfParagraph:), app_text_command_delete_line_start, false },
        { @selector(deleteToEndOfParagraph:), app_text_command_delete_line_end, false },
        { @selector(pageUp:), app_text_command_move_page_up, false },
        { @selector(pageDown:), app_text_command_move_page_down, false },
        { @selector(pageUpAndModifySelection:), app_text_command_move_page_up, true },
        { @selector(pageDownAndModifySelection:), app_text_command_move_page_down, true },
        { @selector(scrollToBeginningOfDocument:), app_text_command_scroll_doc_start, false },
        { @selector(scrollToEndOfDocument:), app_text_command_scroll_doc_end, false },
        { @selector(scrollPageUp:), app_text_command_scroll_page_up, false },
        { @selector(scrollPageDown:), app_text_command_scroll_page_down, false },
        { @selector(insertNewline:), app_text_command_insert_newline, false },
        { @selector(insertLineBreak:), app_text_command_insert_newline, false },
        { @selector(insertNewlineIgnoringFieldEditor:), app_text_command_insert_newline, false },
        { @selector(insertTab:), app_text_command_insert_tab, false },
        { @selector(insertTabIgnoringFieldEditor:), app_text_command_insert_tab, false },
    };
    for (uint32_t i = 0; i < cr_count(map); i++) {
        if (selector == map[i].sel) {
            app_window_add_text_command_event(_app_window, map[i].command, map[i].select);
            return;
        }
    }
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange {
    NSString* text = [self plainString:string];
    [_marked_text release];
    _marked_text = text.length > 0 ? [text retain] : nil;
    app_apple_window_add_marked_text(_app_window, text, selectedRange.location);
}

- (void)unmarkText {
    [_marked_text release];
    _marked_text = nil;
    app_apple_window_add_marked_text(_app_window, @"", 0);
}

- (BOOL)hasMarkedText {
    return _marked_text != nil;
}

- (NSRange)markedRange {
    return _marked_text ? NSMakeRange(0, _marked_text.length) : NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange {
    return NSMakeRange(0, 0);
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
    return nil;
}

- (NSArray<NSAttributedStringKey>*)validAttributesForMarkedText {
    return @[];
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
    if (_app_window == NULL) {
        return NSZeroRect;
    }
    double2 origin = _app_window->caret_origin_pt;
    double2 size = _app_window->caret_size_pt;
    NSRect rect = NSMakeRect(origin.x, self.bounds.size.height - origin.y - size.y, size.x, size.y);
    return [self.window convertRectToScreen:[self convertRect:rect toView:nil]];
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
    return NSNotFound;
}

- (BOOL)validateMenuItem:(NSMenuItem*)item {
    return _app_window && _app_window->text_input_active;
}

- (void)copy:(id)sender {
    app_window_add_text_command_event(_app_window, app_text_command_copy, false);
}

- (void)cut:(id)sender {
    app_window_add_text_command_event(_app_window, app_text_command_cut, false);
}

- (void)paste:(id)sender {
    NSString* string = [NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString];
    if (string) {
        app_apple_window_add_text(_app_window, string);
    }
}

- (void)selectAll:(id)sender {
    app_window_add_text_command_event(_app_window, app_text_command_select_all, false);
}

- (void)deleteLine:(id)sender {
    app_window_add_text_command_event(_app_window, app_text_command_delete_line, false);
}

@end

#endif
