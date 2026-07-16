PORT ?= 9443
TEST_REF ?= ref@2x-20261025-062814

empty :=
space := $(empty) $(empty)
comma := ,
define newline


endef
url_args = $(subst $(space),&,$(strip --test --ref $(TEST_REF) --remote-log $(ARGS)))

bd_sources := $(wildcard src/bd/*.c) src/prim/prim.c src/cr/cr_mem.c src/cr/cr_str.c src/cr/cr_task.c src/cr/cr_util.c \
    src/cl/cl_server.c src/cl/cl_crypto.c external/picohttpparser/picohttpparser.c
bd_headers := $(wildcard include/bd*.h include/cl*.h include/cr*.h include/prim.h src/bd/*.h src/cl/*.h)
bd_flags := -Iinclude -Iexternal/picohttpparser -Iexternal/bearssl-0.6/macos-aarch64/include
bd_libs := external/bearssl-0.6/macos-aarch64/lib/libbearssl.a
bd_optimized := src/cl/cl_crypto.c

apps := tests demo

xcodeprojs := uniform.xcodeproj $(foreach a,$(apps),$a/$a.xcodeproj)
compile_commands := build/compile_commands.json $(apps:%=%/build/compile_commands.json)

endpoints := \
    test-macos test-ios test-wasm test-wasm-ios \
    perf-macos perf-ios perf-wasm perf-wasm-ios \
    capture-ios \
    tests-macos-debug tests-macos-release tests-macos-debug-run tests-macos-release-run \
    tests-ios-debug tests-ios-release tests-ios-debug-run tests-ios-release-run tests-ios-release-export \
    tests-wasm-debug tests-wasm-release tests-wasm-debug-run tests-wasm-release-run \
    tests_server-macos-debug-run tests_server-macos-release-run \
    tests_server-linux-debug tests_server-linux-release tests_server-linux-debug-run tests_server-linux-release-run \
    server_tests-macos-debug-run server_tests-macos-release-run \
    demo-macos-debug demo-macos-release demo-macos-debug-run demo-macos-release-run \
    demo-ios-debug demo-ios-release demo-ios-debug-run demo-ios-release-run demo-ios-release-export \
    demo-wasm-debug demo-wasm-release demo-wasm-debug-run demo-wasm-release-run \
    demo_server-macos-debug-run demo_server-macos-release-run \
    demo_server-linux-debug demo_server-linux-release demo_server-linux-debug-run demo_server-linux-release-run \
    demo_server-linux-upload \
    uniform-macos-debug uniform-macos-release uniform-ios-debug uniform-ios-release \
    uniform-wasm-debug uniform-wasm-release uniform-linux-debug uniform-linux-release \
    runtime-macos-release runtime-macos-release-export runtime_ios-ios-release runtime_web-wasm-release dist-macos-release projectgen

build_endpoints := $(filter %-debug %-release,$(endpoints)) $(foreach p,macos ios wasm,shaders-$(p)-debug shaders-$(p)-release)

project_uniform := .
project_shaders := .
project_runtime := .
project_runtime_ios := .
project_runtime_web := .
project_dist := .
project_tests := tests
project_tests_shaders := tests
project_tests_server := tests
project_server_tests := tests
project_demo := demo
project_demo_server := demo

project = $(or $(project_$(1)),$(error unknown target $(1)))
target = $(word 1,$(subst -, ,$1))
platform = $(word 2,$(subst -, ,$1))
profile = $(word 3,$(subst -, ,$1))
output_arg = $(if $(OUTPUT),--output=$(OUTPUT))
verbose_arg = $(if $(VERBOSE),--verbose)
device_arg = $(if $(DEVICE),--device="$(DEVICE)")
bd = cd $(call project,$(call target,$1)) && ./build/bd $(strip --target=$(call target,$1) --platform=$(call platform,$1) --profile=$(call profile,$1) $(output_arg) $(verbose_arg) $(device_arg))$(if $2, $2)
merge_cdb = { echo '['; { for p in $(compile_commands); do [ -f $$p ] && sed -e '1d' -e '$$d' -e 's/,$$//' $$p; done; } | awk 'NR>1 { printf ",\n" } { printf "%s", $$0 } END { if (NR) printf "\n" }'; echo ']'; } > compile_commands.json
device_filter = $(if $(DEVICE),Name == '$(DEVICE)' OR Identifier == '$(DEVICE)',$1 AND hardwareProperties.reality == 'physical')
device_list = xcrun devicectl list devices --filter "$(call device_filter,$1)" --hide-default-columns --columns Identifier --hide-headers | awk '/ \(/ { print $$1 }'
connected_device = { $(call device_list,connectionProperties.tunnelState == 'connected'); $(call device_list,connectionProperties.transportType == 'wired'); } | head -1

all: dist-macos-release

.SECONDEXPANSION:

build/bd $(apps:%=%/build/bd): %build/bd: %build.c $(bd_sources) $(bd_headers)
	@rm -rf $@-obj && mkdir -p $@-obj
	pids= && for f in $(filter %.c,$^); do env -i clang $(bd_flags) $$([ $$f = $(bd_optimized) ] && echo -O2) -c $$f -o $@-obj/$$(basename $$f .c).o & pids="$$pids $$!"; done && for p in $$pids; do wait $$p || exit 1; done
	env -i clang $@-obj/*.o $(bd_libs) -o $@
	@rm -rf $@-obj

$(build_endpoints): %: $$(call project,$$(call target,$$*))/build/bd
	$(call bd,$*)
	@$(merge_cdb)

%-run: $$(call project,$$(call target,$$*))/build/bd
	$(call bd,$*,--run -- $(ARGS))
	@$(merge_cdb)

%-install: $$(call project,$$(call target,$$*))/build/bd
	$(call bd,$*,--install)
	@$(merge_cdb)

%-export: $$(call project,$$(call target,$$*))/build/bd
	$(call bd,$*,--export)
	@$(merge_cdb)

certificate_dir := $(or $(XDG_CONFIG_HOME),$(HOME)/.config)/uniform

certificate:
	@[ -f $(certificate_dir)/certificate.pem ] || { echo "no certificate in $(certificate_dir), create it with uniform --certificate"; exit 1; }
	@mkdir -p build && cp $(certificate_dir)/certificate.pem $(certificate_dir)/private_key.pem build/

tests_server-linux-debug-run tests_server-linux-release-run: tests_server-linux-%-run: tests_server-linux-% certificate
	ssh vm -q "cd /mnt/shared/tests/build/products/tests_server-linux-$* && (pkill ./tests_server || true) && ./tests_server --certificate /mnt/shared/build/certificate.pem --private-key /mnt/shared/build/private_key.pem"

demo_server-linux-debug-run demo_server-linux-release-run: demo_server-linux-%-run: demo_server-linux-% certificate
	ssh vm -q "cd /mnt/shared/demo/build/products/demo_server-linux-$* && (pkill ./demo_server || true) && ./demo_server --certificate /mnt/shared/build/certificate.pem --private-key /mnt/shared/build/private_key.pem"

demo_server-linux-upload: demo_server-linux-release
	ssh droplet -q "cd /root && (killall -q server demo_server || true) && rm -f server demo_server"
	scp demo/build/products/demo_server-linux-release/demo_server droplet:
	ssh droplet -q "nohup /root/demo_server --certificate /root/.acme.sh/raffk.com/fullchain.cer --private-key /root/.acme.sh/raffk.com/raffk.com.key > log.txt 2>&1 &"

test-macos:
	@$(MAKE) tests-macos-release-run ARGS="--test --ref $(TEST_REF) $(ARGS)"; status=$$?; \
	$(MAKE) test-js ARGS="$(ARGS)" || status=1; exit $$status

test-ios:
	@$(MAKE) tests-ios-release-run ARGS="--test --ref $(TEST_REF) $(ARGS)"; status=$$?; \
	$(MAKE) test-js-ios ARGS="$(ARGS)" || status=1; exit $$status

test-wasm: tests/build/bd
	@rm -rf tests/build/chrome; \
	( for _ in $$(seq 3600); do curl -skf -o /dev/null https://localhost:$(PORT)/ && break; sleep 0.05; done; \
	  exec "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless --enable-webgpu-developer-features --window-size=1200,900 --user-data-dir=tests/build/chrome --no-first-run "https://localhost:$(PORT)/?$(url_args)" ) > /dev/null 2>&1 & chrome=$$!; \
	(cd tests && ./build/bd --target=tests_server --platform=macos --profile=release --run -- --port $(PORT)); status=$$?; kill $$chrome 2> /dev/null; wait $$chrome 2> /dev/null; rm -rf tests/build/chrome; \
	$(MAKE) test-js-wasm ARGS="$(ARGS)" || status=1; exit $$status

test-wasm-ios: tests/build/bd
	@( for _ in $$(seq 3600); do curl -skf -o /dev/null https://localhost:$(PORT)/ && break; sleep 0.05; done; \
	  exec xcrun devicectl device process launch --terminate-existing --device "$$($(connected_device))" --payload-url "https://$$(scutil --get LocalHostName).local:$(PORT)/?$(url_args)" com.apple.mobilesafari ) > /dev/null 2>&1 & \
	(cd tests && ./build/bd --target=tests_server --platform=macos --profile=release --run -- --port $(PORT)); status=$$?; \
	$(MAKE) test-js-wasm-ios ARGS="$(ARGS)" || status=1; exit $$status

perf-macos:
	@$(MAKE) test-macos ARGS="--iterations 10 $(ARGS)"

perf-ios:
	@$(MAKE) test-ios ARGS="--iterations 10 $(ARGS)"

perf-wasm:
	@$(MAKE) test-wasm ARGS="--iterations 10 $(ARGS)"

perf-wasm-ios:
	@$(MAKE) test-wasm-ios ARGS="--iterations 10 $(ARGS)"

capture-ios: tests-ios-release-install tests-projectgen
	@[ -n "$(REF)" ] || { echo "usage: make capture-ios REF=ref@<scale>x-<name> [ONLY=<scene>]"; exit 1; }
	TEST_RUNNER_CAPTURE_REF=$(REF) $(if $(ONLY),TEST_RUNNER_CAPTURE_ONLY=$(ONLY) )xcodebuild test -project tests/tests.xcodeproj -scheme capture -destination id=$$($(connected_device)) -derivedDataPath tests/build/xcode-capture -allowProvisioningUpdates

runtime := bin/macos-release/uniform

release_dir := build/release

sysroots := wasi-libc-27.0/wasm32-wasip1-threads compiler-rt-22.1.8/wasm32-wasip1-threads musl-1.2.5/linux-x86_64
licenses := LICENSE $(filter-out external/yoga-%,$(wildcard external/*/LICENSE* external/*/FTL.TXT external/*/NOTICE))

