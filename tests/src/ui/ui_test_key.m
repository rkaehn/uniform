#include "ui_tests.h"
#include "ui_gestures.h"

static void ui_test_check(bool ok, const char* label) {
    ui_test_group_check_("key", ok, label);
}

static bool ui_test_key_near(double2 v, double x, double y) {
    return fabs(v.x - x) < 0.01 && fabs(v.y - y) < 0.01;
}

struct ui_test_key_tree {
    struct ui_node root;
    struct ui_node a;
    struct ui_node b;
    struct ui_node keyed;
    struct ui_node keyed_child;
    struct ui_node after;
};

static struct ui_test_key_tree ui_test_key_build(bool insert_before, bool with_keyed, struct ui_key after_key) {
    struct ui_test_key_tree t = { 0 };

    t.root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(200.0, 200.0),
    });

    if (insert_before) {
        ui_frame(&(struct ui_frame) {
            .parent = t.root,
            .size = ui_size_fix(10.0, 10.0),
        });
    }

    t.a = ui_frame(&(struct ui_frame) {
        .parent = t.root,
        .size = ui_size_fix(10.0, 10.0),
    });

    t.b = ui_rect(&(struct ui_rect) {
        .parent = t.root,
        .size = ui_size_fix(10.0, 10.0),
    });

    if (with_keyed) {
        t.keyed = ui_frame(&(struct ui_frame) {
            .parent = t.root,
            .key = ui_key_u64(7),
            .size = ui_size_fix(10.0, 10.0),
        });

        t.keyed_child = ui_ellipse(&(struct ui_ellipse) {
            .parent = t.keyed,
            .size = ui_size_fix(10.0, 10.0),
        });
    }

    t.after = ui_frame(&(struct ui_frame) {
        .parent = t.root,
        .key = after_key,
        .size = ui_size_fix(10.0, 10.0),
    });

    return t;
}

