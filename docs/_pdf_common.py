# -*- coding: utf-8 -*-
"""Shared reportlab scaffolding (palette, styles, cover, chrome, helpers) for the
KeynetikPOS PDF manuals. Imported by generate_user_manual.py and
generate_technical_manual.py so the two documents share one look."""

from reportlab.lib.pagesizes import A4
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY
from reportlab.platypus import (
    BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle,
    PageBreak, NextPageTemplate, ListFlowable, ListItem, HRFlowable, CondPageBreak,
)

# ---------------------------------------------------------------- palette
NAVY   = colors.HexColor("#1b2a4a")
BLUE   = colors.HexColor("#2c5fa8")
ACCENT = colors.HexColor("#27ae60")
LIGHT  = colors.HexColor("#eef3fb")
GREYBG = colors.HexColor("#f5f7fa")
LINE   = colors.HexColor("#d4dcea")
DARK   = colors.HexColor("#22272e")
MUTED  = colors.HexColor("#5a6473")
WARNBG = colors.HexColor("#fdf3e7")
WARNBAR = colors.HexColor("#e08a1e")
CODEBG = colors.HexColor("#f3f5f9")

ss = getSampleStyleSheet()


def style(name, **kw):
    return ParagraphStyle(name, parent=ss["Normal"], **kw)


H1   = style("H1", fontName="Helvetica-Bold", fontSize=18, textColor=NAVY,
             spaceBefore=6, spaceAfter=10, leading=22)
H2   = style("H2", fontName="Helvetica-Bold", fontSize=13, textColor=BLUE,
             spaceBefore=14, spaceAfter=6, leading=16)
H3   = style("H3", fontName="Helvetica-Bold", fontSize=10.5, textColor=DARK,
             spaceBefore=8, spaceAfter=3, leading=13)
BODY = style("Body", fontName="Helvetica", fontSize=9.5, textColor=DARK,
             leading=14, alignment=TA_JUSTIFY, spaceAfter=5)
BULLET = style("Bullet", fontName="Helvetica", fontSize=9.3, textColor=DARK, leading=13.5)
STEP = style("Step", fontName="Helvetica", fontSize=9.4, textColor=DARK, leading=14)
SMALL = style("Small", fontName="Helvetica", fontSize=8.2, textColor=MUTED, leading=11)
CELL = style("Cell", fontName="Helvetica", fontSize=8.6, textColor=DARK, leading=11.5)
CELLB = style("CellB", fontName="Helvetica-Bold", fontSize=8.6, textColor=NAVY, leading=11.5)
CELLH = style("CellH", fontName="Helvetica-Bold", fontSize=8.8, textColor=colors.white, leading=11.5)
CODE = style("Code", fontName="Courier", fontSize=8.3, textColor=colors.HexColor("#14306b"),
             leading=11.5)
TOC  = style("TOC", fontName="Helvetica", fontSize=10, textColor=DARK, leading=17)
TOCNUM = style("TOCnum", fontName="Helvetica-Bold", fontSize=10, textColor=BLUE, leading=17)

COVER_TITLE = style("CoverTitle", fontName="Helvetica-Bold", fontSize=30,
                    textColor=colors.white, alignment=TA_CENTER, leading=34)
COVER_SUB = style("CoverSub", fontName="Helvetica", fontSize=13,
                  textColor=colors.HexColor("#cdd9ef"), alignment=TA_CENTER, leading=18)
COVER_SMALL = style("CoverSmall", fontName="Helvetica", fontSize=10,
                    textColor=colors.HexColor("#9fb3d6"), alignment=TA_CENTER, leading=14)

PW, PH = A4
MARGIN = 18 * mm


class Doc(BaseDocTemplate):
    """Two page templates: a navy cover and a chromed body (header bar + footer)."""

    def __init__(self, filename, running_title, footer_tagline, **k):
        super().__init__(filename, pagesize=A4, **k)
        self.running_title = running_title
        self.footer_tagline = footer_tagline
        frame = Frame(MARGIN, MARGIN, PW - 2 * MARGIN, PH - 2 * MARGIN - 6 * mm, id="body")
        cover = Frame(0, 0, PW, PH, id="cover",
                      leftPadding=0, rightPadding=0, topPadding=0, bottomPadding=0)
        self.addPageTemplates([
            PageTemplate(id="cover", frames=[cover], onPage=self._cover_bg),
            PageTemplate(id="body", frames=[frame], onPage=self._chrome),
        ])

    def _cover_bg(self, canvas, doc):
        canvas.saveState()
        canvas.setFillColor(NAVY)
        canvas.rect(0, 0, PW, PH, fill=1, stroke=0)
        canvas.setFillColor(ACCENT)
        canvas.rect(0, PH - 70 * mm, PW, 4 * mm, fill=1, stroke=0)
        canvas.setFillColor(BLUE)
        canvas.rect(0, PH - 70 * mm - 1.4 * mm, PW, 1.4 * mm, fill=1, stroke=0)
        canvas.restoreState()

    def _chrome(self, canvas, doc):
        canvas.saveState()
        canvas.setFillColor(NAVY)
        canvas.rect(0, PH - 12 * mm, PW, 12 * mm, fill=1, stroke=0)
        canvas.setFillColor(colors.white)
        canvas.setFont("Helvetica-Bold", 8.5)
        canvas.drawString(MARGIN, PH - 8 * mm, "KeynetikPOS")
        canvas.setFont("Helvetica", 8)
        canvas.setFillColor(colors.HexColor("#b9c6e0"))
        canvas.drawRightString(PW - MARGIN, PH - 8 * mm, self.running_title)
        canvas.setStrokeColor(LINE)
        canvas.setLineWidth(0.5)
        canvas.line(MARGIN, 13 * mm, PW - MARGIN, 13 * mm)
        canvas.setFont("Helvetica", 8)
        canvas.setFillColor(MUTED)
        canvas.drawString(MARGIN, 9 * mm, self.footer_tagline)
        canvas.drawRightString(PW - MARGIN, 9 * mm, "Page %d" % doc.page)
        canvas.restoreState()


