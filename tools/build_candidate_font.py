"""Build the private horizontal candidate face (requires fontTools).

Usage: python tools/build_candidate_font.py Minh-Nguyen.ttf NomNaTong-Regular.ttf WenJin-P0.ttf WenJin-P2.ttf
The donor fonts are only needed to regenerate the checked-in runtime asset.
"""
import argparse
from pathlib import Path

from fontTools import subset
from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont


DATA = Path(__file__).resolve().parents[1] / "data"
FAMILY = "Nom Keyboard Serif"


def dictionary_characters():
    required = set()
    for filename in ("nom_dict_single.tsv", "nom_dict_word.tsv"):
        for line in (DATA / filename).read_text(encoding="utf-8-sig").splitlines():
            for value in line.split("\t")[1:]:
                required.update(map(ord, value))
    return required


def horizontal_base(path):
    font = TTFont(path, recalcTimestamp=False)
    # Preserve the horizontal layouts and variation sequences. Omitting the
    # vertical alternates leaves space below TrueType's 65,535-glyph limit.
    options = subset.Options()
    options.layout_features = sorted({
        record.FeatureTag for table in ("GSUB", "GPOS")
        for record in font[table].table.FeatureList.FeatureRecord
    } - {"vert", "vrt2", "vpal", "vhal", "vkna"})
    options.name_IDs = ["*"]
    options.name_languages = ["*"]
    options.glyph_names = True
    options.notdef_outline = True
    keep = set(font.getBestCmap())
    for table in font["cmap"].tables:
        if table.format == 14:
            keep.update(table.uvsDict)
    sub = subset.Subsetter(options=options)
    sub.populate(unicodes=keep)
    sub.subset(font)
    return font


def add_missing(base, donor, missing):
    if base["head"].unitsPerEm != donor["head"].unitsPerEm:
        raise ValueError("Expected matching em sizes")
    cmap = donor.getBestCmap()
    take = sorted(missing.intersection(cmap))
    outlines = donor.getGlyphSet()
    imported = {}
    unicode_maps = [t for t in base["cmap"].tables if t.isUnicode() and t.format in (4, 12)]
    for cp in take:
        original = cmap[cp]
        if original not in imported:
            name = f"nomFallback{cp:06X}"
            # Composite references and hint programs belong to the donor face.
            # Copy a standalone outline and its original horizontal metrics.
            outline = DecomposingRecordingPen(outlines)
            outlines[original].draw(outline)
            pen = TTGlyphPen(None)
            outline.replay(pen)
            base["glyf"][name] = pen.glyph()
            base["hmtx"][name] = donor["hmtx"][original]
            imported[original] = name
        for table in unicode_maps:
            if table.format == 12 or cp < 0xFFFF:
                table.cmap[cp] = imported[original]
    missing.difference_update(take)
    print(f"{donor['name'].getDebugName(1)}: {len(take)} supplemental code points")


def rename_face(font):
    names = font["name"]
    # Remove localized names too, so GDI always finds the private family.
    replacements = {1: FAMILY, 2: "Regular", 3: "NomKeyboardSerif-1.000",
                    4: FAMILY, 5: "Version 1.000", 6: "NomKeyboardSerif-Regular"}
    for number in (*replacements, 16, 17, 18, 21, 22, 25):
        names.removeNames(nameID=number)
    for number, value in replacements.items():
        names.setName(value, number, 3, 1, 0x409)
    names.setName("SIL Open Font License 1.1. See CandidateFont-LICENSE.txt for credits and notices.",
                  13, 3, 1, 0x409)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("minh", "nom_na_tong", "wenjin_p0", "wenjin_p2"):
        parser.add_argument(arg, type=Path)
    args = parser.parse_args()
    base = horizontal_base(args.minh)
    previous = TTFont(args.nom_na_tong)
    # Keep the previous release's repertoire for imported and learned words,
    # even when a code point doesn't occur in the shipped dictionaries.
    needed = (dictionary_characters() | set(previous.getBestCmap())) - set(base.getBestCmap())
    for path in (args.wenjin_p0, args.wenjin_p2):
        with TTFont(path) as donor:
            add_missing(base, donor, needed)
    add_missing(base, previous, needed)
    if needed or len(base["glyf"].glyphOrder) > 65535:
        raise ValueError(f"Unmapped code points or glyph limit exceeded: {sorted(needed)}")
    base.setGlyphOrder(list(base["glyf"].glyphOrder))
    rename_face(base)
    base.save(DATA / "NomKeyboardSerif.ttf")


if __name__ == "__main__":
    main()