static void ui_test_key_structure(void) {
    uint32_t before = ui_get_node_alloc_count();

    ui_test_check(ui_key_is_explicit(ui_key("x")) && ui_key("x").raw == ui_key("x").raw && ui_key("x").raw != ui_key("y").raw, "ui_key is explicit and repeatable");
    ui_test_check(ui_key_is_explicit(ui_key_u64(0)) && ui_key_is_explicit(ui_key_auto) == false, "ui_key_u64 zero is explicit, ui_key_auto is not");
    ui_test_check(ui_key_u64(1ull << 52).raw == ui_key_u64(0).raw && ui_key_u64(0).raw != 0 && ui_key_u64(uint64_max).raw < (1ull << 53), "keys pack 52 bits and the explicit flag into 53 bits");

    struct ui_test_key_tree t1 = ui_test_key_build(false, true, ui_key_auto);
    struct ui_test_key_tree t2 = ui_test_key_build(false, true, ui_key_auto);

    uint64_t root = ui_get_key(t1.root);
    uint64_t a = ui_get_key(t1.a);
    uint64_t b = ui_get_key(t1.b);
    uint64_t keyed = ui_get_key(t1.keyed);
    uint64_t keyed_child = ui_get_key(t1.keyed_child);
    uint64_t after = ui_get_key(t1.after);

    ui_test_check(root != 0 && a != 0 && b != 0 && keyed != 0 && keyed_child != 0 && after != 0, "every node gets a non-zero key");
    ui_test_check(root != a && a != b && b != keyed && keyed != keyed_child && keyed_child != after && a != after && b != after && root != after, "keys within a tree are distinct");
    ui_test_check(ui_get_key(ui_node_null) == 0, "null node has key zero");

    ui_test_check(root == ui_get_key(t2.root) && a == ui_get_key(t2.a) && b == ui_get_key(t2.b), "auto keys repeat across rebuilds");
    ui_test_check(keyed == ui_get_key(t2.keyed) && keyed_child == ui_get_key(t2.keyed_child) && after == ui_get_key(t2.after), "keyed subtree repeats across rebuilds");

    struct ui_node zero_named = ui_node(t1.root, ui_key_u64(0));
    struct ui_node one_named = ui_node(t1.root, ui_key_u64(1));
    ui_test_check(ui_get_key(zero_named) != a && ui_get_key(one_named) != b, "explicit keys never collide with auto ordinals");

    struct ui_node nested = ui_frame(&(struct ui_frame) {
        .parent = t1.a,
        .key = ui_key_u64(7),
        .size = ui_size_fix(10.0, 10.0),
    });
    ui_test_check(ui_get_key(nested) != 0 && ui_get_key(nested) != keyed && ui_get_key(nested) != a, "same explicit key under another parent is distinct");

    struct ui_node zero = ui_frame(&(struct ui_frame) {
        .parent = t1.b,
        .key = ui_key_u64(0),
        .size = ui_size_fix(10.0, 10.0),
    });
    struct ui_node one = ui_frame(&(struct ui_frame) {
        .parent = t1.b,
        .key = ui_key_u64(1),
        .size = ui_size_fix(10.0, 10.0),
    });
    struct ui_node zero_auto = ui_frame(&(struct ui_frame) {
        .parent = t1.b,
        .size = ui_size_fix(10.0, 10.0),
    });
    ui_test_check(ui_get_key(zero) != 0 && ui_get_key(zero) != ui_get_key(one) && ui_get_key(zero) != ui_get_key(zero_auto), "explicit key zero is a real key");

    struct ui_test_key_tree t3 = ui_test_key_build(false, false, ui_key_auto);
    ui_test_check(ui_get_key(t3.after) == after, "keyed siblings do not shift auto ordinals");

    struct ui_test_key_tree t4 = ui_test_key_build(true, true, ui_key_auto);
    ui_test_check(ui_get_key(t4.a) != a && ui_get_key(t4.after) != after, "an inserted auto sibling shifts later auto keys");
    ui_test_check(ui_get_key(t4.keyed) == keyed && ui_get_key(t4.keyed_child) == keyed_child, "explicit key and its subtree survive an inserted sibling");

    struct ui_test_key_tree t5 = ui_test_key_build(false, true, ui_key("after"));
    struct ui_test_key_tree t6 = ui_test_key_build(true, true, ui_key("after"));
    ui_test_check(ui_get_key(t5.after) == ui_get_key(t6.after) && ui_get_key(t5.after) != after, "explicit key survives an inserted sibling");

    struct ui_node keyed_root = ui_frame(&(struct ui_frame) {
        .key = ui_key_u64(9),
        .size = ui_size_fix(200.0, 200.0),
    });
    struct ui_node keyed_root_again = ui_node(ui_node_null, ui_key_u64(9));
    ui_test_check(ui_get_key(keyed_root) == ui_get_key(keyed_root_again) && ui_get_key(keyed_root) != root, "root accepts an explicit key");

    struct ui_node line = ui_line(&(struct ui_line) {
        .parent = t1.keyed,
        .key = ui_key_u64(3),
        .size = ui_size_fix(10.0, 1.0),
    });
    struct ui_node shape = ui_shape(&(struct ui_shape) {
        .parent = t1.after,
        .key = ui_key_u64(3),
        .size = ui_size_fix(10.0, 10.0),
    });
    struct ui_node text = ui_text(&(struct ui_text) {
        .parent = t1.keyed,
        .key = ui_key_u64(4),
        .str = ui_str("k", ui_black, 12.0),
    });
    struct ui_node custom = ui_custom(&(struct ui_custom) {
        .parent = t1.a,
        .key = ui_key_u64(3),
        .size = ui_size_fix(10.0, 10.0),
    });
    ui_test_check(ui_get_key(line) != 0 && ui_get_key(shape) != 0 && ui_get_key(text) != 0 && ui_get_key(custom) != 0 &&
                  ui_get_key(line) != ui_get_key(shape) && ui_get_key(line) != ui_get_key(text) && ui_get_key(line) != ui_get_key(custom), "every node type takes a key");

    ui_test_check(ui_node_is_equal(ui_get_root(t1.keyed_child), t1.root) && ui_node_is_equal(ui_get_root(line), t1.root) && ui_node_is_equal(ui_get_root(t1.root), t1.root) && ui_node_is_null(ui_get_root(ui_node_null)), "ui_get_root walks up to the root");

    ui_release(t1.root);
    ui_release(t2.root);
    ui_release(t3.root);
    ui_release(t4.root);
    ui_release(t5.root);
    ui_release(t6.root);
    ui_release(keyed_root);
    ui_release(keyed_root_again);

    ui_test_check(ui_get_node_alloc_count() == before, "structure test releases every node");
}

