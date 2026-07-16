#include "cr.h"

#if cr_ios
#include "app_apple.h"
#import <UIKit/UIKit.h>
#include <stdio.h>

@interface AppTextPosition: UITextPosition
+ (AppTextPosition*)positionWithOffset:(NSInteger)offset;
@end

@interface AppTextRange: UITextRange
+ (AppTextRange*)rangeWithStart:(NSInteger)start end:(NSInteger)end;
@end

@interface AppView: UIView<UITextInput, UIScribbleInteractionDelegate>
- (instancetype)initWithWindow:(struct app_window*)window;
- (void)setAppWindow:(struct app_window*)window;
- (void)handleDisplayLink:(CADisplayLink*)link;
- (void)handleScrollPan:(UIPanGestureRecognizer*)pan;
@end

@interface AppViewController: UIViewController
- (instancetype)initWithWindow:(struct app_window*)window view:(AppView*)view;
- (void)setAppWindow:(struct app_window*)window;
@end

@interface AppSceneDelegate: UIResponder<UIWindowSceneDelegate>
@end

@interface AppApplicationDelegate: UIResponder<UIApplicationDelegate>
@end

@interface AppApplication: UIApplication
@end

struct app_window_native {
    struct app_apple_surface surface;
    UISceneSession* session;
    UIWindowScene* scene;
    UIWindow* ui_window;
    AppViewController* view_controller;
    AppView* view;
    NSString* title;
};

struct app_native {
    struct app_apple_state state;
    NSMutableArray<UIWindowScene*>* unclaimed_scenes;
};

static struct app* app_ios_app;

static struct app_window* app_ios_window_for_ui_window(struct app* app, UIWindow* ui_window) {
    for (struct app_window* window = app->windows; window; window = window->next) {
        if (window->native->ui_window == ui_window) {
            return window;
        }
    }
    return NULL;
}

static struct app_window* app_ios_window_for_scene(struct app* app, UIScene* scene) {
    for (struct app_window* window = app->windows; window; window = window->next) {
        if (window->native->scene == (UIWindowScene*)scene) {
            return window;
        }
    }
    return NULL;
}

static struct app_window* app_ios_window_for_session(struct app* app, UISceneSession* session) {
    for (struct app_window* window = app->windows; window; window = window->next) {
        if (window->closed == false && [window->native->session.persistentIdentifier isEqualToString:session.persistentIdentifier]) {
            return window;
        }
    }
    return NULL;
}

static struct app_window* app_ios_window_awaiting_scene(struct app* app) {
    struct app_window* oldest = NULL;
    for (struct app_window* window = app->windows; window; window = window->next) {
        if (window->closed == false && window->native->session == nil) {
            oldest = window;
        }
    }
    return oldest;
}

static bool app_ios_is_dark(UITraitCollection* traits) {
    return traits.userInterfaceStyle == UIUserInterfaceStyleDark;
}

static uint32_t app_ios_modifiers(UIKeyModifierFlags flags) {
    return ((flags & UIKeyModifierShift) ? app_modifier_shift : 0u)
        | ((flags & UIKeyModifierControl) ? app_modifier_control : 0u)
        | ((flags & UIKeyModifierAlternate) ? app_modifier_alt : 0u)
        | ((flags & UIKeyModifierCommand) ? app_modifier_command : 0u);
}

struct app_apple_state* app_apple_app_state(struct app* app) {
    return &app->native->state;
}

struct app_apple_surface* app_apple_window_surface(struct app_window* window) {
    return &window->native->surface;
}

uint32_t app_apple_window_max_fps(struct app_window* window) {
    return (uint32_t)window->native->scene.screen.maximumFramesPerSecond;
}

void app_platform_create(struct app* app) {
    struct app_native* native = cr_malloc(app->mem, sizeof(*native), 16, cr_loc);
    memset(native, 0, sizeof(*native));
    native->unclaimed_scenes = [[NSMutableArray alloc] init];
    app->native = native;
    app->device = gpu_device_create(app->mem);
    app_ios_app = app;
}

void app_platform_destroy(struct app* app) {
    app_apple_state_deinit(&app->native->state);
    [app->native->unclaimed_scenes release];
    app_ios_app = NULL;
    cr_free(app->mem, app->native, cr_loc);
}

void app_platform_run(struct app* app) {
    UIApplicationMain(app->desc.argc, app->desc.argv, NSStringFromClass(AppApplication.class), NSStringFromClass(AppApplicationDelegate.class));
}

