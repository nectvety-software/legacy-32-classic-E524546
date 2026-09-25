from pathlib import Path
try: from PIL import Image,ImageDraw
except ImportError: raise SystemExit('Install Pillow to create sample icons')
out=Path(__file__).resolve().parents[1] / 'sd/System/Apps/Inbox/welcome_icon.png'
im=Image.new('RGB',(32,32),(0,84,145));d=ImageDraw.Draw(im)
d.rectangle((7,4,24,27),fill='white',outline=(14,30,70))
for y in (10,14,18,22):d.line((11,y,21,y),fill=(65,140,230),width=1)
d.rectangle((19,21,25,29),fill=(255,202,30))
im.save(out)
print(out)