static void ui_test_key_declared(void) {
    uint32_t before = ui_get_node_alloc_count();

    struct ui_test_key_tree ref = ui_test_key_build(false, true, ui_key_auto);
    uint64_t root_key = ui_get_key(ref.root);
    uint64_t a_key = ui_get_key(ref.a);
    uint64_t b_key = ui_get_key(ref.b);
    uint64_t keyed_key = ui_get_key(ref.keyed);
    uint64_t keyed_child_key = ui_get_key(ref.keyed_child);
    uint64_t after_key = ui_get_key(ref.after);
    ui_release(ref.root);

    struct ui_node root = ui_node(ui_node_null, ui_key_auto);
    struct ui_node a = ui_node(root, ui_key_auto);
    struct ui_node b = ui_node(root, ui_key_auto);
    struct ui_node keyed = ui_node(root, ui_key_u64(7));
    struct ui_node keyed_child = ui_node(keyed, ui_key_auto);
    struct ui_node after = ui_node(root, ui_key_auto);
    struct ui_node line = ui_node(root, ui_key_auto);
    struct ui_node shape = ui_node(root, ui_key_auto);
    struct ui_node custom = ui_node(root, ui_key_auto);

    ui_test_check(ui_get_key(root) == root_key && ui_get_key(a) == a_key && ui_get_key(b) == b_key &&
                  ui_get_key(keyed) == keyed_key && ui_get_key(keyed_child) == keyed_child_key && ui_get_key(after) == after_key, "a declared node takes the key its content would have taken");
    ui_test_check(ui_is_alive(a) && ui_node_is_equal(ui_get_root(keyed_child), root), "a declared node is attached to its parent");

    struct ui_node early = ui_frame(&(struct ui_frame) {
        .parent = a,
        .pos = ui_pos_xy(0.0, 0.0),
        .size = ui_size_fix(12.0, 8.0),
    });

    struct ui_node filled = ui_frame(&(struct ui_frame) {
        .node = root,
        .size = ui_size_fix(200.0, 200.0),
    });
    ui_test_check(ui_node_is_equal(filled, root) && ui_get_key(root) == root_key, "content keeps the declared node and its key");

    ui_test_check(ui_node_is_equal(ui_frame(&(struct ui_frame) { .node = a, .size = ui_size_fix(20.0, 20.0) }), a), "a frame fills a declared node");
    ui_test_check(ui_node_is_equal(ui_rect(&(struct ui_rect) { .node = b, .size = ui_size_fix(5.0, 6.0) }), b), "a rect fills a declared node");
    ui_test_check(ui_node_is_equal(ui_frame(&(struct ui_frame) { .node = keyed, .size = ui_size_fix(7.0, 7.0) }), keyed), "a keyed frame fills a declared node");
    ui_test_check(ui_node_is_equal(ui_text(&(struct ui_text) { .node = keyed_child, .str = ui_str("k", ui_black, 12.0) }), keyed_child), "a text fills a declared node");
    ui_test_check(ui_node_is_equal(ui_ellipse(&(struct ui_ellipse) { .node = after, .size = ui_size_fix(9.0, 9.0) }), after), "an ellipse fills a declared node");
    ui_test_check(ui_node_is_equal(ui_line(&(struct ui_line) { .node = line, .size = ui_size_fix(10.0, 1.0) }), line), "a line fills a declared node");
    ui_test_check(ui_node_is_equal(ui_shape(&(struct ui_shape) { .node = shape, .size = ui_size_fix(10.0, 10.0) }), shape), "a shape fills a declared node");
    ui_test_check(ui_node_is_equal(ui_custom(&(struct ui_custom) { .node = custom, .size = ui_size_fix(10.0, 10.0) }), custom), "a custom fills a declared node");

    ui_layout(root);

    ui_test_check(ui_test_key_near(ui_get_size(b), 5.0, 6.0) && ui_test_key_near(ui_get_size(after), 9.0, 9.0), "content given through a declared node lays out");
    ui_test_check(ui_test_key_near(ui_get_size(a), 20.0, 20.0) && ui_test_key_near(ui_get_size(early), 12.0, 8.0) && ui_node_is_equal(ui_get_root(early), root), "a child added before the content stays under the node");

    ui_release(root);

    ui_test_check(ui_get_node_alloc_count() == before, "declared test releases every node");
}

