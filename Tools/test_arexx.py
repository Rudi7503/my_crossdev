import sys
sys.path.append("/home/papa/Vampire_V4/ApolloCrossDev/Tools/amimcp/server")
from amiga import Amiga

TOKEN = "6efa1cf4a53fe3d2905989e94e9aa5108a128f3b998d153b"
am = Amiga("192.168.178.50", token=TOKEN, timeout=5.0)

try:
    rc, out = am.arexx("say 'hello from ARexx'")
    print("ARexx result:", rc, out)
except Exception as e:
    print("ARexx err:", e)