void app_quit(struct app* app) {
    app->quit = true;
}

void app_set_clipboard_text(struct app* app, const char* utf8, uint64_t size) {
    UIPasteboard.generalPasteboard.string = [[[NSString alloc] initWithBytes:utf8 length:size encoding:NSUTF8StringEncoding] autorelease];
}

static void app_ios_set_title(struct app_window_native* native, const char* title) {
    [native->title release];
    native->title = title ? [[NSString alloc] initWithUTF8String:title] : nil;
    if (native->title) {
        native->scene.title = native->title;
    }
}

static void app_ios_show(struct app_window* window) {
    struct app_window_native* native = window->native;
    if (native->scene == nil) {
        return;
    }
    window->is_dark = app_ios_is_dark(native->view_controller.traitCollection);
    if (window->hidden) {
        native->ui_window.hidden = true;
    } else {
        [native->ui_window makeKeyAndVisible];
    }
    if (window->text_input_active) {
        [native->view becomeFirstResponder];
    } else {
        [native->view resignFirstResponder];
    }
    app_apple_window_frame_sync(window);
}

static void app_ios_attach(struct app_window* window, UIWindowScene* scene) {
    struct app_window_native* native = window->native;
    [native->session release];
    native->session = [scene.session retain];
    native->scene = [scene retain];
    native->ui_window = [[UIWindow alloc] initWithWindowScene:scene];
    native->view = [[AppView alloc] initWithWindow:window];
    native->view_controller = [[AppViewController alloc] initWithWindow:window view:native->view];

    CADisplayLink* link = [CADisplayLink displayLinkWithTarget:native->view selector:@selector(handleDisplayLink:)];
    app_apple_surface_init(&native->surface, window, (CAMetalLayer*)native->view.layer, link);

    native->ui_window.frame = scene.coordinateSpace.bounds;
    native->ui_window.rootViewController = native->view_controller;
    if (native->title) {
        scene.title = native->title;
    }
    app_ios_show(window);
}

static void app_ios_detach(struct app_window* window) {
    struct app_window_native* native = window->native;
    app_apple_surface_deinit(&native->surface);
    [native->view setAppWindow:NULL];
    [native->view_controller setAppWindow:NULL];
    native->ui_window.hidden = true;
    native->ui_window.rootViewController = nil;
    [native->view_controller release];
    [native->view release];
    [native->ui_window release];
    [native->scene release];
    native->view_controller = nil;
    native->view = nil;
    native->ui_window = nil;
    native->scene = nil;
}

static void app_ios_take_over(struct app_window* window, struct app_window* vacated, const struct app_window_desc* desc) {
    struct app_window_native* native = window->native;
    app_ios_set_title(native, desc->title);
    [native->view setAppWindow:window];
    [native->view_controller setAppWindow:window];
    if (native->scene) {
        window->target_desc = vacated->target_desc;
        app_apple_surface_configure(&native->surface, window);
    }
    app_ios_show(window);
}

void app_platform_window_create(struct app_window* window, const struct app_window_desc* desc, struct app_window* vacated) {
    struct app* app = window->app;
    struct app_window_native* native = cr_malloc(app->mem, sizeof(*native), 16, cr_loc);
    memset(native, 0, sizeof(*native));
    if (vacated) {
        window->native = vacated->native;
        vacated->native = native;
        app_ios_take_over(window, vacated, desc);
        return;
    }
    window->native = native;
    app_ios_set_title(native, desc->title);

    NSMutableArray<UIWindowScene*>* unclaimed = app->native->unclaimed_scenes;
    if (unclaimed.count > 0) {
        UIWindowScene* scene = [unclaimed.firstObject retain];
        [unclaimed removeObjectAtIndex:0];
        app_ios_attach(window, scene);
        [scene release];
    } else if (UIApplication.sharedApplication.supportsMultipleScenes) {
        [UIApplication.sharedApplication requestSceneSessionActivation:nil userActivity:nil options:nil errorHandler:^(NSError* error) {
            printf("scene activation failed: %s\n", error.localizedDescription.UTF8String);
        }];
    }
}

void app_platform_window_close(struct app_window* window) {
    window->native->surface.link.paused = YES;
}

void app_platform_window_destroy(struct app_window* window) {
    struct app_window_native* native = window->native;
    app_ios_detach(window);
    if (native->session) {
        [UIApplication.sharedApplication requestSceneSessionDestruction:native->session options:nil errorHandler:nil];
        [native->session release];
    }
    [native->title release];
    cr_free(window->app->mem, native, cr_loc);
}

