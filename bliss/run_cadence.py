import subprocess
import time
from pathlib import Path

# 1. Define input and output directories
# change those to your specific input and output folders
input_dir = Path("/datax/users/obs/mastrone/input")
output_dir = Path("/datax/users/obs/mastrone/output")

# Ensure the output directory exists
output_dir.mkdir(parents=True, exist_ok=True)

# 2. FILE SELECTION
# Option A (Automatic - Recommended): Gets all .h5 files in the folder and sorts them alphabetically
cadence_files = sorted(input_dir.glob("*.h5"))

# Option B (Manual): Uncomment the lines below if you want to process specific files only
# cadence_files = [
#     input_dir / "guppi_60703_16858_008036_TIC286923464_ON_0001.0000.h5",
#     input_dir / "guppi_60703_16858_008036_TIC286923464_OFF_0002.0000.h5",
# ]

# 3. Define the base parameters for your command
base_cmd = [
    "./build/bliss/bliss_find_hits", # this should be the default path, if it dosen't work try putting the full path instead
    "-e", "/datax/users/obs/mastrone/input/gbt_pfb.f32", # change this path to your f32 file if you have one, otherwise just comment the full line
    "--nofilter-zero-drift",
    "-d", "cuda:0",
    "-md", "-2",
    "-MD", "2",
    "-s", "20",
    "--number-coarse", "64",
    "--nchan-per-coarse", "0",
    "--filter-sigmaclip",
    "--filter-low-sk",
    "--filter-high-sk",
    "--distance", "10"
]

print(f"[*] Found {len(cadence_files)} files in the cadence.")
print(f"[*] Starting processing...")
start_time_totale = time.time()

for h5_file in cadence_files:
    print("\n" + "="*70)
    print(f"[*] Processing: {h5_file.name}")
    
    # Automatically construct the output filename
    output_filename = f"refeq{h5_file.stem}.dat" 
    output_path = output_dir / output_filename
    
    # Assemble the final command
    current_cmd = base_cmd.copy()
    current_cmd.insert(1, str(h5_file))
    current_cmd.extend(["--output", str(output_path)])
    
    # Print the exact command that is about to be launched (useful for debugging)
    print("Command:", " ".join(current_cmd))
    
    # --- COMMAND EXECUTION ---
    start_time_file = time.time()
    result = subprocess.run(current_cmd)
    
    elapsed = time.time() - start_time_file
    
    if result.returncode == 0:
        print(f"[+] File successfully completed in {elapsed:.1f} seconds.")
    else:
        print(f"[!] ERROR (Code {result.returncode}) on: {h5_file.name}")
        print("[!] Interrupting cadence execution.")
        break

tempo_totale = (time.time() - start_time_totale) / 60
print("\n" + "="*70)
print(f"[*] CADENCE COMPLETED in {tempo_totale:.2f} minutes.")