'use strict';

const build = bd.create();

const config = bd.configCreateDefault(build);
bd.configSetString(config, bd.all, 'uniformPath', '../..');

const shaders = bd.targetCreate(build, 'triangle_shaders', 'shaders');
bd.targetSetConfig(shaders, config);
bd.targetAddInput(shaders, 'triangle.hlsl');

const configApp = bd.configCreateCopy(config);
bd.configSetString(configApp, bd.all, 'appleBundleIdentifier', 'com.example.triangle-js');
bd.configAddString(configApp, bd.platformIos, 'appleInfoPlistBool', 'UIFileSharingEnabled=YES');
bd.configAddString(configApp, bd.platformIos, 'appleInfoPlistBool', 'LSSupportsOpeningDocumentsInPlace=YES');
bd.configAddString(configApp, bd.platformIos, 'appleInfoPlistBool', 'UniformEditableScript=YES');

const triangle = bd.targetCreate(build, 'triangle-js', 'app');
bd.targetSetConfig(triangle, configApp);
bd.targetAddInput(triangle, 'main.js');
bd.targetAddInputEx(triangle, { target: shaders });
bd.targetAddUniform(triangle);

bd.setDefaults(build, { target: triangle, platform: bd.platformMacos, profile: bd.profileRelease });

uniform.exit(bd.run(build, uniform.args));
