from pathlib import Path
from html import escape
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parent
image = Image.new('RGB', (1900, 1060), 'white')
d = ImageDraw.Draw(image)
svg = ['<svg xmlns="http://www.w3.org/2000/svg" width="1900" height="1060" viewBox="0 0 1900 1060">', '<rect width="1900" height="1060" fill="white"/>']
font_path = 'C:/Windows/Fonts/msyh.ttc'

def line(points, color='#263445', width=4):
    d.line(points, fill=color, width=width)
    svg.append(f'<polyline points="{" ".join(f"{x},{y}" for x,y in points)}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linejoin="round"/>')

def rect(box, fill='#f4f7fb'):
    d.rectangle(box, fill=fill, outline='#263445', width=3)
    x,y,x2,y2=box
    svg.append(f'<rect x="{x}" y="{y}" width="{x2-x}" height="{y2-y}" fill="{fill}" stroke="#263445" stroke-width="3"/>')

def label(x,y,text,size=30,color='#162438',center=False):
    font=ImageFont.truetype(font_path,size)
    if center:
        x-=d.textlength(text,font=font)/2
    d.text((x,y),text,font=font,fill=color,anchor='lt')
    svg.append(f'<text x="{x}" y="{y+size}" font-family="Microsoft YaHei, sans-serif" font-size="{size}" fill="{color}">{escape(text)}</text>')

def dot(x,y):
    d.ellipse((x-7,y-7,x+7,y+7),fill='#263445')
    svg.append(f'<circle cx="{x}" cy="{y}" r="7" fill="#263445"/>')

label(950,40,'三线制 4–20 mA 位移传感器 → S32K144 ADC',44,center=True)
rect((350,180,700,580))
label(525,285,'位移传感器',38,center=True)
label(525,345,'三线制 / 4–20 mA',27,center=True)
label(375,205,'红线 V+',26,color='#b32630')
label(375,485,'黑线 0V',26)
line([(60,250),(350,250)],'#b32630')
label(60,160,'外部电源正极',28)
label(60,200,'+12–24 V¹',30,color='#b32630')
line([(350,520),(270,520),(270,800),(1600,800),(1600,580)])
line([(60,800),(270,800)])
label(60,840,'电源负极（0 V）',28)
dot(270,800)

rect((1280,180,1810,580))
label(1545,225,'S32K144 开发板',36,center=True)
line([(700,400),(1000,400),(1280,400)],'#176fb0')
label(745,350,'灰线 IOUT',27,color='#176fb0')
label(1040,350,'A：0.4–2.0 V',28,color='#176fb0')
label(1300,365,'PTB3 / ADC0_SE7',30)
label(1300,425,'ADC 测量节点 A 相对 GND 的电压',25)
label(1560,535,'GND',25)
dot(1000,400)
line([(1000,400),(1000,480)],'#176fb0')
rect((965,480,1035,640),'white')
line([(1000,640),(1000,800)])
label(1070,505,'R1 = 100 Ω',32)
label(1070,550,'采样电阻',27)
label(865,535,'I ↓',29,color='#176fb0')
dot(1000,800)
label(1130,740,'公共地 GND',28)
line([(1000,800),(1000,825)])
line([(965,825),(1035,825)])
line([(976,838),(1024,838)])
line([(987,851),(1013,851)])
label(950,910,'U_ADC = I_OUT × R1 = 4–20 mA × 100 Ω = 0.4–2.0 V',34,center=True)
label(950,965,'ADC 接节点 A；电阻下端、电源负极和开发板 GND 共地。',28,center=True)
label(950,1010,'¹ 传感器供电以铭牌为准；开发板由其正常电源供电（图中省略）。',23,center=True)
svg.append('</svg>')
(root/'displacement_4_20ma_circuit.svg').write_text('\n'.join(svg),encoding='utf-8')
image.save(root/'displacement_4_20ma_circuit.png')
