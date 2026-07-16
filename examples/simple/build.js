'use strict';

const build = bd.create('../..');

const simple = bd.targetCreate(build, 'simple', 'app');
bd.targetSetConfig(simple, bd.configCreateDefault(build, { bundleIdentifier: 'com.example.simple' }));
bd.targetAddInputEx(simple, { path: 'build/ios/simple', filter: bd.platformIos });
bd.targetAddInputEx(simple, { path: 'build/wasm/index.wasm', filter: bd.platformWasm });
bd.targetAddInputEx(simple, { path: '../../lib/wasm-release/lib.js', filter: bd.platformWasm });

bd.setDefaults(build, { target: simple, profile: bd.profileRelease });

uniform.exit(bd.run(build, uniform.args));
