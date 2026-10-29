import sys
sys.path.append("/home/papa/Vampire_V4/ApolloCrossDev/Tools/amimcp/server")
from amiga import Amiga

TOKEN = "6efa1cf4a53fe3d2905989e94e9aa5108a128f3b998d153b"
am = Amiga("192.168.178.50", token=TOKEN, timeout=5.0)

print("Clicking Suspend at 572, 411...")
am.input_click(572, 411)
print("Clicked!")

import time
time.sleep(1.0)
print("Checking UI tree...")
print(am.ui_tree())
