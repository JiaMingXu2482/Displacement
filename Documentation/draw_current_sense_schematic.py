from pathlib import Path
from html import escape
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parent
W,H = 1900,1400
im=Image.new('RGB',(W,H),'white'); d=ImageDraw.Draw(im)
svg=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">','<rect width="100%" height="100%" fill="white"/>']

def wire(points,width=3):
    d.line(points,fill='black',width=width)
    svg.append(f'<polyline points="{" ".join(f"{x},{y}" for x,y in points)}" fill="none" stroke="black" stroke-width="{width}"/>')

def text(x,y,s,size=28,center=False):
    f=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',size)
    if center: x-=d.textlength(s,font=f)/2
    d.text((x,y),s,font=f,fill='black',anchor='lt')
    svg.append(f'<text x="{x}" y="{y+size}" font-family="Microsoft YaHei, sans-serif" font-size="{size}">{escape(s)}</text>')

def box(x,y,w,h):
    d.rectangle((x,y,x+w,y+h),fill='white',outline='black',width=3)
    svg.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" fill="white" stroke="black" stroke-width="3"/>')

def dot(x,y):
    d.ellipse((x-6,y-6,x+6,y+6),fill='black')
    svg.append(f'<circle cx="{x}" cy="{y}" r="6"/>')

def ground(x,y):
    wire([(x,y),(x,y+20)])
    for k,w in enumerate((32,21,10)): wire([(x-w,y+20+13*k),(x+w,y+20+13*k)])

text(950,35,'0–2 A 电流检测放大器原理图',40,True)
text(950,95,'U1：INA240A1PW（TSSOP-8） · 固定增益 20 V/V · 单向测量',29,True)

# Current path: two terminals continue into the valve's power/recirculation loop.
wire([(120,280),(550,280)],5)
box(550,250,180,60)
wire([(730,280),(1680,280)],5)
dot(400,280); dot(900,280)
text(400,225,'A',30,True); text(900,225,'B',30,True)
text(640,335,'R1  0.05 Ω / 0.5 W / 0.1%',29,True)
wire([(1070,190),(1280,190)])
wire([(1260,178),(1280,190),(1260,202)])
text(1175,145,'I = 0–2 A',29,True)
text(120,325,'回路端 1',25); text(1550,325,'回路端 2',25)
text(950,1330,'R1 串入线圈电流通路；本图仅画检测前端，省略功率驱动与续流电路。',25,True)

# Kelvin sense lines do not carry load current. Arrange input pins to avoid crossings.
wire([(400,280),(400,790),(800,790)])
wire([(900,280),(900,455),(660,455),(660,630),(800,630)])
text(145,590,'两根独立检测线',25)
text(145,630,'从 R1 焊盘引出',25)

box(800,550,440,390)
text(800,495,'U1',29)
text(765,585,'3',23); text(820,610,'IN−',28)
text(765,745,'2',23); text(820,770,'IN+',28)
text(1030,685,'G = 20',31,True)

# Supply and local 100 nF bypass capacitor.
wire([(1060,550),(1060,475),(1450,475),(1450,545)])
dot(1060,475)
wire([(1060,475),(1060,440)])
wire([(1046,455),(1060,440),(1074,455)])
text(1130,432,'+3.3 V',29)
text(1020,505,'5',23); text(1038,567,'VS',25)
wire([(1415,545),(1485,545)])
wire([(1415,565),(1485,565)])
wire([(1450,565),(1450,610)])
ground(1450,610)
text(1520,525,'C1  100 nF',27)
text(1520,570,'靠近 U1 的 VS / GND',23)

# Output to ADC, separate from the power current loop.
wire([(1240,730),(1640,730)])
wire([(1640,713),(1665,730),(1640,747),(1640,713)])
text(1130,705,'OUT',28); text(1270,685,'8',23)
text(1675,705,'ADC_I',29)
text(1425,775,'约 0–2.0 V',28)

# NC left unconnected; reference pins and analogue ground tied to shared ground.
wire([(1240,865),(1310,865)])
wire([(1300,855),(1320,875)]); wire([(1300,875),(1320,855)])
text(1160,840,'NC',24); text(1260,823,'1',23)
for x,pin,name in ((900,'4','GND'),(1040,'7','REF1'),(1180,'6','REF2')):
    text(x,895,name,23,True)
    text(x+12,950,pin,23)
    wire([(x,940),(x,1040)])
wire([(900,1040),(1180,1040)])
dot(1040,1040)
ground(1040,1040)
text(1040,1110,'GND（与 MCU 共地）',26,True)

text(950,1185,'V_OUT ≈ 20 × (V_A − V_B) = 20 × I × 0.05 Ω',32,True)
text(950,1240,'2 A → 0.10 V 压差 → 2.0 V 输出；零点附近需考虑输出下限及失调。',26,True)
svg.append('</svg>')
(root/'current_sense_ina240_schematic.svg').write_text('\n'.join(svg),encoding='utf-8')
im.save(root/'current_sense_ina240_schematic.png')
