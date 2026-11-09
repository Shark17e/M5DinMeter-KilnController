#!/usr/bin/env python3
"""
Build the Italian user manual PDF for the M5DinMeter Kiln Controller.

Requires reportlab (pip install reportlab). The screenshots in docs/images are
rendered from the firmware drawing code by tools/render_ui.py.
"""

from pathlib import Path
from xml.sax.saxutils import escape

import reportlab
from PIL import Image as PILImage
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY, TA_LEFT, TA_RIGHT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate,
    Frame,
    HRFlowable,
    Image,
    KeepTogether,
    NextPageTemplate,
    PageBreak,
    PageTemplate,
    Paragraph,
    Spacer,
    Table,
    TableStyle,
)
from reportlab.platypus.tableofcontents import TableOfContents

ROOT = Path(__file__).resolve().parent.parent
IMG = ROOT / "docs" / "images"
OUT = ROOT / "docs" / "M5DinMeter-KilnController-Manuale-Utente.pdf"

# --- fonts -----------------------------------------------------------------
FONT_DIR = Path(reportlab.__file__).parent / "fonts"
pdfmetrics.registerFont(TTFont("Vera", str(FONT_DIR / "Vera.ttf")))
pdfmetrics.registerFont(TTFont("VeraBd", str(FONT_DIR / "VeraBd.ttf")))
pdfmetrics.registerFont(TTFont("VeraIt", str(FONT_DIR / "VeraIt.ttf")))
pdfmetrics.registerFont(TTFont("VeraBI", str(FONT_DIR / "VeraBI.ttf")))
pdfmetrics.registerFontFamily(
    "Vera", normal="Vera", bold="VeraBd", italic="VeraIt", boldItalic="VeraBI"
)

# --- palette ---------------------------------------------------------------
NAVY = colors.HexColor("#0B1220")
SURFACE = colors.HexColor("#121A26")
SURFACE2 = colors.HexColor("#1E293B")
CYAN = colors.HexColor("#0891B2")
CYAN_LIGHT = colors.HexColor("#22D3EE")
AMBER = colors.HexColor("#D97706")
GREEN = colors.HexColor("#059669")
RED = colors.HexColor("#DC2626")
TEXT = colors.HexColor("#1E293B")
MUTED = colors.HexColor("#64748B")
BORDER = colors.HexColor("#CBD5E1")
LIGHT = colors.HexColor("#F8FAFC")
NOTE_BG = colors.HexColor("#ECFEFF")
WARN_BG = colors.HexColor("#FEF2F2")
CODE_BG = colors.HexColor("#F1F5F9")
WHITE = colors.white

# --- styles ----------------------------------------------------------------
def style(name, **kw):
    base = dict(fontName="Vera", fontSize=9.6, leading=13.4, textColor=TEXT, spaceAfter=5)
    base.update(kw)
    return ParagraphStyle(name, **base)


S = {
    "h1": style("H1", fontName="VeraBd", fontSize=19, leading=23, textColor=NAVY,
                spaceBefore=2, spaceAfter=2),
    "h1_no_toc": style("H1NoTOC", fontName="VeraBd", fontSize=19, leading=23, textColor=NAVY,
                       spaceBefore=2, spaceAfter=2),
    "h2": style("H2", fontName="VeraBd", fontSize=12.2, leading=16, textColor=CYAN,
                spaceBefore=10, spaceAfter=3),
    "h3": style("H3", fontName="VeraBd", fontSize=10.8, leading=14, textColor=CYAN,
                spaceBefore=8, spaceAfter=3),
    "body": style("Body", alignment=TA_JUSTIFY),
    "bullet": style("Bullet", leftIndent=12, bulletIndent=2, spaceAfter=2.5),
    "small": style("Small", fontSize=8.2, leading=10.8, textColor=MUTED),
    "caption": style("Caption", fontName="VeraIt", fontSize=8.2, leading=10.8,
                     textColor=MUTED, alignment=TA_CENTER, spaceBefore=2, spaceAfter=8),
    "note": style("Note", fontSize=9.2, leading=12.6),
    "code": style("Code", fontName="Courier", fontSize=8.1, leading=10.5,
                  textColor=colors.HexColor("#0F172A")),
    "toc1": style("TOC1", fontName="VeraBd", fontSize=10.2, leading=15, textColor=NAVY,
                  spaceBefore=1),
    "toc2": style("TOC2", fontSize=9.0, leading=12, leftIndent=14, textColor=colors.HexColor("#334155")),
}


def P(text, s="body"):
    return Paragraph(text, S[s])


def h1(text):
    return [Paragraph(text, S["h1"]),
            HRFlowable(width="100%", thickness=1.1, color=CYAN_LIGHT, spaceBefore=1, spaceAfter=8)]


def h2(text):
    return [Paragraph(text, S["h2"])]


def bullets(items):
    return [Paragraph(t, S["bullet"], bulletText="•") for t in items]


def note_box(title, body, kind="note"):
    bg = NOTE_BG if kind == "note" else WARN_BG
    col = CYAN if kind == "note" else RED
    text = f"<b>{title}</b> {body}"
    p = Paragraph(text, S["note"])
    t = Table([[p]], colWidths=[170 * mm])
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), bg),
        ("LINEBEFORE", (0, 0), (0, -1), 3, col),
        ("BOX", (0, 0), (-1, -1), 0.4, col),
        ("LEFTPADDING", (0, 0), (-1, -1), 8),
        ("RIGHTPADDING", (0, 0), (-1, -1), 8),
        ("TOPPADDING", (0, 0), (-1, -1), 6),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
    ]))
    return [Spacer(1, 3), t, Spacer(1, 7)]


