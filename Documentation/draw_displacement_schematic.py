from pathlib import Path
from html import escape
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parent
W, H = 1500, 950
im = Image.new('RGB', (W,H), 'white')
d = ImageDraw.Draw(im)
parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">', '<rect width="100%" height="100%" fill="white"/>']
font_path = 'C:/Windows/Fonts/msyh.ttc'

def wire(points, width=3):
    d.line(points, fill='black', width=width)
    parts.append(f'<polyline points="{" ".join(f"{x},{y}" for x,y in points)}" fill="none" stroke="black" stroke-width="{width}"/>')

def text(x,y,s,size=28,center=False):
    f=ImageFont.truetype(font_path,size)
    if center: x-=d.textlength(s,font=f)/2
    d.text((x,y),s,font=f,fill='black',anchor='lt')
    parts.append(f'<text x="{x}" y="{y+size}" font-family="Microsoft YaHei, sans-serif" font-size="{size}" fill="black">{escape(s)}</text>')

def circle(x,y,r,fill='white',width=3):
    d.ellipse((x-r,y-r,x+r,y+r),fill=fill,outline='black',width=width)
    parts.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{fill}" stroke="black" stroke-width="{width}"/>')

def rectangle(x,y,w,h):
    d.rectangle((x,y,x+w,y+h),fill='white',outline='black',width=3)
    parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="white" stroke="black" stroke-width="3"/>')

def ground(x,y):
    wire([(x,y),(x,y+22)])
    wire([(x-30,y+22),(x+30,y+22)])
    wire([(x-20,y+34),(x+20,y+34)])
    wire([(x-9,y+46),(x+9,y+46)])

text(750,55,'4–20 mA 位移信号采样电路',38,True)
text(750,110,'传感器输出端的等效原理图',26,True)

# Ideal current output of the separately powered three-wire transmitter.
wire([(360,370),(360,270),(800,270),(1140,270)])
circle(360,445,75)
wire([(360,488),(360,400)],4)
wire([(344,420),(360,399),(376,420)],4)
wire([(360,520),(360,690),(800,690),(800,580)])
text(125,385,'I1',32)
text(105,435,'4–20 mA',30)
text(75,485,'传感器等效电流源',25)

# IEC resistor symbol, shunt connected to ground.
wire([(800,270),(800,370)])
rectangle(772,370,56,210)
text(858,425,'R1',32)
text(858,475,'100 Ω',32)
circle(800,270,6,'black',1)
circle(800,690,6,'black',1)
ground(800,690)
text(800,758,'GND',27,True)

# ADC is an electrical input port, not an illustrated board.
wire([(1140,250),(1170,270),(1140,290),(1140,250)])
text(1190,253,'ADC_IN',30)
text(1140,310,'PTB3 / ADC0_SE7',25)
text(1140,350,'S32K144',25)
text(805,215,'V_ADC',30)
text(750,845,'V_ADC = I1 × R1 = 0.4–2.0 V',32,True)

parts.append('</svg>')
(root/'displacement_sampling_schematic.svg').write_text('\n'.join(parts),encoding='utf-8')
im.save(root/'displacement_sampling_schematic.png')
