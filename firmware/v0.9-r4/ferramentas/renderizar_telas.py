"""Gera imagens PNG das telas da v0.9 a partir das operações registradas nos testes.

Uso (a partir de firmware/v0.9/):
    python3 ferramentas/renderizar_telas.py <telas.jsonl> <glcdfont.c> <pasta_saida>

As operações vêm de testes/teste_v09.cpp (fillScreen, fillRect, drawFastHLine e
texto). O texto é desenhado com a MESMA fonte clássica da Adafruit GFX
(glcdfont.c), como no drawChar da biblioteca: 5 colunas de 8 pixels, escala
inteira, fundo transparente e deslocamento dos códigos >= 176 sem cp437(true).

É uma prévia para revisão: não reproduz o contraste, o ângulo de visão nem a
calibração de cor do painel físico.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw

LARGURA, ALTURA, ESCALA = 160, 128, 3


def carregar_fonte(caminho: Path) -> list[int]:
    t = caminho.read_text(encoding="utf-8", errors="replace")
    corpo = t[t.index("{", t.index("font[]")) + 1: t.index("};")]
    corpo = re.sub(r"/\*.*?\*/", "", re.sub(r"//.*", "", corpo), flags=re.S)
    b = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", corpo)]
    assert len(b) == 256 * 5
    return b


def rgb565(c: int) -> tuple[int, int, int]:
    r, g, b = (c >> 11) & 0x1F, (c >> 5) & 0x3F, c & 0x1F
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)


def renderizar(ops: list, fonte: list[int]) -> Image.Image:
    img = Image.new("RGB", (LARGURA, ALTURA), (0, 0, 0))
    px = img.load()
    d = ImageDraw.Draw(img)
    for tipo, x, y, w, h, tam, cor, texto in ops:
        rgb = rgb565(cor)
        if tipo in "SR" and w > 0 and h > 0:
            d.rectangle([x, y, x + w - 1, y + h - 1], fill=rgb)
        elif tipo == "L":
            d.line([x, y, x + w - 1, y], fill=rgb)
        elif tipo == "T":
            cx = x
            for ch in texto:
                c = ord(ch)
                if c >= 176:
                    c += 1  # comportamento 'classic' da GFX sem cp437(true)
                for i in range(5):
                    coluna = fonte[c * 5 + i]
                    for j in range(8):
                        if coluna >> j & 1:
                            for a in range(tam):
                                for b in range(tam):
                                    X, Y = cx + i * tam + a, y + j * tam + b
                                    if 0 <= X < LARGURA and 0 <= Y < ALTURA:
                                        px[X, Y] = rgb
                cx += 6 * tam
    return img.resize((LARGURA * ESCALA, ALTURA * ESCALA), Image.NEAREST)


def main(argv: list[str]) -> int:
    telas, fonte_c, saida = Path(argv[1]), Path(argv[2]), Path(argv[3])
    fonte = carregar_fonte(fonte_c)
    saida.mkdir(parents=True, exist_ok=True)
    imagens = []
    for linha in telas.read_text(encoding="utf-8").splitlines():
        t = json.loads(linha)
        img = renderizar(t["ops"], fonte)
        img.save(saida / f"{t['nome']}.png")
        imagens.append((t["nome"], img))
    # Folha de contato com as telas principais (sem as variações de cada pergunta).
    principais = [(n, i) for n, i in imagens if not re.search(r"_V0[78]-.*_(dica|feedback)|^feedback_.*_V0", n)]
    cols, w, h = 4, LARGURA * ESCALA, ALTURA * ESCALA + 22
    folha = Image.new("RGB", (cols * (w + 10) + 10, ((len(principais) + cols - 1) // cols) * (h + 10) + 10), (40, 40, 40))
    d = ImageDraw.Draw(folha)
    for k, (nome, img) in enumerate(principais):
        x, y = 10 + (k % cols) * (w + 10), 10 + (k // cols) * (h + 10)
        folha.paste(img, (x, y + 22))
        d.text((x, y + 4), nome, fill=(230, 230, 230))
    folha.save(saida / "_folha_de_contato.png")
    print(f"{len(imagens)} telas em {saida} ({len(principais)} na folha de contato)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