void* app_window_get_native(struct app_window* window) {
    return window->native->ui_window;
}

void app_window_set_pos(struct app_window* window, double2 pos_pt) {
}

double2 app_window_get_pos(struct app_window* window) {
    return make_double2(0.0, 0.0);
}

void app_platform_window_set_size(struct app_window* window, double2 size_pt) {
}

double2 app_window_get_size(struct app_window* window) {
    UIWindow* ui_window = window->native->ui_window;
    return ui_window ? make_double2(ui_window.bounds.size.width, ui_window.bounds.size.height) : make_double2(0.0, 0.0);
}

struct app_screen_desc app_window_get_screen(struct app_window* window) {
    UIScreen* screen = window->native->scene.screen;
    if (screen == nil) {
        return (struct app_screen_desc) { 0 };
    }
    return (struct app_screen_desc) { .size = make_double2(screen.bounds.size.width, screen.bounds.size.height) };
}

void app_platform_window_set_hidden(struct app_window* window, bool hidden) {
    window->native->surface.link.paused = hidden;
    app_ios_show(window);
}

void app_window_focus(struct app_window* window) {
    [window->native->ui_window makeKeyWindow];
}

bool app_window_get_focused(struct app_window* window) {
    return window->native->ui_window.isKeyWindow;
}

void app_window_order_front(struct app_window* window) {
}

void app_window_order_back(struct app_window* window) {
}

void app_platform_window_set_title(struct app_window* window, const char* title) {
    app_ios_set_title(window->native, title);
}

bool app_platform_window_get_target_size(struct app_window* window, double2* out_pt_size, uint2* out_px_size, double* out_to_px_from_pt) {
    AppView* view = window->native->view;
    if (window->native->surface.mtl_layer == nil) {
        return false;
    }
    *out_pt_size = make_double2(view.bounds.size.width, view.bounds.size.height);
    *out_to_px_from_pt = view.traitCollection.displayScale;
    *out_px_size = app_apple_px_size(*out_pt_size, *out_to_px_from_pt);
    return true;
}

void app_platform_window_set_cursor(struct app_window* window, enum app_cursor cursor) {
}

void app_platform_window_set_text_input_active(struct app_window* window, bool active) {
    AppView* view = window->native->view;
    if (active) {
        [view becomeFirstResponder];
    } else {
        [view resignFirstResponder];
    }
}

void app_platform_window_set_caret_rect(struct app_window* window, double2 origin_pt, double2 size_pt) {
}

static enum app_pointer_source app_ios_pointer_source(UITouch* touch) {
    switch (touch.type) {
    case UITouchTypePencil: return app_pointer_source_pencil;
    case UITouchTypeIndirectPointer: return app_pointer_source_indirect;
    default: return app_pointer_source_direct;
    }
}

static bool app_ios_hover_event_type(UITouch* touch, enum app_pointer_event_type* out_type) {
    switch (touch.phase) {
    case UITouchPhaseRegionEntered: *out_type = app_pointer_event_type_hover_began; return true;
    case UITouchPhaseRegionMoved: *out_type = app_pointer_event_type_hover_moved; return true;
    case UITouchPhaseRegionExited: *out_type = app_pointer_event_type_hover_ended; return true;
    default: return false;
    }
}

static void app_ios_add_pointer_event(struct app_window* window, UITouch* touch, enum app_pointer_event_type type, uint32_t modifiers) {
    CGPoint location = [touch locationInView:window->native->view];
    bool touching = type == app_pointer_event_type_touch_began || type == app_pointer_event_type_touch_moved;
    bool has_force = touch.type == UITouchTypePencil && touch.maximumPossibleForce > 0.0;
    struct app_event event = {
        .type = app_event_type_pointer,
        .pointer.type = type,
        .pointer.source = app_ios_pointer_source(touch),
        .pointer.button = app_pointer_button_primary,
        .pointer.id = (uint64_t)touch,
        .pointer.modifiers = modifiers,
        .pointer.pressure = has_force ? touch.force / touch.maximumPossibleForce : (touching ? 1.0 : 0.0),
        .pointer.radius = touch.majorRadius,
        .pointer.pos = make_double2(location.x, location.y),
        .pointer.timestamp = touch.timestamp,
    };
    app_window_add_event(window, &event);
}

