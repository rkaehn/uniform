'use strict';

const build = bd.create('../../..');

const config = bd.configCreateDefault(build, { bundleIdentifier: 'com.raffaelkaehn.uniform-tests', teamIdentifier: 'LSYVV8JF85' });
bd.configAddString(config, bd.platformIos, 'appleInfoPlistBool', 'UIFileSharingEnabled=YES');
bd.configAddString(config, bd.platformIos, 'appleInfoPlistBool', 'LSSupportsOpeningDocumentsInPlace=YES');
bd.configAddString(config, bd.platformIos, 'appleInfoPlistBool', 'UniformEditableScript=YES');

const tests = bd.targetCreate(build, 'uniform_tests', 'app');
bd.targetSetConfig(tests, config);
bd.targetAddInput(tests, 'main.js');
bd.targetAddInput(tests, 'Inter.ttf');
bd.addUniform(tests);

uniform.exit(bd.run(build, uniform.args));
