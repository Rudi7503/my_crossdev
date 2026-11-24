import subprocess
import pathlib

db_root = pathlib.Path("D:/programmier_ordner/my_crossdev/68080-db")
bin_dir = db_root / "bin-v6"
log_dir = db_root / "log-v6"

ip = "192.168.178.50"
amiga_tool = "/home/papa/Vampire_V4/ApolloCrossDev/Tools/amiga_tool.py"

for seq_id in ["HUR-102", "HUR-103"]:
    print(f"--- Running {seq_id} on V4 ---")
    wsl_bin = f"/mnt/d/programmier_ordner/my_crossdev/68080-db/bin-v6/{seq_id}"
    
    # 1. upload via amiga_tool
    res = subprocess.run(["wsl", "python3", amiga_tool, "write", f"RAM:{seq_id}", wsl_bin], capture_output=True, text=True)
    if res.returncode != 0:
        print(f"Error uploading {seq_id}: {res.stderr}")
        continue
    
    # 2. protect +e
    subprocess.run(["ash.exe", ip, f"Protect RAM:{seq_id} +e"], capture_output=True)
    
    # 3. execute
    subprocess.run(["ash.exe", ip, f"RAM:{seq_id} RAM:LOGS/{seq_id}.log"], capture_output=True)
    
    # 4. read log back
    log_file = log_dir / f"{seq_id}.log"
    res = subprocess.run(["wsl", "python3", amiga_tool, "read", f"RAM:LOGS/{seq_id}.log"], capture_output=True, text=True)
    if res.returncode == 0:
        log_file.write_text(res.stdout)
        print(f"Successfully saved {log_file}")
        lines = res.stdout.strip().splitlines()
        for l in lines[-12:]:
            print("  " + l)
    else:
        print(f"Error reading log for {seq_id}: {res.stderr}")