static void app_ios_handle_touches(struct app_window* window, NSSet<UITouch*>* touches, UIEvent* ui_event, enum app_pointer_event_type type) {
    if (window == NULL) {
        return;
    }
    uint32_t modifiers = app_ios_modifiers(ui_event.modifierFlags);
    for (UITouch* touch in touches) {
        app_ios_add_pointer_event(window, touch, type, modifiers);
    }
}

static void app_ios_handle_hover(struct app* app, UIEvent* ui_event) {
    uint32_t modifiers = app_ios_modifiers(ui_event.modifierFlags);
    for (UITouch* touch in ui_event.allTouches) {
        struct app_window* window = app_ios_window_for_ui_window(app, touch.window);
        enum app_pointer_event_type type;
        if (window && app_ios_hover_event_type(touch, &type)) {
            app_ios_add_pointer_event(window, touch, type, modifiers);
        }
    }
}

static bool app_ios_owns_hover(struct app* app, UIEvent* ui_event) {
    if (ui_event.allTouches.count == 0) {
        return false;
    }
    for (UITouch* touch in ui_event.allTouches) {
        if (app_ios_window_for_ui_window(app, touch.window) == NULL) {
            return false;
        }
    }
    return true;
}

static void app_ios_handle_keyboard_frame(struct app* app, NSNotification* notification) {
    NSNumber* is_local = notification.userInfo[UIKeyboardIsLocalUserInfoKey];
    NSValue* end_value = notification.userInfo[UIKeyboardFrameEndUserInfoKey];
    if (end_value == nil || (is_local && is_local.boolValue == false)) {
        return;
    }
    CGRect end_frame = end_value.CGRectValue;
    for (struct app_window* window = app->windows; window; window = window->next) {
        UIWindow* ui_window = window->native->ui_window;
        if (ui_window == nil) {
            continue;
        }
        if (CGRectIsEmpty(end_frame)) {
            window->keyboard_inset = 0.0;
            continue;
        }
        CGRect local = [ui_window convertRect:end_frame fromCoordinateSpace:ui_window.screen.coordinateSpace];
        double overlap = min_double(CGRectGetMaxY(ui_window.bounds) - CGRectGetMinY(local), local.size.height);
        window->keyboard_inset = max_double(overlap, 0.0);
    }
}

@implementation AppSceneDelegate

- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)connectionOptions {
    struct app* app = app_ios_app;
    UIWindowScene* window_scene = (UIWindowScene*)scene;
    struct app_window* window = app_ios_window_for_session(app, session);
    if (window == NULL) {
        window = app_ios_window_awaiting_scene(app);
    }
    if (window) {
        app_ios_attach(window, window_scene);
        return;
    }
    [app->native->unclaimed_scenes addObject:window_scene];
    if (app->ready) {
        app_window_requested(app);
    } else {
        app_ready(app);
    }
}

- (void)sceneDidDisconnect:(UIScene*)scene {
    struct app* app = app_ios_app;
    [app->native->unclaimed_scenes removeObjectIdenticalTo:(UIWindowScene*)scene];
    struct app_window* window = app_ios_window_for_scene(app, scene);
    if (window) {
        app_ios_detach(window);
    }
}

@end

@implementation AppApplicationDelegate

- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey, id>*)launchOptions {
    struct app* app = app_ios_app;
    NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
    [NSUserDefaults.standardUserDefaults setBool:false forKey:@"_UIConstraintBasedLayoutLogUnsatisfiable"];
    [center addObserverForName:UIApplicationDidEnterBackgroundNotification object:nil queue:nil usingBlock:^(NSNotification* notification) {
        for (struct app_window* window = app->windows; window; window = window->next) {
            app_window_cancel_events(window);
        }
        app->is_in_background = true;
    }];
    [center addObserverForName:UIApplicationWillEnterForegroundNotification object:nil queue:nil usingBlock:^(NSNotification* notification) {
        app->is_in_background = false;
    }];
    [center addObserverForName:UIKeyboardWillChangeFrameNotification object:nil queue:nil usingBlock:^(NSNotification* notification) {
        app_ios_handle_keyboard_frame(app, notification);
    }];
    return YES;
}

- (UISceneConfiguration*)application:(UIApplication*)application configurationForConnectingSceneSession:(UISceneSession*)connectingSceneSession options:(UISceneConnectionOptions*)options {
    UISceneConfiguration* configuration = [UISceneConfiguration configurationWithName:@"Default Configuration" sessionRole:connectingSceneSession.role];
    configuration.delegateClass = AppSceneDelegate.class;
    return configuration;
}

