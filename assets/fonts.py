import io
import os
import sys
import urllib.request
import zipfile

from fontTools import subset
from fontTools.ttLib import TTFont

INTER_URL = "https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip"
JBM_URL = "https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip"

UNICODES = (
    list(range(0x0020, 0x007F))
    + [c for c in range(0x00A0, 0x0100) if c != 0x00AD]
    + list(range(0x2010, 0x2016))
    + [0x2018, 0x2019, 0x201A, 0x201C, 0x201D, 0x201E]
    + list(range(0x2020, 0x2023))
    + [0x2026, 0x2030, 0x2039, 0x203A, 0x20AC, 0x2122, 0x2192, 0x2190, 0x2191, 0x2193, 0x2212, 0x2713]
)

ASSETS = os.path.dirname(os.path.abspath(__file__))
GLYPHS_HEADER = os.path.join(ASSETS, "..", "tests", "src", "ui", "ui_test_glyphs.h")


def fetch(url):
    cache = os.path.join(ASSETS, ".cache", os.path.basename(url))
    if os.path.exists(cache):
        return open(cache, "rb").read()
    print(f"downloading {url}")
    data = urllib.request.urlopen(url).read()
    os.makedirs(os.path.dirname(cache), exist_ok=True)
    open(cache, "wb").write(data)
    return data


def extract(zip_data, suffix):
    with zipfile.ZipFile(io.BytesIO(zip_data)) as z:
        for name in z.namelist():
            if name.endswith(suffix):
                return z.read(name)
    raise KeyError(suffix)


def subset_font(raw, out_path):
    font = TTFont(io.BytesIO(raw))
    options = subset.Options()
    options.layout_features = ["*"]
    options.name_IDs = ["*"]
    options.notdef_outline = True
    subsetter = subset.Subsetter(options)
    subsetter.populate(unicodes=UNICODES)
    subsetter.subset(font)
    font.save(out_path)
    covered = sorted(set(font.getBestCmap().keys()))
    print(f"{os.path.basename(out_path)}: {font['maxp'].numGlyphs} glyphs, {len(covered)} codepoints, {os.path.getsize(out_path)} bytes")
    return covered


def write_glyphs_header(mono, ui):
    def table(name, cps):
        lines = [f"static const uint32_t {name}[] = {{"]
        for i in range(0, len(cps), 12):
            lines.append("    " + " ".join(f"0x{c:04x}," for c in cps[i : i + 12]))
        lines.append("};")
        return "\n".join(lines)

    with open(GLYPHS_HEADER, "w") as f:
        f.write("#pragma once\n\n#include <stdint.h>\n\n")
        f.write(table("ui_test_glyphs_mono_", mono))
        f.write("\n\n")
        f.write(table("ui_test_glyphs_ui_", ui))
        f.write("\n")
    print(f"wrote {os.path.normpath(GLYPHS_HEADER)}")


def main():
    inter_zip = fetch(INTER_URL)
    jbm_zip = fetch(JBM_URL)

    ui = subset_font(extract(inter_zip, "InterVariable.ttf"), os.path.join(ASSETS, "Inter.ttf"))
    mono = subset_font(extract(jbm_zip, "JetBrainsMono[wght].ttf"), os.path.join(ASSETS, "JetBrainsMono.ttf"))

    open(os.path.join(ASSETS, "Inter-LICENSE.txt"), "wb").write(extract(inter_zip, "LICENSE.txt"))
    open(os.path.join(ASSETS, "JetBrainsMono-LICENSE.txt"), "wb").write(extract(jbm_zip, "OFL.txt"))

    write_glyphs_header(mono, ui)


if __name__ == "__main__":
    sys.exit(main())
