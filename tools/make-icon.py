#!/usr/bin/env python3
"""Génère l'icône de l'application : le SVG, les PNG et le fichier ICO.

L'icône est un carré vert à coins arrondis portant la lettre « R » dorée. Les
couleurs sont celles de la palette du tapis par défaut.

Ce script se lance à la main, lorsque le dessin change :

    python3 tools/make-icon.py

Il écrit dans `assets/icon/`. Les fichiers produits sont versionnés, de sorte
que la compilation du projet ne requiert ni Inkscape ni aucun autre outil
graphique.

Dépendances d'exécution :
  - fontconfig, auquel la police du dépôt est présentée au moyen d'un
    XDG_DATA_HOME temporaire, sans installation sur la machine ;
  - Inkscape, qui convertit la lettre en tracé puis effectue le rendu des PNG.

Le fichier ICO est assemblé par ce script plutôt que par ImageMagick, pour
deux raisons : ImageMagick recompresse chaque taille en bitmap, ce qui
quintuple le poids du fichier, et il produit les petites tailles par
réduction de la grande image au lieu d'utiliser les rendus d'Inkscape.
"""

import shutil
import struct
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "icon"
FONT = ROOT / "assets" / "ZenDots" / "ZenDots-Regular.ttf"
NAME = "pdcartes-rami"

# Tailles attendues par les thèmes d'icônes, et incluses dans le fichier ICO.
SIZES = (16, 24, 32, 48, 64, 128, 256)

# Couleurs de la palette, teinte 142 : le tapis « Vert » rendu par
# `defaultFeltHue()`. Les deux verts correspondent au tapis, légèrement
# éclairci, et à son bord. L'or et l'ombre sont ceux employés par `TitleMark`.
FELT_TOP = "#2d7447"
FELT_BOTTOM = "#143620"
GOLD = "#e2bc6b"
SHADE = "#102b1a"

# Géométrie du carré, sur un canevas de 256 pixels. La marge évite que
# l'icône touche le bord de sa case. Le rayon vaut environ le quart du côté,
# proportion usuelle pour une icône d'application.
BOX, MARGIN, RADIUS = 256.0, 8.0, 56.0
# Hauteur de la lettre, en proportion de la hauteur du canevas.
LETTER_SHARE = 0.52
# Décalage de l'ombre portée, en proportion de la hauteur de la lettre.
# Même valeur que `kDrop` dans TitleMark, dont le décalage horizontal vaut la
# moitié du décalage vertical.
DROP_SHARE = 0.022


def run(args, **kw):
    try:
        return subprocess.run(args, check=True, capture_output=True, text=True, **kw)
    except FileNotFoundError:
        sys.exit(f"outil absent : {args[0]}")
    except subprocess.CalledProcessError as e:
        sys.exit(f"{args[0]} a échoué :\n{e.stderr}")


def glyph_path(tmp):
    """Convertit le « R » de la police Zen Dots en tracé SVG.

    La police est rendue visible de fontconfig au moyen d'un XDG_DATA_HOME
    temporaire. Rien n'est installé sur la machine, et le résultat ne dépend
    donc pas des polices présentes.
    """
    home = tmp / "fcroot"
    (home / "fonts").mkdir(parents=True)
    shutil.copy(FONT, home / "fonts" / FONT.name)
    env = {"XDG_DATA_HOME": str(home), "XDG_CACHE_HOME": str(tmp / "fccache"),
           "PATH": "/usr/bin:/bin", "HOME": str(tmp)}

    src, dst = tmp / "r.svg", tmp / "r-path.svg"
    src.write_text(
        '<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="1000">'
        '<text x="100" y="800" font-family="Zen Dots" font-size="800">R</text></svg>')
    run(["inkscape", str(src), "--export-text-to-path", "--export-plain-svg",
         "-o", str(dst)], env=env)

    tree = ET.parse(dst)
    node = tree.find(".//{http://www.w3.org/2000/svg}path")
    if node is None:
        sys.exit("Inkscape n'a produit aucun tracé : police introuvable.")
    return node.get("d")


