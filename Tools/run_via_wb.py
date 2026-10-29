import sys, time
sys.path.append("/home/papa/Vampire_V4/ApolloCrossDev/Tools/amimcp/server")
from amiga import Amiga

TOKEN = "6efa1cf4a53fe3d2905989e94e9aa5108a128f3b998d153b"
am = Amiga("192.168.178.50", token=TOKEN, timeout=5.0)

print("Opening Execute command...")
am.menu_select("Execute command...")
time.sleep(0.5)

print("Typing command...")
am.input_text("RAM:test_pavgb >RAM:pavgb_out.txt")
time.sleep(0.5)

print("Clicking Execute...")
am.ui_click("Execute")
time.sleep(1.0)

print("Reading output...")
try:
    data = am.read_file("RAM:pavgb_out.txt")
    print("OUTPUT:\n" + data.decode("latin-1"))
except Exception as e:
    print("Read err:", e)