- (void)application:(UIApplication*)application didDiscardSceneSessions:(NSSet<UISceneSession*>*)sceneSessions {
    struct app* app = app_ios_app;
    for (UISceneSession* session in sceneSessions) {
        struct app_window* window = app_ios_window_for_session(app, session);
        if (window) {
            app_window_close_requested(window);
            app_window_destroy(window);
        }
    }
}

- (void)applicationWillTerminate:(UIApplication*)application {
    struct app* app = app_ios_app;
    if (app) {
        app_finish(app);
    }
}

@end

@implementation AppApplication

- (void)sendEvent:(UIEvent*)event {
    struct app* app = app_ios_app;
    if (app && event.type == UIEventTypeHover && app_ios_owns_hover(app, event)) {
        app_ios_handle_hover(app, event);
        return;
    }
    [super sendEvent:event];
}

@end

@implementation AppViewController {
    struct app_window* _app_window;
    AppView* _view;
}

- (instancetype)initWithWindow:(struct app_window*)window view:(AppView*)view {
    self = [super init];
    _app_window = window;
    _view = [view retain];
    return self;
}

- (void)dealloc {
    [_view release];
    [super dealloc];
}

- (void)setAppWindow:(struct app_window*)window {
    _app_window = window;
}

- (void)loadView {
    self.view = _view;
}

- (BOOL)prefersHomeIndicatorAutoHidden {
    return YES;
}

- (BOOL)prefersStatusBarHidden {
    return YES;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
    [super traitCollectionDidChange:previousTraitCollection];
    if (_app_window) {
        _app_window->is_dark = app_ios_is_dark(self.traitCollection);
    }
}

@end

@implementation AppTextPosition {
@public
    NSInteger _offset;
}

+ (AppTextPosition*)positionWithOffset:(NSInteger)offset {
    AppTextPosition* position = [[[AppTextPosition alloc] init] autorelease];
    position->_offset = offset;
    return position;
}

@end

@implementation AppTextRange {
@public
    NSInteger _start;
    NSInteger _end;
}

+ (AppTextRange*)rangeWithStart:(NSInteger)start end:(NSInteger)end {
    AppTextRange* range = [[[AppTextRange alloc] init] autorelease];
    range->_start = start;
    range->_end = end;
    return range;
}

- (UITextPosition*)start {
    return [AppTextPosition positionWithOffset:_start];
}

- (UITextPosition*)end {
    return [AppTextPosition positionWithOffset:_end];
}

- (BOOL)isEmpty {
    return _start == _end;
}

@end

static NSInteger app_ios_text_offset(UITextPosition* position) {
    return [position isKindOfClass:AppTextPosition.class] ? ((AppTextPosition*)position)->_offset : 0;
}

@implementation AppView {
    struct app_window* _app_window;
    NSString* _marked_text;
    NSDictionary<NSAttributedStringKey, id>* _marked_text_style;
    UITextInputStringTokenizer* _tokenizer;
    id<UITextInputDelegate> _input_delegate;
    CGSize _last_size;
}

+ (Class)layerClass {
    return CAMetalLayer.class;
}

- (instancetype)initWithWindow:(struct app_window*)window {
    self = [super init];
    _app_window = window;
    self.layer.contentsGravity = kCAGravityBottomLeft;
    self.multipleTouchEnabled = YES;
    self.inputAssistantItem.leadingBarButtonGroups = @[];
    self.inputAssistantItem.trailingBarButtonGroups = @[];

    UIPanGestureRecognizer* pan = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(handleScrollPan:)];
    pan.allowedScrollTypesMask = UIScrollTypeMaskAll;
    pan.allowedTouchTypes = @[];
    [self addGestureRecognizer:pan];
    [pan release];

    UIScribbleInteraction* scribble = [[UIScribbleInteraction alloc] initWithDelegate:self];
    [self addInteraction:scribble];
    [scribble release];
    return self;
}

- (BOOL)scribbleInteraction:(UIScribbleInteraction*)interaction shouldBeginAtLocation:(CGPoint)location {
    return NO;
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    app_ios_handle_touches(_app_window, touches, event, app_pointer_event_type_touch_began);
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    app_ios_handle_touches(_app_window, touches, event, app_pointer_event_type_touch_moved);
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    app_ios_handle_touches(_app_window, touches, event, app_pointer_event_type_touch_ended);
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    app_ios_handle_touches(_app_window, touches, event, app_pointer_event_type_canceled);
}