release:
	@[ -n "$(VERSION)" ] || { echo "usage: make release VERSION=v1.2.3"; exit 1; }
	@[ -z "$$(git status --porcelain)" ] || { echo "commit all changes before releasing"; exit 1; }
	@$(MAKE) dist-macos-release runtime-macos-release-export
	rm -rf $(release_dir)
	mkdir -p $(release_dir)/uniform/lib $(release_dir)/uniform/bin
	cp -R include $(release_dir)/uniform
	cp -R lib/*-release $(release_dir)/uniform/lib
	cp -R bin/*-release $(release_dir)/uniform/bin
	for s in $(sysroots); do mkdir -p $(release_dir)/uniform/external/$$s && cp -R external/$$s/. $(release_dir)/uniform/external/$$s; done
	for f in $(licenses); do mkdir -p $(release_dir)/uniform/$$(dirname $$f) && cp $$f $(release_dir)/uniform/$$f; done
	cd $(release_dir) && zip -qrX uniform.zip uniform -x '*.DS_Store'
	git tag -f $(VERSION)
	git push -f origin $(VERSION)
	if gh release view $(VERSION) > /dev/null 2>&1; then gh release upload $(VERSION) $(release_dir)/uniform.zip --clobber; else gh release create $(VERSION) $(release_dir)/uniform.zip --verify-tag --title $(VERSION) --notes ""; fi

js_tests := tests/js/main.js $(sort $(wildcard tests/js/test_*.js))

tests/build/js/main.js: $(js_tests) tests/js/build.js assets/Inter.ttf
	@mkdir -p tests/build/js
	cat $(js_tests) > tests/build/js/main.js
	cp tests/js/build.js assets/Inter.ttf tests/build/js/

js_url_args = $(subst $(space),&,$(strip --remote-log $(ARGS)))

test-js: runtime-macos-release tests/build/js/main.js
	cd tests/build/js && ../../../$(runtime) main.js $(ARGS)

test-js-wasm: runtime-macos-release runtime_web-wasm-release tests/build/js/main.js
	@rm -rf tests/build/js/chrome; \
	( for _ in $$(seq 600); do curl -skf -o /dev/null https://localhost:$(PORT)/ && break; sleep 0.05; done; \
	  exec "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless --enable-webgpu-developer-features --window-size=800,600 --user-data-dir=tests/build/js/chrome --no-first-run "https://localhost:$(PORT)/?$(js_url_args)" ) > /dev/null 2>&1 & chrome=$$!; \
	./$(runtime) --serve --port $(PORT) tests/build/js/main.js; status=$$?; kill $$chrome 2> /dev/null; wait $$chrome 2> /dev/null; rm -rf tests/build/js/chrome; exit $$status

test-js-wasm-ios: runtime-macos-release runtime_web-wasm-release tests/build/js/main.js
	./$(runtime) --serve --port $(PORT) $(if $(DEVICE),--device="$(DEVICE)",--device) tests/build/js/main.js --remote-log $(ARGS)

test-js-ios: runtime-macos-release runtime_ios-ios-release tests/build/js/main.js
	cd tests/build/js && ../../../$(runtime) build.js --target=uniform_tests --platform=ios --profile=release --run $(device_arg) -- $(ARGS)

uniform-projectgen: build/bd
	./build/bd --xcodegen
	@$(merge_cdb)

$(apps:%=%-projectgen): %-projectgen: %/build/bd uniform-projectgen
	cd $* && ./build/bd --xcodegen
	@$(merge_cdb)

define workspace
<?xml version="1.0" encoding="UTF-8"?>
<Workspace
   version = "1.0">$(subst $(space)$(newline),$(newline),$(foreach p,$(xcodeprojs),$(newline)   <FileRef$(newline)      location = "group:$p">$(newline)   </FileRef>))
</Workspace>
endef

define settings
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>DerivedDataCustomLocation</key>
	<string>build/xcode</string>
	<key>DerivedDataLocationStyle</key>
	<string>WorkspaceRelativePath</string>
</dict>
</plist>
endef

define sublime
{
    "folders": [
        {
            "path": ".",
            "file_exclude_patterns": ["project.yml", "*.iso"],
            "folder_exclude_patterns": [".claude"]
        }
    ],
    "settings": {
        "auto_complete": false,
        "LSP": { "LSP-clangd": { "enabled": true } }
    },
    "build_systems": [
        {
            "name": "bd",
            "file_regex": "^(..[^:]*):([0-9]+):?([0-9]+)?:? (.*)$$",
            "working_dir": "$$project_path",
            "variants": [
                $(subst } {,}$(comma)$(newline)                {,$(foreach e,$(endpoints),{ "name": "$e"$(comma) "shell_cmd": "source ~/.config/zsh/.zshrc && make $e" }))
            ]
        }
    ]
}
endef

export workspace
export settings
export sublime

projectgen: uniform-projectgen $(apps:%=%-projectgen)
	@mkdir -p uniform.xcworkspace/xcuserdata/$(USER).xcuserdatad
	@printf '%s\n' "$$workspace" > uniform.xcworkspace/contents.xcworkspacedata
	@printf '%s\n' "$$settings" > uniform.xcworkspace/xcuserdata/$(USER).xcuserdatad/WorkspaceSettings.xcsettings
	@printf '%s\n' "$$sublime" > uniform.sublime-project
	@echo "[Generated uniform.xcworkspace]"
	@echo "[Generated uniform.sublime-project]"
	@echo "[Generated compile_commands.json]"

.PHONY: all $(build_endpoints) uniform-projectgen $(apps:%=%-projectgen) projectgen certificate \
    tests_server-linux-debug-run tests_server-linux-release-run demo_server-linux-debug-run demo_server-linux-release-run demo_server-linux-upload \
    test-macos test-ios test-wasm test-wasm-ios perf-macos perf-ios perf-wasm perf-wasm-ios \
    release test-js test-js-wasm test-js-wasm-ios test-js-ios capture-ios
