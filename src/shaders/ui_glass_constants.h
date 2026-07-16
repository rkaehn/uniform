#define ui_glass_pick(t, gv) ((gv) == 0u ? (t)[0] : (gv) == 1u ? (t)[1] : (gv) == 2u ? (t)[2] : (t)[3])

static const float ui_glass_cap[ui_glass_variant_count] = { 1.082224f, 1.082224f, 1.128684f, 1.030777f };
static const float ui_glass_kfh_cos_spread[ui_glass_variant_count] = { 0.258819f, -0.275637f, 0.258819f, -0.275637f };
static const float ui_glass_tint_cap = 1.0859375f;

#define ui_glass_kfh_amount 0.4f
#define ui_glass_kfh_color_bias -0.25f
#define ui_glass_kfh_offset_pt -0.5f