- (void)handleScrollPan:(UIPanGestureRecognizer*)pan {
    if (pan.state != UIGestureRecognizerStateBegan && pan.state != UIGestureRecognizerStateChanged) {
        return;
    }
    CGPoint translation = [pan translationInView:self];
    [pan setTranslation:CGPointZero inView:self];
    CGPoint location = [pan locationInView:self];
    struct app_event event = {
        .type = app_event_type_scroll,
        .scroll.delta = make_double2(-translation.x, -translation.y),
        .scroll.pos = make_double2(location.x, location.y),
        .scroll.timestamp = CACurrentMediaTime(),
    };
    app_window_add_event(_app_window, &event);
}

- (void)dealloc {
    [_marked_text release];
    [_marked_text_style release];
    [_tokenizer release];
    [super dealloc];
}

- (void)setAppWindow:(struct app_window*)window {
    _app_window = window;
}

- (void)handleDisplayLink:(CADisplayLink*)link {
    app_apple_window_frame(_app_window, link);
}

- (id<CAAction>)actionForLayer:(CALayer*)layer forKey:(NSString*)key {
    return (id<CAAction>)NSNull.null;
}

- (void)didMoveToWindow {
    [super didMoveToWindow];
    if (self.window) {
        self.layer.contentsScale = self.traitCollection.displayScale;
    }
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
    [super traitCollectionDidChange:previousTraitCollection];
    if (self.window && previousTraitCollection.displayScale != self.traitCollection.displayScale) {
        app_apple_window_frame_sync(_app_window);
    }
}

- (void)layoutSubviews {
    [super layoutSubviews];
    CGSize size = self.bounds.size;
    if (CGSizeEqualToSize(size, _last_size)) {
        return;
    }
    _last_size = size;
    if (self.window) {
        app_apple_window_frame_sync(_app_window);
    }
}

- (BOOL)canBecomeFirstResponder {
    return _app_window && _app_window->text_input_active;
}

- (BOOL)resignFirstResponder {
    if (_app_window && _app_window->text_input_active) {
        app_window_add_text_input_dismissed_event(_app_window);
    }
    return [super resignFirstResponder];
}

- (UIEditingInteractionConfiguration)editingInteractionConfiguration {
    return UIEditingInteractionConfigurationNone;
}

- (BOOL)hasText {
    return YES;
}

- (void)insertText:(NSString*)text {
    [_marked_text release];
    _marked_text = nil;
    if ([text isEqualToString:@"\n"]) {
        app_window_add_text_command_event(_app_window, app_text_command_insert_newline, false);
    } else {
        app_apple_window_add_text(_app_window, text);
    }
}

- (void)deleteBackward {
    app_window_add_text_command_event(_app_window, app_text_command_delete_backward, false);
}

- (NSString*)textInRange:(UITextRange*)range {
    return @"";
}

- (void)replaceRange:(UITextRange*)range withText:(NSString*)text {
    app_apple_window_add_text(_app_window, text);
}

- (UITextRange*)selectedTextRange {
    return [AppTextRange rangeWithStart:0 end:0];
}

- (void)setSelectedTextRange:(UITextRange*)range {
}

- (UITextRange*)markedTextRange {
    return _marked_text ? [AppTextRange rangeWithStart:0 end:(NSInteger)_marked_text.length] : nil;
}

- (NSDictionary<NSAttributedStringKey, id>*)markedTextStyle {
    return _marked_text_style;
}

- (void)setMarkedTextStyle:(NSDictionary<NSAttributedStringKey, id>*)style {
    if (_marked_text_style != style) {
        [_marked_text_style release];
        _marked_text_style = [style copy];
    }
}

- (void)setMarkedText:(NSString*)text selectedRange:(NSRange)selectedRange {
    if (text == nil) {
        text = @"";
    }
    [_marked_text release];
    _marked_text = text.length > 0 ? [text retain] : nil;
    app_apple_window_add_marked_text(_app_window, text, selectedRange.location);
}

- (void)unmarkText {
    if (_marked_text == nil) {
        return;
    }
    NSString* text = _marked_text;
    _marked_text = nil;
    app_apple_window_add_marked_text(_app_window, @"", 0);
    app_apple_window_add_text(_app_window, text);
    [text release];
}

- (UITextPosition*)beginningOfDocument {
    return [AppTextPosition positionWithOffset:0];
}

