#!/usr/bin/env python3
import sys, time
sys.path.append("/home/papa/Vampire_V4/ApolloCrossDev/Tools/amimcp/server")
from amiga import Amiga

if len(sys.argv) < 2:
    print("Usage: run_amiga.py <command_str> [output_file]")
    sys.exit(1)

cmd = sys.argv[1]
outfile = sys.argv[2] if len(sys.argv) > 2 else "RAM:out.txt"

TOKEN = "6efa1cf4a53fe3d2905989e94e9aa5108a128f3b998d153b"
am = Amiga("192.168.178.50", token=TOKEN, timeout=5.0)

full_cmd = f"{cmd} >{outfile}"
am.menu_select("Execute command...")
time.sleep(0.3)
am.input_text(full_cmd)
time.sleep(0.3)
am.ui_click("Execute")

# Poll for output
time.sleep(1.0)
for attempt in range(10):
    try:
        data = am.read_file(outfile)
        if len(data) > 0:
            print(data.decode("latin-1", "replace"))
            break
    except Exception:
        pass
    time.sleep(0.5)