def framed_image(path, width_mm):
    img_path = IMG / path
    with PILImage.open(img_path) as im:
        iw, ih = im.size
    w = width_mm * mm
    h = w * ih / iw
    img = Image(str(img_path), width=w, height=h)
    t = Table([[img]], colWidths=[w])
    t.setStyle(TableStyle([
        ("BOX", (0, 0), (-1, -1), 0.6, BORDER),
        ("LEFTPADDING", (0, 0), (-1, -1), 1),
        ("RIGHTPADDING", (0, 0), (-1, -1), 1),
        ("TOPPADDING", (0, 0), (-1, -1), 1),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 1),
    ]))
    return t, h


def img_block(path, caption, width_mm=150):
    t, _ = framed_image(path, width_mm)
    return KeepTogether([t, Paragraph(caption, S["caption"])])


def code_block(lines):
    p = Paragraph("<br/>".join(escape(line) for line in lines), S["code"])
    t = Table([[p]], colWidths=[170 * mm])
    t.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), CODE_BG),
        ("BOX", (0, 0), (-1, -1), 0.4, BORDER),
        ("LEFTPADDING", (0, 0), (-1, -1), 8),
        ("RIGHTPADDING", (0, 0), (-1, -1), 8),
        ("TOPPADDING", (0, 0), (-1, -1), 6),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 6),
    ]))
    return [Spacer(1, 2), t, Spacer(1, 6)]


def two_images(path1, cap1, path2, cap2, width_mm=82):
    t1, _ = framed_image(path1, width_mm)
    t2, _ = framed_image(path2, width_mm)
    table = Table(
        [[t1, t2],
         [Paragraph(cap1, S["caption"]), Paragraph(cap2, S["caption"])]],
        colWidths=[width_mm * mm, width_mm * mm],
    )
    table.setStyle(TableStyle([
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 2),
        ("RIGHTPADDING", (0, 0), (-1, -1), 2),
        ("TOPPADDING", (0, 0), (-1, -1), 2),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 2),
    ]))
    return table


def data_table(rows, widths=(50, 120), header=True, font_size=8.6):
    cell = style("Cell", fontSize=font_size, leading=11.2, textColor=TEXT)
    head = style("CellHead", fontName="VeraBd", fontSize=font_size, leading=11.2, textColor=WHITE)
    data = []
    for i, r in enumerate(rows):
        if header and i == 0:
            data.append([Paragraph(str(c), head) for c in r])
        else:
            data.append([Paragraph(str(c), cell) for c in r])
    t = Table(data, colWidths=[w * mm for w in widths], repeatRows=1 if header else 0)
    st = [
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("GRID", (0, 0), (-1, -1), 0.35, BORDER),
        ("LEFTPADDING", (0, 0), (-1, -1), 5),
        ("RIGHTPADDING", (0, 0), (-1, -1), 5),
        ("TOPPADDING", (0, 0), (-1, -1), 4),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
    ]
    if header:
        st.append(("BACKGROUND", (0, 0), (-1, 0), NAVY))
        for i in range(1, len(data)):
            if i % 2 == 0:
                st.append(("BACKGROUND", (0, i), (-1, i), LIGHT))
    t.setStyle(TableStyle(st))
    return t


# --- page decorations ------------------------------------------------------
def on_cover(canvas, doc):
    canvas.saveState()
    canvas.setFillColor(NAVY)
    canvas.rect(0, 0, A4[0], A4[1], fill=1, stroke=0)
    canvas.setFillColor(CYAN_LIGHT)
    canvas.rect(0, 0, 4 * mm, A4[1], fill=1, stroke=0)

    canvas.setFont("Vera", 8)
    canvas.setFillColor(colors.HexColor("#94A3B8"))
    canvas.drawString(20 * mm, 272 * mm, "M5STACK DINMETER  ·  ESP32-S3")

    canvas.setStrokeColor(CYAN_LIGHT)
    canvas.setLineWidth(1.2)
    canvas.line(20 * mm, 266 * mm, 55 * mm, 266 * mm)

    canvas.setFillColor(WHITE)
    canvas.setFont("VeraBd", 34)
    canvas.drawString(20 * mm, 232 * mm, "Kiln Controller")
    canvas.setFillColor(CYAN_LIGHT)
    canvas.setFont("VeraBd", 20)
    canvas.drawString(20 * mm, 216 * mm, "Manuale utente")

    canvas.setFillColor(colors.HexColor("#CBD5E1"))
    canvas.setFont("Vera", 10.5)
    canvas.drawString(20 * mm, 200 * mm, "Controllore per forni ceramici fino a 1200 °C")

    canvas.setFillColor(colors.HexColor("#94A3B8"))
    canvas.setFont("Vera", 8.8)
    canvas.drawString(20 * mm, 185 * mm, "Curve 2–20 punti   ·   PID con auto-correzione   ·   Sicurezza fail-safe")

    # screenshot
    img_path = IMG / "main_menu.png"
    with PILImage.open(img_path) as im:
        iw, ih = im.size
    w = 120 * mm
    h = w * ih / iw
    canvas.drawImage(str(img_path), 45 * mm, 62 * mm, width=w, height=h, mask="auto")
    canvas.setStrokeColor(CYAN_LIGHT)
    canvas.setLineWidth(0.8)
    canvas.rect(45 * mm, 62 * mm, w, h)

    canvas.setFillColor(colors.HexColor("#94A3B8"))
    canvas.setFont("VeraIt", 8)
    canvas.drawString(45 * mm, 55 * mm, "Menu principale — render fedele del firmware")

    canvas.setStrokeColor(colors.HexColor("#334155"))
    canvas.setLineWidth(0.6)
    canvas.line(20 * mm, 30 * mm, 190 * mm, 30 * mm)
    canvas.setFont("Vera", 8)
    canvas.setFillColor(colors.HexColor("#94A3B8"))
    canvas.drawString(20 * mm, 22 * mm, "Shark17e  ·  CC BY-NC 4.0")
    canvas.drawRightString(190 * mm, 22 * mm, "Rev. 1.0  ·  Novembre 2026")
    canvas.restoreState()