- (UITextPosition*)endOfDocument {
    return [AppTextPosition positionWithOffset:0];
}

- (UITextRange*)textRangeFromPosition:(UITextPosition*)from toPosition:(UITextPosition*)to {
    return [AppTextRange rangeWithStart:app_ios_text_offset(from) end:app_ios_text_offset(to)];
}

- (UITextPosition*)positionFromPosition:(UITextPosition*)position offset:(NSInteger)offset {
    return [AppTextPosition positionWithOffset:app_ios_text_offset(position)];
}

- (UITextPosition*)positionFromPosition:(UITextPosition*)position inDirection:(UITextLayoutDirection)direction offset:(NSInteger)offset {
    return [AppTextPosition positionWithOffset:app_ios_text_offset(position)];
}

- (NSComparisonResult)comparePosition:(UITextPosition*)position toPosition:(UITextPosition*)other {
    NSInteger a = app_ios_text_offset(position);
    NSInteger b = app_ios_text_offset(other);
    return a < b ? NSOrderedAscending : (a > b ? NSOrderedDescending : NSOrderedSame);
}

- (NSInteger)offsetFromPosition:(UITextPosition*)from toPosition:(UITextPosition*)to {
    return app_ios_text_offset(to) - app_ios_text_offset(from);
}

- (UITextPosition*)positionWithinRange:(UITextRange*)range farthestInDirection:(UITextLayoutDirection)direction {
    bool leading = direction == UITextLayoutDirectionLeft || direction == UITextLayoutDirectionUp;
    return leading ? range.start : range.end;
}

- (UITextRange*)characterRangeByExtendingPosition:(UITextPosition*)position inDirection:(UITextLayoutDirection)direction {
    NSInteger offset = app_ios_text_offset(position);
    return [AppTextRange rangeWithStart:offset end:offset];
}

- (NSWritingDirection)baseWritingDirectionForPosition:(UITextPosition*)position inDirection:(UITextStorageDirection)direction {
    return NSWritingDirectionLeftToRight;
}

- (void)setBaseWritingDirection:(NSWritingDirection)writingDirection forRange:(UITextRange*)range {
}

- (CGRect)caretRectForPosition:(UITextPosition*)position {
    if (_app_window == NULL) {
        return CGRectZero;
    }
    double2 origin = _app_window->caret_origin_pt;
    double2 size = _app_window->caret_size_pt;
    return CGRectMake(origin.x, origin.y, size.x, size.y);
}

- (CGRect)firstRectForRange:(UITextRange*)range {
    return [self caretRectForPosition:range.start];
}

- (NSArray<UITextSelectionRect*>*)selectionRectsForRange:(UITextRange*)range {
    return @[];
}

- (UITextPosition*)closestPositionToPoint:(CGPoint)point {
    return [AppTextPosition positionWithOffset:0];
}

- (UITextPosition*)closestPositionToPoint:(CGPoint)point withinRange:(UITextRange*)range {
    return range.start;
}

- (UITextRange*)characterRangeAtPoint:(CGPoint)point {
    return [AppTextRange rangeWithStart:0 end:0];
}

- (id<UITextInputTokenizer>)tokenizer {
    if (_tokenizer == nil) {
        _tokenizer = [[UITextInputStringTokenizer alloc] initWithTextInput:self];
    }
    return _tokenizer;
}

- (id<UITextInputDelegate>)inputDelegate {
    return _input_delegate;
}

- (void)setInputDelegate:(id<UITextInputDelegate>)delegate {
    _input_delegate = delegate;
}

- (UIView*)textInputView {
    return self;
}

- (void)beginFloatingCursorAtPoint:(CGPoint)point {
    app_window_add_floating_caret_event(_app_window, app_floating_caret_phase_began, make_double2(point.x, point.y));
}

- (void)updateFloatingCursorAtPoint:(CGPoint)point {
    app_window_add_floating_caret_event(_app_window, app_floating_caret_phase_moved, make_double2(point.x, point.y));
}

- (void)endFloatingCursor {
    app_window_add_floating_caret_event(_app_window, app_floating_caret_phase_ended, make_double2(0.0, 0.0));
}

- (UITextAutocorrectionType)autocorrectionType {
    return UITextAutocorrectionTypeNo;
}

- (UITextAutocapitalizationType)autocapitalizationType {
    return UITextAutocapitalizationTypeNone;
}

