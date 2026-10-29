import sys, time
sys.path.append("/home/papa/Vampire_V4/ApolloCrossDev/Tools/amimcp/server")
from amiga import Amiga

TOKEN = "6efa1cf4a53fe3d2905989e94e9aa5108a128f3b998d153b"
am = Amiga("192.168.178.50", token=TOKEN, timeout=10.0)

print("Opening Execute command window...")
am.menu_select("Execute command...")
time.sleep(0.6)

print("Typing command...")
am.input_text("RAM:VideoOpt >RAM:videoopt_out.txt")
time.sleep(0.6)

print("Clicking Execute...")
am.ui_click("Execute")
time.sleep(2.0)

print("Checking if output file exists...")
for i in range(10):
    try:
        data = am.read_file("RAM:videoopt_out.txt")
        print("\n=== BENCHMARK OUTPUT ===\n")
        print(data.decode("latin-1", "replace"))
        sys.exit(0)
    except Exception as e:
        time.sleep(1.0)

print("Failed to read output file after 10 seconds.")