static void ui_test_key_derive(void) {
    uint32_t before = ui_get_node_alloc_count();

    struct ui_node root = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(200.0, 200.0),
    });
    struct ui_node child = ui_frame(&(struct ui_frame) {
        .parent = root,
        .size = ui_size_fix(10.0, 10.0),
    });
    struct ui_node named_child = ui_frame(&(struct ui_frame) {
        .parent = root,
        .key = ui_key("named"),
        .size = ui_size_fix(10.0, 10.0),
    });
    uint64_t first = ui_derive_key(root, ui_key_auto);
    uint64_t second = ui_derive_key(root, ui_key_auto);
    struct ui_node later = ui_frame(&(struct ui_frame) {
        .parent = root,
        .size = ui_size_fix(10.0, 10.0),
    });
    uint64_t named = ui_derive_key(root, ui_key("named"));

    ui_test_check(first != 0 && second != 0 && first != second, "anonymous derived keys are distinct");
    ui_test_check(first != ui_get_key(child) && second != ui_get_key(later) && first != ui_get_key(later) && second != ui_get_key(child), "anonymous derived keys do not collide with child keys");
    ui_test_check(named != 0 && named == ui_derive_key(root, ui_key("named")) && named != first && named != second, "a named derived key is repeatable");
    ui_test_check(named != ui_get_key(named_child) && named != ui_get_key(root), "a named derived key does not collide with a child of the same name");
    ui_test_check(ui_derive_key(child, ui_key("named")) != named, "the same name under another node derives a distinct key");

    struct ui_node root2 = ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(200.0, 200.0),
    });
    struct ui_node child2 = ui_frame(&(struct ui_frame) {
        .parent = root2,
        .size = ui_size_fix(10.0, 10.0),
    });
    uint64_t first2 = ui_derive_key(root2, ui_key_auto);
    struct ui_node later2 = ui_frame(&(struct ui_frame) {
        .parent = root2,
        .size = ui_size_fix(10.0, 10.0),
    });

    ui_test_check(first2 == first && ui_get_key(child2) == ui_get_key(child), "anonymous derived keys repeat across rebuilds");
    ui_test_check(ui_get_key(later2) == ui_get_key(later), "anonymous derived keys do not shift child ordinals");

    ui_release(root);
    ui_release(root2);

    ui_test_check(ui_get_node_alloc_count() == before, "derive test releases every node");
}

static void ui_test_key_on_tap(struct ui_tap_gesture gesture, void* user) {
    (*(uint32_t*)user)++;
}

static void ui_test_key_on_pointer(struct app_pointer_event event, struct ui_pointer pointer, void* user) {
    if (event.type == app_pointer_event_type_touch_ended) {
        (*(uint32_t*)user)++;
    }
}

static void ui_test_key_on_event(const struct app_event* event, void* user) {
}

static struct ui_node ui_test_key_root(void) {
    return ui_frame(&(struct ui_frame) {
        .size = ui_size_fix(400.0, 400.0),
    });
}

static struct ui_node ui_test_key_zone(struct ui_node root, bool insert_before, struct ui_key zone_key) {
    if (insert_before) {
        ui_frame(&(struct ui_frame) {
            .parent = root,
            .size = ui_size_fix(10.0, 10.0),
        });
    }

    return ui_node(root, zone_key);
}

static void ui_test_key_zone_fill(struct ui_node zone) {
    ui_frame(&(struct ui_frame) {
        .node = zone,
        .pos = ui_pos_xy(0.0, 0.0),
        .size = ui_size_fix(400.0, 400.0),
    });
}

static struct ui_node ui_test_key_zone_filled(struct ui_node root, bool insert_before, struct ui_key zone_key) {
    struct ui_node zone = ui_test_key_zone(root, insert_before, zone_key);
    ui_test_key_zone_fill(zone);
    return zone;
}

static struct ui_tap_gesture ui_test_key_tap(struct ui_node zone, struct ui_key key, uint32_t* count) {
    return ui_tap(&(struct ui_tap) {
        .node = zone,
        .key = key,
        .on_change = ui_test_key_on_tap,
        .user = count,
    });
}

static void ui_test_key_touch(struct ui_node root, enum app_pointer_event_type type, double timestamp) {
    struct app_event event = {
        .type = app_event_type_pointer,
        .pointer = {
            .type = type,
            .id = 1,
            .pos = make_double2(200.0, 200.0),
            .timestamp = timestamp,
        },
    };
    ui_input_dispatch(root, timestamp, &event, 1, 1.0);
}

static void ui_test_key_touch_frame(struct ui_node prev, uint32_t frame, double timestamp) {
    if (frame == 1) {
        ui_test_key_touch(prev, app_pointer_event_type_touch_began, timestamp);
    } else if (frame == 2) {
        ui_test_key_touch(prev, app_pointer_event_type_touch_ended, timestamp + 0.1);
    }
}