def on_main(canvas, doc):
    canvas.saveState()
    canvas.setFont("Vera", 7.5)
    canvas.setFillColor(MUTED)
    canvas.drawString(20 * mm, 284 * mm, "M5DinMeter Kiln Controller")
    canvas.drawRightString(190 * mm, 284 * mm, "Manuale utente  ·  Rev. 1.0")
    canvas.setStrokeColor(CYAN_LIGHT)
    canvas.setLineWidth(0.6)
    canvas.line(20 * mm, 281 * mm, 190 * mm, 281 * mm)

    canvas.setStrokeColor(BORDER)
    canvas.setLineWidth(0.4)
    canvas.line(20 * mm, 14 * mm, 190 * mm, 14 * mm)
    canvas.setFont("Vera", 7.5)
    canvas.setFillColor(MUTED)
    canvas.drawString(20 * mm, 9.5 * mm, "Shark17e  ·  CC BY-NC 4.0")
    canvas.drawCentredString(105 * mm, 9.5 * mm, f"Pagina {canvas.getPageNumber()}")
    canvas.drawRightString(190 * mm, 9.5 * mm, "Novembre 2026")
    canvas.restoreState()


def on_back(canvas, doc):
    canvas.saveState()
    canvas.setFillColor(NAVY)
    canvas.rect(0, 0, A4[0], A4[1], fill=1, stroke=0)
    canvas.setFillColor(CYAN_LIGHT)
    canvas.rect(0, 0, 4 * mm, A4[1], fill=1, stroke=0)

    canvas.setFillColor(WHITE)
    canvas.setFont("VeraBd", 24)
    canvas.drawString(20 * mm, 245 * mm, "M5DinMeter")
    canvas.setFillColor(CYAN_LIGHT)
    canvas.drawString(20 * mm, 232 * mm, "Kiln Controller")
    canvas.setFillColor(colors.HexColor("#94A3B8"))
    canvas.setFont("Vera", 10)
    canvas.drawString(20 * mm, 220 * mm, "Manuale utente  ·  Rev. 1.0  ·  Novembre 2026")

    canvas.setStrokeColor(colors.HexColor("#334155"))
    canvas.setLineWidth(0.8)
    canvas.line(20 * mm, 208 * mm, 190 * mm, 208 * mm)

    canvas.setFillColor(WHITE)
    canvas.setFont("VeraBd", 13)
    canvas.drawString(20 * mm, 190 * mm, "Grazie per aver usato il controller.")
    canvas.setFillColor(colors.HexColor("#CBD5E1"))
    canvas.setFont("Vera", 9.5)
    canvas.drawString(20 * mm, 178 * mm, "Per aggiornamenti, codice e documentazione:")
    canvas.setFillColor(CYAN_LIGHT)
    canvas.setFont("Vera", 9.5)
    canvas.drawString(20 * mm, 170 * mm, "github.com/Shark17e/M5DinMeter-KilnController")

    canvas.setFillColor(colors.HexColor("#94A3B8"))
    canvas.setFont("Vera", 9)
    canvas.drawString(20 * mm, 60 * mm, "Shark17e  ·  CC BY-NC 4.0")
    canvas.drawString(20 * mm, 52 * mm, "Uso non commerciale consentito con attribuzione; uso commerciale vietato senza autorizzazione.")

    canvas.setFillColor(colors.HexColor("#F87171"))
    canvas.setFont("VeraBd", 9)
    canvas.drawString(20 * mm, 36 * mm, "Il forno va sempre sorvegliato: il software non sostituisce le protezioni hardware.")
    canvas.restoreState()


class ManualDoc(BaseDocTemplate):
    def __init__(self, filename, **kw):
        super().__init__(filename, **kw)
        self._toc_count = 0

    def afterFlowable(self, flowable):
        if isinstance(flowable, Paragraph) and flowable.style.name in ("H1", "H2"):
            level = 0 if flowable.style.name == "H1" else 1
            text = flowable.getPlainText()
            # stable key across the multiBuild passes
            key = "toc_" + str(abs(hash(text)))
            self.canv.bookmarkPage(key)
            self.canv.addOutlineEntry(text, key, level=level, closed=False)
            self.notify("TOCEntry", (level, text, self.page, key))


