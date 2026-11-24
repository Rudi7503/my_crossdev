import subprocess
import pathlib
import sys

root = pathlib.Path("/mnt/d/programmier_ordner/my_crossdev/68080-db")
(root / "build-v6").mkdir(exist_ok=True)
(root / "bin-v6").mkdir(exist_ok=True)
h = (root / "harness-v6.asm").read_text()
vasm = "/home/papa/Vampire_V4/ApolloCrossDev/Compilers/GCC-6.50-Dev/bin/vasmm68k_mot"

ALL_IDS = [f"HUR-{n:03d}" for n in range(1, 85)]
ids = sys.argv[1:] if len(sys.argv) > 1 else ALL_IDS
for i in ids:
    asm = root / "build-v6" / f"{i}.asm"
    exe = root / "bin-v6" / i
    asm.write_text(
        h.replace("; @@INCLUDE_SEQ@@", f'        include "seq/{i}.i"').replace("@@ID@@", i)
    )
    subprocess.run([vasm, "-quiet", "-Fhunkexe", "-m68080", "-o", str(exe), str(asm)], cwd=str(root), check=True)
    print(f"BUILT {exe} ({exe.stat().st_size} bytes)")