static uint32_t ui_test_key_tap_across_frames(bool insert_before, struct ui_key zone_key) {
    uint32_t count = 0;
    struct ui_node prev = ui_node_null;

    for (uint32_t frame = 0; frame < 3; frame++) {
        ui_test_key_touch_frame(prev, frame, 1.0);

        struct ui_node root = ui_test_key_root();
        struct ui_node zone = ui_test_key_zone_filled(root, insert_before && frame >= 1, zone_key);
        ui_test_key_tap(zone, ui_key_auto, &count);

        ui_layout(root);

        ui_release(prev);
        prev = root;
    }

    ui_release(prev);

    return count;
}

static void ui_test_key_named(void) {
    uint32_t count = 0;
    struct ui_node root = ui_test_key_root();
    struct ui_node zone = ui_test_key_zone_filled(root, false, ui_key_auto);
    struct ui_tap_gesture first = ui_test_key_tap(zone, ui_key_auto, &count);
    struct ui_tap_gesture second = ui_test_key_tap(zone, ui_key_auto, &count);
    struct ui_tap_gesture named = ui_test_key_tap(zone, ui_key("named"), &count);
    struct ui_pan_gesture pan = ui_pan(&(struct ui_pan) {
        .node = zone,
        .key = ui_key("named"),
    });

    ui_test_check(ui_input_get_gesture(first.raw) != NULL && ui_input_get_gesture(named.raw) != NULL && ui_tap_gesture_has_touch(first) == false, "a gesture has state as soon as it is declared");
    ui_test_check(first.raw != second.raw && second.raw != named.raw && named.raw != pan.raw, "gestures on a node get distinct keys");

    ui_layout(root);

    ui_test_key_touch(root, app_pointer_event_type_touch_began, 3.0);

    ui_test_check(ui_input_get_gesture(named.raw) != ui_input_get_gesture(pan.raw), "the same name on two kinds stays distinct");
    ui_test_check(ui_tap_gesture_has_touch(named), "a named gesture tracks the touch");

    ui_test_key_touch(root, app_pointer_event_type_touch_ended, 3.1);
    ui_test_check(count >= 1, "two anonymous taps and a named one coexist on a node and a tap fires");

    ui_release(root);
}

static void ui_test_key_anonymous(void) {
    uint32_t count = 0;
    bool distinct = false;
    bool tracked = false;
    struct ui_node prev = ui_node_null;

    for (uint32_t frame = 0; frame < 3; frame++) {
        ui_test_key_touch_frame(prev, frame, 7.0);

        struct ui_node root = ui_test_key_root();
        struct ui_node zone = ui_test_key_zone_filled(root, false, ui_key_auto);
        struct ui_tap_gesture first = ui_test_key_tap(zone, ui_key_auto, &count);
        struct ui_tap_gesture second = ui_test_key_tap(zone, ui_key_auto, &count);
        struct ui_tap_gesture named = ui_test_key_tap(zone, ui_key("named"), &count);

        if (frame == 1) {
            distinct = ui_input_get_gesture(first.raw) != NULL && first.raw != second.raw && second.raw != named.raw && first.raw != named.raw;
            tracked = ui_tap_gesture_has_touch(first) && ui_tap_gesture_has_touch(second) && ui_tap_gesture_has_touch(named);
        }

        ui_layout(root);

        ui_release(prev);
        prev = root;
    }

    ui_release(prev);

    ui_test_check(distinct, "anonymous gestures of one kind on a node keep distinct state");
    ui_test_check(tracked, "gestures constructed from a created node read the touch across a rebuild");
    ui_test_check(count >= 1, "anonymous and named gestures coexist on a node and a tap fires");
}

static void ui_test_key_before_content(void) {
    uint32_t count = 0;
    bool touch_before_content = false;
    struct ui_node prev = ui_node_null;

    for (uint32_t frame = 0; frame < 3; frame++) {
        ui_test_key_touch_frame(prev, frame, 5.0);

        struct ui_node root = ui_test_key_root();
        struct ui_node zone = ui_test_key_zone(root, frame >= 1, ui_key("zone"));
        struct ui_tap_gesture tap = ui_test_key_tap(zone, ui_key_auto, &count);

        if (frame == 1) {
            touch_before_content = ui_tap_gesture_has_touch(tap);
        }

        ui_test_key_zone_fill(zone);
        ui_layout(root);

        ui_release(prev);
        prev = root;
    }

    ui_release(prev);

    ui_test_check(touch_before_content, "a gesture bound to a declared node reads state before the content is set");
    ui_test_check(count == 1, "the gesture keeps the tap through a shifted sibling");
}

