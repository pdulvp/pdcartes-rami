#!/usr/bin/env python3
"""Le dessin de l'icône, et la série de fichiers qu'elle doit devenir.

Un carré vert aux coins arrondis, un « R » d'or dedans : les couleurs du
premier tapis et la lettre de l'enseigne.

Ce script est fait pour tourner à la main, quand le dessin change :

    python3 tools/make-icon.py

Il écrit dans `assets/icon/`, et ce qu'il écrit est versionné. Le build
n'installe que des fichiers : qui compile le jeu n'a besoin ni d'Inkscape ni
d'ImageMagick, et l'icône qu'il empaquette est celle qu'on a regardée.

Deux outils ici, et rien ailleurs :
  - fontconfig, à qui l'on montre la fonte du dépôt sans la poser sur la
    machine, par un XDG_DATA_HOME de passage ;
  - Inkscape, qui convertit la lettre en tracé puis rend les PNG.

L'ICO est écrit ici même : c'est un en-tête, une table et les PNG recopiés
tels quels, et le faire soi-même vaut mieux que de l'emprunter à ImageMagick,
qui recomprime chaque taille en bitmap -- cinq fois le poids, et les rendus
nets d'Inkscape remplacés par des réductions de la grande image.
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

# Les tailles qu'un thème d'icônes attend, et que l'ICO de Windows emporte.
SIZES = (16, 24, 32, 48, 64, 128, 256)

# La palette, teinte 142 -- le tapis « Vert », celui de `defaultFeltHue()`.
# Les deux verts sont le drap pris un peu au-dessus de sa clarté et son bord ;
# l'or et l'ombre sont ceux que `TitleMark` donne déjà à l'enseigne.
FELT_TOP = "#2d7447"
FELT_BOTTOM = "#143620"
GOLD = "#e2bc6b"
SHADE = "#102b1a"

# Le carré, sur un canevas de 256. Une marge, pour que l'icône ne touche pas
# le bord de sa case ; un rayon au quart du côté, qui est ce que portent les
# icônes d'aujourd'hui.
BOX, MARGIN, RADIUS = 256.0, 8.0, 56.0
# Ce que la lettre prend de la hauteur du canevas.
LETTER_SHARE = 0.52
# L'ombre portée, en part de la hauteur de la lettre -- `kDrop` de TitleMark,
# qui décale d'un demi sur la largeur.
DROP_SHARE = 0.022


def run(args, **kw):
    try:
        return subprocess.run(args, check=True, capture_output=True, text=True, **kw)
    except FileNotFoundError:
        sys.exit(f"manque : {args[0]}")
    except subprocess.CalledProcessError as e:
        sys.exit(f"{args[0]} a échoué :\n{e.stderr}")


def glyph_path(tmp):
    """Le « R » de Zen Dots, rendu en tracé et ramené à son coin haut-gauche.

    La fonte est montrée à fontconfig par un XDG_DATA_HOME de passage : rien
    n'est installé sur la machine, et le script tourne pareil partout.
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
        sys.exit("Inkscape n'a pas rendu de tracé : la fonte n'a pas été trouvée ?")
    return node.get("d")


def bounds(d):
    """La boîte du tracé, mesurée par Inkscape plutôt que devinée des points.

    Un tracé a des courbes, et le coin d'une courbe n'est pas dans la liste de
    ses points de contrôle.
    """
    with tempfile.TemporaryDirectory() as t:
        probe = Path(t) / "probe.svg"
        probe.write_text(
            '<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="1000">'
            f'<path id="r" d="{d}"/></svg>')
        out = run(["inkscape", str(probe), "--query-id=r", "--query-all"]).stdout
    # `--query-all` rend une ligne par objet : id,x,y,largeur,hauteur.
    for line in out.splitlines():
        parts = line.split(",")
        if parts[0] == "r":
            return tuple(float(p) for p in parts[1:5])
    sys.exit("Inkscape n'a pas mesuré le tracé.")


def master_svg(d, box):
    x, y, w, h = box
    scale = BOX * LETTER_SHARE / h
    # Le tracé est posé par un `translate` qui tient compte de son propre
    # coin : c'est l'encre qu'on centre, pas le repère de la fonte.
    tx = (BOX - w * scale) / 2.0 - x * scale
    ty = (BOX - h * scale) / 2.0 - y * scale
    drop = h * DROP_SHARE
    side = BOX - 2 * MARGIN

    def n(v):
        return f"{v:.3f}".rstrip("0").rstrip(".")

    return f"""<?xml version="1.0" encoding="UTF-8"?>
<!-- Écrit par tools/make-icon.py. Le dessin se change là-bas, pas ici. -->
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
    """Les PNG rendus, reliés en un .ico.

    Le format tient en trois lignes : un en-tête de six octets, une entrée de
    seize par taille, puis les images bout à bout. Les PNG y entrent tels
    quels -- Windows les lit depuis Vista, et le jeu demande bien plus récent
    que cela. Une hauteur de 256 s'écrit zéro, le champ ne faisant qu'un
    octet.
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