def bounds(d):
    """Retourne le rectangle englobant du tracé, mesuré par Inkscape.

    La mesure est déléguée à Inkscape parce que le tracé comporte des courbes :
    leur étendue réelle ne se déduit pas des seuls points de contrôle.
    """
    with tempfile.TemporaryDirectory() as t:
        probe = Path(t) / "probe.svg"
        probe.write_text(
            '<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="1000">'
            f'<path id="r" d="{d}"/></svg>')
        out = run(["inkscape", str(probe), "--query-id=r", "--query-all"]).stdout
    # `--query-all` produit une ligne par objet : id,x,y,largeur,hauteur.
    for line in out.splitlines():
        parts = line.split(",")
        if parts[0] == "r":
            return tuple(float(p) for p in parts[1:5])
    sys.exit("Inkscape n'a pas mesuré le tracé.")


def master_svg(d, box):
    x, y, w, h = box
    scale = BOX * LETTER_SHARE / h
    # Le `translate` tient compte du coin du rectangle englobant, afin de
    # centrer le tracé lui-même et non l'origine du repère de la police.
    tx = (BOX - w * scale) / 2.0 - x * scale
    ty = (BOX - h * scale) / 2.0 - y * scale
    drop = h * DROP_SHARE
    side = BOX - 2 * MARGIN

    def n(v):
        return f"{v:.3f}".rstrip("0").rstrip(".")

    return f"""<?xml version="1.0" encoding="UTF-8"?>
<!-- Fichier généré par tools/make-icon.py. Ne pas modifier directement. -->
<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256"
     viewBox="0 0 256 256">
  <defs>
    <linearGradient id="felt" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="{FELT_TOP}"/>
      <stop offset="1" stop-color="{FELT_BOTTOM}"/>
    </linearGradient>
  </defs>
  <rect x="{n(MARGIN)}" y="{n(MARGIN)}" width="{n(side)}" height="{n(side)}"
        rx="{n(RADIUS)}" fill="url(#felt)"/>
  <g transform="translate({n(tx)},{n(ty)}) scale({n(scale)})">
    <path d="{d}" fill="{SHADE}" fill-opacity="0.45"
          transform="translate({n(drop / 2)},{n(drop)})"/>
    <path d="{d}" fill="{GOLD}"/>
  </g>
</svg>
"""


def write_ico(pngs, dest):
    """Assemble les PNG rendus en un fichier ICO.

    Le format se compose d'un en-tête de six octets, d'une entrée de seize
    octets par image, puis des images concaténées. Les PNG sont inclus sans
    recompression : Windows sait les lire depuis Vista, version très
    antérieure à celles que vise le programme. Les champs de largeur et de
    hauteur occupant un octet, la valeur 256 s'y code par zéro.
    """
    blobs = [p.read_bytes() for p in pngs]
    offset = 6 + 16 * len(blobs)
    head = struct.pack("<HHH", 0, 1, len(blobs))
    table = b""
    for size, blob in zip(SIZES, blobs):
        table += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, 1, 32,
                             len(blob), offset)
        offset += len(blob)
    dest.write_bytes(head + table + b"".join(blobs))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as t:
        tmp = Path(t)
        d = glyph_path(tmp)
        svg = OUT / f"{NAME}.svg"
        svg.write_text(master_svg(d, bounds(d)))
        print(f"écrit {svg.relative_to(ROOT)}")

        pngs = []
        for size in SIZES:
            png = OUT / f"{NAME}-{size}.png"
            run(["inkscape", str(svg), f"--export-width={size}",
                 f"--export-height={size}", "-o", str(png)])
            pngs.append(png)
            print(f"écrit {png.relative_to(ROOT)}")

        ico = OUT / f"{NAME}.ico"
        write_ico(pngs, ico)
        print(f"écrit {ico.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