# ---------------------------------------------------------------- helpers
def bullets(items, st=BULLET):
    return ListFlowable(
        [ListItem(Paragraph(t, st), leftIndent=10, value="•") for t in items],
        bulletType="bullet", start="•", leftIndent=14, bulletColor=BLUE,
        bulletFontSize=7, spaceBefore=1, spaceAfter=1,
    )


def steps(items):
    return ListFlowable(
        [ListItem(Paragraph(t, STEP), leftIndent=12) for t in items],
        bulletType="1", leftIndent=18, bulletColor=BLUE, bulletFontName="Helvetica-Bold",
        bulletFontSize=9.4, spaceBefore=1, spaceAfter=2,
    )


def table(rows, col_widths=None, header=True, center_cols=()):
    if col_widths is None:
        ncols = len(rows[0])
        avail = PW - 2 * MARGIN
        if ncols == 1:
            col_widths = [avail]
        else:
            first = 44 * mm
            rest = (avail - first) / (ncols - 1)
            col_widths = [first] + [rest] * (ncols - 1)
    data = []
    for i, r in enumerate(rows):
        if header and i == 0:
            data.append([Paragraph(c, CELLH) for c in r])
        else:
            row = []
            for j, c in enumerate(r):
                if j == 0:
                    row.append(Paragraph(c, CELLB))
                elif j in center_cols:
                    row.append(Paragraph(c, style("cc", parent=CELL, alignment=TA_CENTER)))
                else:
                    row.append(Paragraph(c, CELL))
            data.append(row)
    t = Table(data, colWidths=col_widths, repeatRows=1 if header else 0)
    cmds = [
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 6),
        ("RIGHTPADDING", (0, 0), (-1, -1), 6),
        ("TOPPADDING", (0, 0), (-1, -1), 4),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
        ("LINEBELOW", (0, 0), (-1, -1), 0.4, LINE),
        ("ROWBACKGROUNDS", (0, 1 if header else 0), (-1, -1), [colors.white, GREYBG]),
    ]
    if header:
        cmds += [("BACKGROUND", (0, 0), (-1, 0), BLUE),
                 ("TOPPADDING", (0, 0), (-1, 0), 6),
                 ("BOTTOMPADDING", (0, 0), (-1, 0), 6)]
    t.setStyle(TableStyle(cmds))
    return t


def callout(title, text, bg=LIGHT, bar=BLUE):
    inner = [Paragraph("<b>%s</b>" % title, style("coT", fontName="Helvetica-Bold",
             fontSize=9.5, textColor=NAVY, leading=13, spaceAfter=2))]
    if text:
        inner.append(Paragraph(text, style("coB", fontName="Helvetica", fontSize=9,
                     textColor=DARK, leading=13)))
    box = Table([[inner]], colWidths=[PW - 2 * MARGIN - 8])
    box.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), bg),
        ("LEFTPADDING", (0, 0), (-1, -1), 12),
        ("RIGHTPADDING", (0, 0), (-1, -1), 10),
        ("TOPPADDING", (0, 0), (-1, -1), 8),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 8),
        ("LINEBEFORE", (0, 0), (0, -1), 3, bar),
    ]))
    return box


def code_block(lines):
    """Monospace fenced block on a tinted background."""
    def esc(l):
        return (l.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
                 .replace(" ", "&nbsp;"))
    para = Paragraph("<br/>".join(esc(l) for l in lines), CODE)
    box = Table([[para]], colWidths=[PW - 2 * MARGIN - 8])
    box.setStyle(TableStyle([
        ("BACKGROUND", (0, 0), (-1, -1), CODEBG),
        ("LEFTPADDING", (0, 0), (-1, -1), 10),
        ("RIGHTPADDING", (0, 0), (-1, -1), 10),
        ("TOPPADDING", (0, 0), (-1, -1), 7),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 7),
        ("LINEBEFORE", (0, 0), (0, -1), 3, BLUE),
    ]))
    return box


def cover(title, subtitle, meta_lines):
    """Returns the flowables for the navy cover page."""
    flow = [Spacer(1, 78 * mm),
            Paragraph("KeynetikPOS", style("cbrand", fontName="Helvetica-Bold",
                      fontSize=16, textColor=ACCENT, alignment=TA_CENTER, leading=20)),
            Spacer(1, 6 * mm),
            Paragraph(title, COVER_TITLE),
            Spacer(1, 6 * mm),
            Paragraph(subtitle, COVER_SUB),
            Spacer(1, 60 * mm)]
    for m in meta_lines:
        flow.append(Paragraph(m, COVER_SMALL))
    flow += [NextPageTemplate("body"), PageBreak()]
    return flow


def toc(entries):
    """entries: list of (number, title). Renders a simple two-column TOC table."""
    rows = [[Paragraph(n, TOCNUM), Paragraph(t, TOC)] for n, t in entries]
    t = Table(rows, colWidths=[14 * mm, PW - 2 * MARGIN - 14 * mm])
    t.setStyle(TableStyle([
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("LEFTPADDING", (0, 0), (-1, -1), 0),
        ("TOPPADDING", (0, 0), (-1, -1), 1),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 1),
    ]))
    return t