static uint32_t ui_test_key_pointer_across_frames(bool explicit_zone) {
    uint32_t ended = 0;
    struct ui_node prev = ui_node_null;

    for (uint32_t frame = 0; frame < 3; frame++) {
        ui_test_key_touch_frame(prev, frame, 2.0);

        struct ui_node root = ui_test_key_root();
        struct ui_key zone_key = explicit_zone ? ui_key("pointer zone") : ui_key_auto;
        struct ui_node zone = ui_test_key_zone_filled(root, explicit_zone && frame >= 1, zone_key);
        ui_on_pointer(&(struct ui_on_pointer) {
            .node = zone,
            .func = ui_test_key_on_pointer,
            .user = &ended,
        });
        ui_layout(root);

        ui_release(prev);
        prev = root;
    }

    ui_release(prev);

    return ended;
}

static void ui_test_key_input(void) {
    struct cr_mem* mem = cr_mem_libc;

    uint32_t allocs_before = cr_rec_get_alloc_count();
    uint32_t nodes_before = ui_get_node_alloc_count();

    struct ui_input* prev_input = ui_input_get();
    ui_input_set(ui_input_create(mem));

    ui_test_check(ui_test_key_tap_across_frames(false, ui_key_auto) == 1, "anonymous gesture persists across identical rebuilds");
    ui_test_check(ui_test_key_tap_across_frames(true, ui_key_auto) == 0, "anonymous gesture follows a shifted node ordinal");
    ui_test_check(ui_test_key_tap_across_frames(true, ui_key("zone")) == 1, "explicit node key keeps the gesture through a shift");

    ui_test_key_named();
    ui_test_key_anonymous();
    ui_test_key_before_content();

    ui_test_check(ui_test_key_pointer_across_frames(false) == 1, "pointer handler binds by node key across rebuilds");
    ui_test_check(ui_test_key_pointer_across_frames(true) == 1, "explicit node key keeps the pointer handler through a shift");

    {
        struct ui_node root = ui_test_key_root();
        struct ui_node field = ui_frame(&(struct ui_frame) {
            .parent = root,
            .size = ui_size_fix(100.0, 20.0),
        });

        ui_on_text(&(struct ui_on_text) {
            .node = field,
            .func = ui_test_key_on_event,
        });
        ui_input_set_focus(ui_get_key(field));
        ui_layout(root);
        ui_input_dispatch(root, 0.0, NULL, 0, 1.0);

        ui_test_check(ui_input_get_focus() == ui_get_key(field), "focus is the node key of the node with a text handler");

        ui_release(root);
        ui_input_dispatch(ui_node_null, 0.0, NULL, 0, 1.0);
        ui_test_check(ui_input_get_focus() == 0, "focus clears once the node with the text handler is gone");
    }

    {
        struct ui_node root = ui_test_key_root();
        struct ui_node panel = ui_frame(&(struct ui_frame) {
            .parent = root,
            .pos = ui_pos_xy(100.0, 100.0),
            .size = ui_size_fix(200.0, 200.0),
        });
        struct ui_node field = ui_frame(&(struct ui_frame) {
            .parent = panel,
            .pos = ui_pos_xy(10.0, 10.0),
            .size = ui_size_fix(50.0, 20.0),
        });

        ui_on_text(&(struct ui_on_text) {
            .node = field,
            .func = ui_test_key_on_event,
        });
        ui_layout(root);

        double2 origin = ui_get_origin_from_root(root, field);
        ui_test_check(fabs(origin.x - 110.0) < 0.01 && fabs(origin.y - 110.0) < 0.01, "origin from root is relative to the root");
        ui_test_check(ui_input_has_focusable_at(make_double2(115.0, 115.0)), "a nested focusable is found in root space");
        ui_test_check(ui_input_has_focusable_at(make_double2(15.0, 15.0)) == false, "a nested focusable is not found at its parent-relative origin");

        ui_release(root);
    }

    ui_input_destroy(ui_input_get());
    ui_input_set(prev_input);

    ui_test_check(ui_get_node_alloc_count() == nodes_before, "input test releases every node");
    ui_test_check(cr_rec_get_alloc_count() == allocs_before, "input test alloc balance");
}

void ui_test_key_(void) {
    ui_test_key_structure();
    ui_test_key_declared();
    ui_test_key_derive();
    ui_test_key_input();
}