def build():
    doc = ManualDoc(
        str(OUT),
        pagesize=A4,
        leftMargin=20 * mm,
        rightMargin=20 * mm,
        topMargin=22 * mm,
        bottomMargin=18 * mm,
        title="M5DinMeter Kiln Controller — Manuale utente",
        author="Shark17e",
        subject="Manuale utente del controllore per forni ceramici M5Stack DinMeter",
        creator="tools/build_manual.py",
    )
    cover_frame = Frame(20 * mm, 18 * mm, 170 * mm, 255 * mm, id="cover",
                        leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
    main_frame = Frame(20 * mm, 18 * mm, 170 * mm, 255 * mm, id="main",
                       leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
    doc.addPageTemplates([
        PageTemplate(id="Cover", frames=[cover_frame], onPage=on_cover),
        PageTemplate(id="Main", frames=[main_frame], onPage=on_main),
        PageTemplate(id="Back", frames=[main_frame], onPage=on_back),
    ])

    story = [NextPageTemplate("Main"), PageBreak()]

    # ---------------- info + TOC ----------------
    story.append(Paragraph("Informazioni sul documento", S["h1_no_toc"]))
    story.append(HRFlowable(width="100%", thickness=1.1, color=CYAN_LIGHT, spaceBefore=1, spaceAfter=8))
    story.append(data_table([
        ("Documento", "Manuale utente"),
        ("Versione", "1.0"),
        ("Data", "Novembre 2026"),
        ("Firmware", "UI con popup azioni, editor con t Tot., rinomina"),
        ("Autore", "Shark17e"),
        ("Licenza", "CC BY-NC 4.0"),
        ("Repository", "github.com/Shark17e/M5DinMeter-KilnController"),
    ], widths=(35, 135), header=False))
    story.append(Spacer(1, 10))
    story.append(P("Questo manuale descrive l'uso del controllore M5DinMeter Kiln Controller. "
                   "Le schermate sono render fedeli del firmware: layout, colori e font sono gli stessi "
                   "del display; i valori mostrati (programmi, temperature, batteria) sono esempi.", "body"))
    story.append(Paragraph("Convenzioni", S["h3"]))
    story.append(data_table([
        ("<b>SHORT</b>", "Pressione breve del pulsante: apre, conferma o esegue l'azione selezionata."),
        ("<b>LONG</b>", "Pressione di 1 s: nel menu e nel popup è libero; serve in manuale, nome e conferma."),
        ("<b>Ruota</b>", "Rotazione del pomello: scorre la lista o regola il valore selezionato."),
    ], widths=(30, 140), header=False))
    story.append(Paragraph("Struttura del manuale", S["h3"]))
    story += bullets([
        "Sezioni 1–5: panoramica, sicurezza, installazione, comandi e menu.",
        "Sezioni 6–9: creazione e modifica dei programmi, avvio, modalità manuale.",
        "Sezioni 10–13: allarmi, batteria, dati e risoluzione problemi.",
        "Sezioni 14–17: specifiche tecniche e appendici.",
    ])
    story += note_box("Prima di iniziare:", "collegare la termocoppia, verificare il driver SSR e "
                      "alimentare da USB. Il controller non scalda se il sensore non è valido.")
    story.append(Spacer(1, 8))
    story.append(PageBreak())
    story.append(Paragraph("Indice", S["h1_no_toc"]))
    toc = TableOfContents()
    toc.levelStyles = [S["toc1"], S["toc2"]]
    toc.dotsMinLevel = 0
    story.append(toc)
    story.append(PageBreak())

    # ---------------- 1. Panoramica ----------------
    story += h1("1. Panoramica")
    story.append(P("Il controller gestisce l'intera cottura di un forno ceramico: segue una curva di "
                   "temperatura programmata, regola l'SSR con un PID e applica una serie di protezioni. "
                   "Si usa come un programmatore industriale: si sceglie un programma, si preme Avvia e "
                   "si lascia lavorare il forno."))
    story.append(P("L'interfaccia è composta dal display TFT 240×135, dal pomello rotativo e da un solo "
                   "pulsante. Non servono tastiere, touch o app esterne."))
    story += h2("Caratteristiche principali")
    story += bullets([
        "<b>Curve di temperatura</b>: da 2 a 20 punti, fino a 10 programmi, durata massima 24 h.",
        "<b>PID</b> con ciclo di 1 s, gain scheduling su 3 fasce e auto-correzione durante le cotture reali.",
        "<b>SSR time-proportional</b>: finestra fissa di 10 s, blocchi DC pieni, adatta a SSR zero-cross.",
        "<b>Sicurezza</b>: stop a 1250 °C, watchdog 10 s, fail-safe su errore sensore, watchdog di salita lenta.",
        "<b>Persistenza NVS</b>: programmi e guadagni PID sopravvivono ai riavvii.",
        "<b>P0 virtuale</b>: la curva parte dalla temperatura misurata all'avvio, non da 0 °C.",
    ])
    story += note_box("In breve:", "la prima rampa parte sempre dalla temperatura reale del forno. "
                      "Se il forno è freddo, la curva inizia dal valore ambiente; non c'è un salto a 0 °C.")

    # ---------------- 2. Sicurezza ----------------
    story += h1("2. Sicurezza")
    story += note_box("Attenzione:", "il controller non sostituisce le protezioni del forno. "
                      "Usare sempre fusibile, messa a terra, termostato di sicurezza indipendente e "
                      "sorveglianza durante la cottura. Il limite software di 1250 °C è un'ultima barriera, "
                      "non una protezione primaria.", "warn")
    story += bullets([
        "Temperature fino a 1200 °C: superfici, resistenze e parti metalliche possono causare ustioni. "
        "Tenere lontani materiali infiammabili e persone non addette.",
        "SSR e rete 230 V: l'installazione deve essere eseguita da personale qualificato, nel rispetto "
        "delle norme locali. Montare l'SSR su dissipatore e serrare i terminali.",
        "Alimentare il controller da USB 5 V durante la cottura; la batteria LiPo è una riserva.",
        "Usare una termocoppia tipo K isolata e fissata correttamente nel forno. Non toccare parti in "
        "tensione durante la misura.",
        "Non aggirare il fail-safe: se compare SENSORE ERRORE! l'SSR viene forzato OFF.",
        "Non lasciare la cottura incustodita. Il controller non è un dispositivo di sicurezza certificato.",
    ])
    story += h2("Protezioni del firmware")
    story.append(data_table([
        ("Condizione", "Comportamento"),
        ("Temperatura > 1250 °C", "SSR OFF, schermata rossa SOVRATEMP; uscita solo con long press."),
        ("5 letture sensore fallite", "sensorError = true, SSR OFF, banner SENSORE ERRORE!"),
        ("Temperatura >15 °C sotto la curva per 60 min", "schermata SALITA LENTA con 3 scelte."),
        ("Loop bloccato per 10 s", "watchdog hardware: reset del dispositivo; SSR OFF al boot."),
        ("Programma non in esecuzione", "SSR sempre OFF (tranne la modalità manuale)."),
    ], widths=(65, 105)))
    story += note_box("Nota:", "il watchdog resetta il dispositivo se il loop si blocca. Dopo il reset "
                      "l'SSR parte spento e i programmi restano in NVS.")

    # ---------------- 3. Installazione ----------------
    story += h1("3. Installazione e collegamenti")
    story.append(P("Lo schema seguente è un diagramma a blocchi: serve a capire il flusso dei segnali, "
                   "non sostituisce lo schema elettrico di progetto (docs/01_hardware.md)."))
    story.append(img_block("wiring.png", "Schema a blocchi: DinMeter, driver SSR, SSR, resistenze, "
                          "termocoppia e KMeterISO.", 160))
    story += h2("Componenti")
    story.append(data_table([
        ("Componente", "Dettagli"),
        ("Scheda", "M5Stack DinMeter (ESP32-S3), display TFT 240×135, encoder e pulsante."),
        ("SSR", "Fotek SSR-40 DA, zero-cross, 40 A, ingresso di controllo 3–32 VDC."),
        ("Driver SSR", "NPN 2N2222 / BC547: base da GPIO1 con 1 kΩ, pull-down 10 kΩ, emettitore a GND, "
                       "collettore sul terminale − dell'SSR; terminale + a +5 V. GND comune."),
        ("Termocoppia", "Tipo K, letta dal modulo KMeterISO (compatibile MAX31855), I2C 0x66, SDA 13, SCL 15, 400 kHz."),
        ("Alimentazione", "USB 5 V consigliata in cottura; LiPo 3000 mAh (progetto) o 250 mAh (fornita con il DinMeter)."),
    ], widths=(40, 130)))
    story += note_box("Importante:", "non pilotare l'SSR direttamente dal GPIO: il livello 3.3 V è marginale "
                      "per l'ingresso 3–32 VDC. Usare il driver NPN indicato nello schema di progetto. "
                      "Il firmware usa GPIO1 attivo alto.")
    story.append(P("La termocoppia va collegata al KMeterISO e questo alla porta I2C del DinMeter. "
                   "All'accensione il firmware prova 5 volte a rilevare il modulo; se non risponde, "
                   "prosegue in fail-safe con SSR OFF e il banner SENSORE ERRORE!."))

    # ---------------- 4. Comandi ----------------
    story.append(PageBreak())
    story += h1("4. Comandi e interfaccia")
    story.append(P("L'ingresso è composto da un solo pomello con pulsante. I gesti sono: rotazione, "
                   "short press e long press (1 s)."))
    story.append(data_table([
        ("Gesto", "Uso"),
        ("Ruota", "Scorre la lista, cambia il carattere o regola il valore selezionato."),
        ("Short press", "Apre, conferma o esegue l'azione selezionata."),
        ("Long press", "Nel menu principale e nel popup è libero (riservato). Serve in modalità manuale, "
                       "nella schermata nome e nella conferma cancellazione."),
    ], widths=(35, 135)))
    story.append(Spacer(1, 6))
    story.append(P("La status bar mostra l'icona e la percentuale della batteria e, nel menu, una pillola "
                   "con lo stato del sensore. La temperatura misurata è mostrata in grande nella parte "
                   "alta del menu."))
    story.append(two_images(
        "main_menu.png", "Menu principale: status bar, temperatura, card programmi.",
        "program_actions.png", "Short press su un programma: popup azioni.",
    ))

    # ---------------- 5. Menu e azioni ----------------
    story += h1("5. Menu principale e azioni")
    story.append(P("La lista mostra i programmi salvati, poi le voci Manuale, Aggiungi Programma e Spegni. "
                   "Il pomello scorre; la barra cyan indica la voce selezionata. Short press su un programma "
                   "apre il popup azioni; sulle altre voci esegue direttamente l'azione."))
    story.append(data_table([
        ("Voce del popup", "Cosa fa"),
        ("Avvia", "Avvia il programma selezionato. La curva parte dalla temperatura misurata (P0)."),
        ("Modifica curva", "Apre l'editor con una copia del programma; Salva aggiorna l'originale."),
        ("Rinomina", "Mostra il nome attuale e permette di digitare il nuovo nome."),
        ("Cancella programma", "Apre la conferma: SHORT conferma, LONG annulla."),
        ("Annulla", "Chiude il popup senza fare nulla."),
    ], widths=(45, 125)))
    story += note_box("Long press:", "nel menu principale e nel popup non fa nulla: è riservato a future "
                      "funzioni. Per uscire dal popup si usa la voce Annulla.")
    story.append(Spacer(1, 4))
    story.append(img_block("confirm_delete.png",
                           "Conferma cancellazione: SHORT conferma, LONG annulla.", 140))
    story += note_box("Limite:", "il sistema gestisce al massimo 10 programmi. Se la memoria è piena, "
                      "Salva non va a buon fine e il messaggio compare solo sulla seriale; cancellare un "
                      "programma e riprovare.")

    # ---------------- 6. Creare un programma ----------------
    story += h1("6. Creare un programma")
    story.append(P("Dal menu principale selezionare Aggiungi Programma e premere short. Si entra nella "
                   "schermata del nome; dopo la conferma si apre l'editor della curva."))
    story += h2("6.1 Nome")
    story.append(img_block("new_name.png",
                           "Nome: il pomello scorre l'alfabeto, short aggiunge, long conferma.", 140))
    story += bullets([
        "Alfabeto: A–Z, 0–9, spazio, '-' e '_'. Lunghezza massima 20 caratteri.",
        "Short press aggiunge il carattere corrente; long press conferma il nome.",
        "Un nome vuoto annulla la creazione (un programma senza nome non verrebbe ricaricato).",
    ])
    story.append(PageBreak())
    story += h2("6.2 Editor curva")
    story.append(img_block("editor.png",
                           "Editor: grafico, riga valori e barra comandi.", 150))
    story.append(P("Un nuovo programma parte con 2 punti a 0 °C (2 h e 4 h). <b>Impostare le temperature "
                   "prima di salvare</b>: se si salva subito, la curva resta a 0 °C."))
    story.append(KeepTogether(data_table([
        ("Comando", "Funzione"),
        ("< / >", "Seleziona il punto precedente o successivo."),
        ("T", "Modifica la temperatura del punto: ±5 °C per detent; short conferma."),
        ("t", "Mostra/modifica la durata del segmento dal punto precedente: ±5 min per detent. "
              "Il tempo cumulativo del punto cambia di conseguenza."),
        ("t Tot.", "Tempo cumulativo dall'inizio al punto selezionato (sola lettura)."),
        ("+P", "Inserisce un punto dopo quello selezionato, stessa temperatura, tempo a metà verso il successivo."),
        ("-P", "Elimina il punto selezionato; minimo 2 punti."),
        ("Salva / Annulla", "Salva il programma oppure scarta le modifiche."),
    ], widths=(35, 135))))
    story += bullets([
        "Temperatura: 0–1200 °C, passi di 5 °C.",
        "Tempi strettamente crescenti, almeno 5 min tra punti; durata totale massima 24 h.",
        "P0 è il punto grigio fisso: è la temperatura misurata all'avvio, non si modifica.",
        "L'editor è la review: quello che si vede è quello che verrà eseguito.",
    ])
    # ---------------- 7. Modifica / rinomina / cancella ----------------
    story += h1("7. Modificare, rinominare, cancellare")
    story += h2("7.1 Modificare una curva")
    story.append(P("Short press sul programma -> Modifica curva. Il firmware copia il programma nell'editor; "
                   "Salva aggiorna il programma esistente, Annulla scarta la copia. Il nome non cambia."))
    story.append(PageBreak())
    story += h2("7.2 Rinominare")
    story.append(img_block("rename.png",
                           "Rinomina: nome attuale in alto, nuovo nome digitato col pomello.", 140))
    story.append(P("Short press sul programma -> Rinomina. Il nome attuale è mostrato come riferimento; "
                   "il nuovo nome si digita con lo stesso alfabeto della creazione. Long press conferma, "
                   "nome vuoto annulla. La modifica viene salvata subito in NVS."))
    story += h2("7.3 Cancellare")
    story.append(P("Short press sul programma -> Cancella programma. La schermata di conferma mostra il "
                   "nome: SHORT conferma la cancellazione, LONG annulla. Dopo la cancellazione la lista "
                   "si riallinea e il programma viene rimosso da NVS."))

    # ---------------- 8. RUNNING ----------------
    story += h1("8. Avviare e monitorare")
    story.append(P("Selezionare un programma e scegliere Avvia. La prima rampa parte dalla temperatura "
                   "misurata in quel momento (P0). La schermata RUNNING mostra l'avanzamento in tempo reale."))
    story.append(img_block("running.png",
                           "RUNNING: nome e batteria, Tgt/Tsens e tempo, grafico con griglia e duty SSR.", 150))
    story += bullets([
        "<b>Barra superiore</b>: nome del programma (troncato) e batteria.",
        "<b>Seconda riga</b>: Tgt (setpoint) in ambra, Tsens (temperatura misurata) in verde, tempo trascorso/totale.",
        "<b>Card grafico</b>: intestazione RAMPA/TENUTA e pillola SSR ON/OFF duty%, griglia, curva programmata "
        "in ambra, storia misurata in verde, pallino bianco di posizione.",
        "<b>SSR</b>: time-proportional su finestra di 10 s; il duty è a passi del 10%.",
        "<b>Short press</b>: non fa nulla. <b>Long press</b>: ferma la cottura.",
        "<b>Fine cottura</b>: COTTURA COMPLETATA, durata e tempo di SSR acceso; long press per tornare al menu.",
    ])
    story += note_box("Errore sensore:", "la riga 2 diventa un banner rosso SENSORE ERRORE! SSR OFF. "
                      "Il controller non scalda con una lettura non valida.")

    # ---------------- 9. Manuale ----------------
    story += h1("9. Modalità manuale")
    story.append(P("Dal menu principale selezionare Manuale e premere short. L'SSR parte spento. "
                   "Long press accende/spegne l'SSR; short press esce e spegne l'SSR. Se il sensore è in "
                   "errore il toggle è bloccato."))
    story.append(img_block("manual.png",
                           "Modalità manuale: stato SSR e temperatura misurata.", 140))
    story += note_box("Attenzione:", "la modalità manuale non segue la curva e non usa il PID. Usarla solo "
                      "per prove brevi e non lasciare il forno incustodito.", "warn")
    story.append(Paragraph("Quando usarlo", S["h3"]))
    story += bullets([
        "Prova breve di accensione o verifica del SSR e del cablaggio.",
        "Recupero manuale dopo un allarme, sempre sotto supervisione.",
        "Non usarlo per una cottura completa: la temperatura non è regolata dal PID.",
    ])
    story += note_box("Suggerimento:", "in manuale l'SSR resta nello stato impostato fino all'uscita; "
                      "non c'è un timer di sicurezza. Controllare la temperatura e uscire manualmente.")

    # ---------------- 10. Stati speciali ----------------
    story.append(PageBreak())
    story += h1("10. Stati speciali e allarmi")
    story += h2("10.1 Salita lenta")
    story.append(img_block("slow_rise.png",
                           "SALITA LENTA: contesto, tre scelte e spiegazione della voce selezionata.", 150))
    story.append(P("La schermata compare se la temperatura resta più di 15 °C sotto la curva per 60 minuti "
                   "consecutivi. Non è un errore: il forno sta salendo troppo lentamente. Le scelte sono:"))
    story.append(KeepTogether(data_table([
        ("Scelta", "Comportamento"),
        ("Aspetta che recuperi", "Congela il setpoint al valore corrente della curva e attende che la "
                                 "temperatura arrivi (±2 °C), con tetto di 1 h; poi la scelta viene riproposta."),
        ("Allunga di 30 min", "Inserisce un punto piatto da 30 min alla posizione attuale e slitta il resto della curva."),
        ("Annulla cottura", "Ferma il programma e spegne l'SSR."),
    ], widths=(45, 125))))
    story += h2("10.2 Sovratemperatura")
    story.append(img_block("overtemp.png",
                           "SOVRATEMP: SSR spento; si esce solo con long press.", 140))
    story.append(P("Se la temperatura supera 1250 °C il firmware spegne l'SSR e mostra la schermata rossa. "
                   "L'uscita richiede un long press deliberato. Controllare il forno prima di riprendere."))
    story += h2("10.3 Errore sensore")
    story.append(P("Dopo 5 letture consecutive fallite (circa 5 s) il firmware imposta sensorError, spegne "
                   "l'SSR e mostra SENSORE ERRORE!. Controllare il collegamento del KMeterISO, l'indirizzo "
                   "I2C 0x66 e i pin SDA 13 / SCL 15. Se il sensore manca al boot, il dispositivo parte "
                   "comunque in fail-safe e non scalda."))

    # ---------------- 11. Batteria ----------------
    story += h1("11. Batteria e alimentazione")
    story.append(P("La percentuale mostrata nella status bar è una stima basata sulla tensione della "
                   "batteria, non un contatore di carica. L'icona cambia colore: verde sopra 60%, ambra "
                   "tra 25% e 60%, rossa sotto 25%."))
    story += bullets([
        "Durante la cottura usare USB 5 V: la batteria è una riserva, non l'alimentazione principale.",
        "Il caricatore TP4057 carica a circa 100 mA. Con la batteria ufficiale da 250 mAh la carica è "
        "relativamente rapida; con una cella da 3000 mAh può richiedere molte ore.",
        "Non è disponibile un indicatore di carica via software: i pin CHRG e STDBY del TP4057 non sono "
        "collegati al microcontrollore e il DinMeter non espone una lettura VBUS.",
        "Un valore fermo all'89% con USB collegato può essere normale: la tensione dipende dallo stato "
        "della cella e dalla corrente di carica. Per un controllo reale misurare la batteria con un multimetro.",
    ])
    story += note_box("Nota:", "il riscaldatore non è alimentato dalla batteria: la batteria alimenta solo "
                      "il controller e il circuito di comando dell'SSR.")

    # ---------------- 12. Dati ----------------
    story += h1("12. Dati e persistenza")
    story += bullets([
        "Fino a <b>10 programmi</b>; ogni programma ha da <b>2 a 20 punti</b>.",
        "Nome massimo 20 caratteri; durata totale massima 24 h; almeno 5 min tra punti.",
        "Programmi e guadagni PID sono salvati in NVS e sopravvivono ai riavvii.",
        "Se la NVS è vuota viene creato il programma Default: (0,200) -> (1h,200) -> (3h,500) -> (4h,500).",
        "Non esiste un comando di reset di fabbrica: i programmi si cancellano dalla UI. La cancellazione "
        "di un programma non azzera più i guadagni PID.",
    ])
    story += note_box("Salvataggio:", "il salvataggio riscrive il namespace dei programmi e poi ripristina "
                      "i guadagni PID correnti. I dati restano tra un riavvio e l'altro.")

    # ---------------- 13. Troubleshooting ----------------
    story.append(KeepTogether(h1("13. Manutenzione e risoluzione problemi") + [data_table([
        ("Sintomo", "Cosa fare"),
        ("SENSORE ERRORE!", "Controllare KMeterISO, alimentazione, SDA 13 / SCL 15 e indirizzo 0x66. "
                            "Riavviare dopo aver sistemato il collegamento."),
        ("Il programma non parte", "Servono almeno 2 punti e un programma selezionato; usare Avvia dal popup. "
                                   "Se ci sono 10 programmi, il salvataggio non riesce."),
        ("L'SSR non scalda", "Verificare sensorError, SOVRATEMP, programma fermo, driver NPN, fusibile e "
                             "collegamenti SSR. Il LED dell'SSR aiuta a capire se il comando arriva."),
        ("La temperatura non segue la curva", "Controllare la posizione della termocoppia e la curva. "
                                              "Se compare SALITA LENTA scegliere una delle tre opzioni."),
        ("Batteria non al 100%", "La percentuale è una stima di tensione; il caricatore è da 100 mA e non "
                                 "c'è uno stato di carica software. Misurare la batteria se serve."),
        ("Salvataggio non riuscito", "Memoria programmi piena: cancellare un programma e riprovare."),
        ("Schermo nero o reset", "Controllare alimentazione USB/batteria; il watchdog resetta dopo 10 s "
                                 "di loop bloccato. Riavviare e verificare i collegamenti."),
    ], widths=(48, 122))]))
    story += h2("Manutenzione")
    story += bullets([
        "Pulire il display con un panno morbido; non usare solventi.",
        "Tenere libero il dissipatore dell'SSR e verificare il serraggio dei terminali.",
        "Controllare periodicamente la termocoppia e il suo isolamento.",
        "Non aprire il case con alimentazione e SSR collegati.",
    ])
    story.append(Paragraph("Prima di contattare l'assistenza", S["h3"]))
    story += bullets([
        "Annotare il programma, il punto della curva e la temperatura al momento del problema.",
        "Riprodurre il problema e annotare esattamente cosa mostra il display.",
        "Aprire una issue su GitHub indicando versione firmware, comportamento e, se possibile, una foto.",
    ])
    story += note_box("Sicurezza:", "prima di qualsiasi intervento scollegare alimentazione e forno. "
                      "Non inviare il dispositivo collegato alla rete.", "warn")

    # ---------------- 14. Specifiche ----------------
    story.append(PageBreak())
    story += h1("14. Specifiche tecniche")
    story.append(data_table([
        ("Voce", "Valore"),
        ("MCU", "ESP32-S3, 240 MHz, 8 MB flash."),
        ("Display", "TFT 240×135, rotazione landscape."),
        ("Input", "Encoder rotativo + pulsante (short/long press)."),
        ("Uscita SSR", "GPIO1 attivo alto, time-proportional 10 s, passi 10%."),
        ("Termocoppia", "Tipo K via KMeterISO, I2C 0x66, SDA 13 / SCL 15, 400 kHz."),
        ("Programmi", "10 programmi, 2–20 punti, nome max 20 caratteri."),
        ("Tempi", "Min 5 min tra punti, durata totale max 24 h."),
        ("Temperatura", "0–1200 °C operativi; stop di sicurezza a 1250 °C."),
        ("PID", "Ciclo 1 s, 3 fasce, anti-windup, auto-correzione."),
        ("Sicurezza", "Watchdog 10 s, fail-safe sensore, watchdog salita lenta."),
        ("Alimentazione", "USB 5 V consigliata; LiPo 3000 mAh (progetto) / 250 mAh (fornita)."),
        ("Carica", "TP4057, circa 100 mA."),
        ("Build", "Circa 586 KB di flash (44%), 54 KB di RAM (16%)."),
        ("Licenza", "CC BY-NC 4.0."),
    ], widths=(40, 130)))

    # ---------------- 15. Quick start ----------------
    story += h1("15. Appendice A — Avvio rapido")
    story += bullets([
        "Collegare termocoppia, KMeterISO, driver SSR, SSR e alimentazione.",
        "Accendere il DinMeter e attendere il splash; verificare che il sensore sia OK.",
        "Selezionare un programma (oppure crearne uno) e premere short.",
        "Scegliere Avvia dal popup.",
        "Monitorare Tgt/Tsens e il grafico; long press per fermare.",
        "A fine cottura leggere durata e SSR on-time; long press per tornare al menu.",
    ])
    story += note_box("Prima di iniziare:", "verificare che il programma parta dalla temperatura reale "
                      "(P0) e che la prima rampa sia quella desiderata. Un nuovo programma parte con due "
                      "punti a 0 °C: impostare le temperature prima di salvare.")

    # ---------------- 16. Firmware update ----------------
    story += h1("16. Appendice B — Aggiornamento firmware")
    story.append(P("Per aggiornare il firmware servono arduino-cli (o Arduino IDE 2.x), il core "
                   "m5stack:esp32 3.3.8 e le librerie M5DinMeter, M5Unified e M5GFX."))
    story += code_block([
        "arduino-cli core install m5stack:esp32",
        "arduino-cli compile --fqbn m5stack:esp32:m5stack_dinmeter M5StackDin_KilnController",
        "arduino-cli upload -p COM7 --fqbn m5stack:esp32:m5stack_dinmeter M5StackDin_KilnController",
    ])
    story += note_box("Nota:", "l'upload dell'applicazione non cancella la NVS: programmi e guadagni PID "
                      "restano. Sostituire COM7 con la porta corretta.")

    # ---------------- 17. Glossario ----------------
    story.append(PageBreak())
    story += h1("17. Appendice C — Glossario")
    story.append(data_table([
        ("Termine", "Significato"),
        ("P0", "Punto virtuale (0, temperatura misurata) da cui parte la curva."),
        ("Setpoint / Tgt", "Temperatura obiettivo calcolata sulla curva."),
        ("Tsens", "Temperatura letta dalla termocoppia."),
        ("Duty", "Percentuale di tempo in cui l'SSR è acceso nella finestra di 10 s."),
        ("SSR", "Solid State Relay: interruttore elettronico per il carico del forno."),
        ("Zero-cross", "SSR che commuta vicino allo zero della tensione di rete."),
        ("PID", "Controllore proporzionale-integrale-derivativo."),
        ("NVS", "Memoria non volatile dell'ESP32, usata per programmi e PID."),
        ("Fail-safe", "Comportamento sicuro in caso di errore (SSR OFF)."),
        ("Watchdog", "Timer hardware che resetta il dispositivo se il loop si blocca."),
    ], widths=(40, 130)))
    story.append(Paragraph("Checklist prima della cottura", S["h3"]))
    story += bullets([
        "Il programma selezionato è quello giusto e la curva è stata controllata.",
        "La termocoppia è nel forno e lo stato del sensore è OK.",
        "L'SSR è su dissipatore, i terminali sono serrati e il fusibile è integro.",
        "Il controller è alimentato da USB 5 V.",
        "Il forno è chiuso e l'area intorno è libera da materiali infiammabili.",
        "La cottura sarà sorvegliata per tutta la durata.",
    ])
    story += note_box("Sicurezza:", "non lasciare mai il forno incustodito. Il controller è un "
                      "programmatore, non un dispositivo di sicurezza certificato.", "warn")

    # ---------------- back cover ----------------
    story.append(NextPageTemplate("Back"))
    story.append(PageBreak())
    story.append(Spacer(1, 1))

    doc.multiBuild(story)
    print("written", OUT)


if __name__ == "__main__":
    build()