- (UITextSmartQuotesType)smartQuotesType {
    return UITextSmartQuotesTypeNo;
}

- (UITextSmartDashesType)smartDashesType {
    return UITextSmartDashesTypeNo;
}

- (UITextSpellCheckingType)spellCheckingType {
    return UITextSpellCheckingTypeNo;
}

- (UITextSmartInsertDeleteType)smartInsertDeleteType {
    return UITextSmartInsertDeleteTypeNo;
}

- (UITextInlinePredictionType)inlinePredictionType API_AVAILABLE(ios(17.0)) {
    return UITextInlinePredictionTypeNo;
}

- (UIWritingToolsBehavior)writingToolsBehavior API_AVAILABLE(ios(18.0)) {
    return UIWritingToolsBehaviorNone;
}

- (BOOL)canPerformAction:(SEL)action withSender:(id)sender {
    if (action == @selector(copy:) || action == @selector(cut:) || action == @selector(paste:) || action == @selector(selectAll:)) {
        return _app_window && _app_window->text_input_active;
    }
    return [super canPerformAction:action withSender:sender];
}

- (void)copy:(id)sender {
    app_window_add_text_command_event(_app_window, app_text_command_copy, false);
}

- (void)cut:(id)sender {
    app_window_add_text_command_event(_app_window, app_text_command_cut, false);
}

- (void)paste:(id)sender {
    NSString* string = UIPasteboard.generalPasteboard.string;
    if (string) {
        app_apple_window_add_text(_app_window, string);
    }
}

- (void)selectAll:(id)sender {
    app_window_add_text_command_event(_app_window, app_text_command_select_all, false);
}

- (bool)handleKey:(UIKey*)key {
    UIKeyModifierFlags mods = key.modifierFlags;
    bool select = (mods & UIKeyModifierShift) != 0;
    bool alt = (mods & UIKeyModifierAlternate) != 0;
    bool cmd = (mods & UIKeyModifierCommand) != 0;
    enum app_text_command command;
    switch (key.keyCode) {
    case UIKeyboardHIDUsageKeyboardLeftArrow: command = cmd ? app_text_command_move_line_start : (alt ? app_text_command_move_word_left : app_text_command_move_left); break;
    case UIKeyboardHIDUsageKeyboardRightArrow: command = cmd ? app_text_command_move_line_end : (alt ? app_text_command_move_word_right : app_text_command_move_right); break;
    case UIKeyboardHIDUsageKeyboardUpArrow: command = cmd ? app_text_command_move_doc_start : app_text_command_move_up; break;
    case UIKeyboardHIDUsageKeyboardDownArrow: command = cmd ? app_text_command_move_doc_end : app_text_command_move_down; break;
    case UIKeyboardHIDUsageKeyboardHome: command = cmd ? app_text_command_move_doc_start : app_text_command_move_line_start; break;
    case UIKeyboardHIDUsageKeyboardEnd: command = cmd ? app_text_command_move_doc_end : app_text_command_move_line_end; break;
    case UIKeyboardHIDUsageKeyboardDeleteForward: command = app_text_command_delete_forward; select = false; break;
    default: {
        if (cmd && select && [key.charactersIgnoringModifiers.lowercaseString isEqualToString:@"k"]) {
            command = app_text_command_delete_line;
            select = false;
            break;
        }
        return false;
    }
    }
    app_window_add_text_command_event(_app_window, command, select);
    return true;
}

- (void)pressesBegan:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event {
    NSMutableSet<UIPress*>* unhandled = [NSMutableSet set];
    for (UIPress* press in presses) {
        UIKey* key = press.key;
        if (key) {
            app_window_add_key_event(_app_window, (enum app_key)key.keyCode, app_ios_modifiers(key.modifierFlags), true, false);
        }
        bool handled = key != nil && _app_window && _app_window->text_input_active && [self handleKey:key];
        if (handled == false) {
            [unhandled addObject:press];
        }
    }
    if (unhandled.count > 0) {
        [super pressesBegan:unhandled withEvent:event];
    }
}

- (void)pressesEnded:(NSSet<UIPress*>*)presses withEvent:(UIPressesEvent*)event {
    for (UIPress* press in presses) {
        UIKey* key = press.key;
        if (key) {
            app_window_add_key_event(_app_window, (enum app_key)key.keyCode, app_ios_modifiers(key.modifierFlags), false, false);
        }
    }
    [super pressesEnded:presses withEvent:event];
}

@end

#endif
