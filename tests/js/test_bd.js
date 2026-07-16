'use strict';

js_test('bd', async () => {
    if (bd.create === undefined) {
        check(true, 'build tools are only available on macOS');
        return;
    }
    const build = bd.create('../../..');
    const config = bd.configCreateDefault(build);
    const frameworks = bd.configGetArrayCount(config, bd.platformMacos | bd.profileRelease, 'frameworks');
    bd.configSetString(config, bd.all, 'appleBundleIdentifier', 'com.example.test');
    bd.configAddString(config, bd.platformMacos, 'frameworks', 'Metal');
    bd.configAddString(config, bd.platformMacos, 'frameworks', 'ImageIO');
    bd.configSetTrue(config, bd.profileDebug, 'cAsan');
    bd.configSetInt(config, bd.all, 'cOptimize', 2);
    check(bd.configGetString(config, bd.platformMacos | bd.profileRelease, 'appleBundleIdentifier') === 'com.example.test', 'config strings round trip');
    check(bd.configGetArrayCount(config, bd.platformMacos | bd.profileRelease, 'frameworks') === frameworks + 2 && bd.configGetArrayString(config, bd.platformMacos | bd.profileRelease, 'frameworks', frameworks + 1) === 'ImageIO', 'config arrays accumulate');
    check(bd.configIsTrue(config, bd.platformMacos | bd.profileDebug, 'cAsan') && bd.configHasValue(config, bd.platformMacos | bd.profileRelease, 'cAsan') === false, 'config flags are filtered by profile');
    check(bd.configGetInt(config, bd.platformIos | bd.profileRelease, 'cOptimize') === 2, 'config numbers round trip');
    const copy = bd.configCreateCopy(config);
    bd.configSetNull(copy, bd.all, 'cOptimize');
    check(bd.configIsNull(copy, bd.platformIos | bd.profileRelease, 'cOptimize') && bd.configGetInt(config, bd.platformIos | bd.profileRelease, 'cOptimize') === 2, 'copies are independent');
    check(bd.configHasValue(config, bd.platformIos | bd.profileRelease, 'appleSceneDelegate') === false, 'configs without a bundle identifier skip the app settings');
    const appConfig = bd.configCreateDefault(build, { bundleIdentifier: 'com.example.app', teamIdentifier: 'TEAM' });
    check(bd.configGetString(appConfig, bd.platformMacos | bd.profileRelease, 'appleBundleIdentifier') === 'com.example.app' && bd.configGetString(appConfig, bd.platformIos | bd.profileRelease, 'appleSceneDelegate') === 'AppSceneDelegate' && bd.configGetString(appConfig, bd.platformIos | bd.profileRelease, 'appleTeamIdentifier') === 'TEAM', 'app configs add the apple settings');
    bd.configAddString(appConfig, bd.platformIos, 'appleEntitlementBool', 'com.apple.developer.sustained-execution=YES');
    check(bd.configHasValue(appConfig, bd.platformMacos | bd.profileRelease, 'appleEntitlementBool') === false && bd.configGetArrayString(appConfig, bd.platformIos | bd.profileRelease, 'appleEntitlementBool', 0) === 'com.apple.developer.sustained-execution=YES', 'entitlements are configured per platform');
    const target = bd.targetCreate(build, 'js_test_target', 'app');
    bd.targetSetConfig(target, appConfig);
    bd.targetAddInput(target, 'main.js');
    bd.targetAddInputEx(target, { path: 'Inter.ttf', filter: bd.platformIos });
    bd.configSetInt(bd.targetGetConfig(target), bd.all, 'runPort', 9444);
    check(bd.configGetInt(bd.targetGetConfig(target), bd.platformWasm | bd.profileRelease, 'runPort') === 9444 && bd.configGetInt(appConfig, bd.platformWasm | bd.profileRelease, 'runPort') === 9443, 'targets own a copy of their config');
    check(target !== 0 && bd.platformLabel(bd.platformMacos) === 'macos' && bd.profileLabel(bd.profileRelease) === 'release' && bd.all === 63 && bd.moduleAll === 1023, 'targets and labels');
});
