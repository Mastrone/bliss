### `README_CONDA.md`

Instructions on how I install BLISS in this specific environment

# Environment Management and Compilation (Conda)

## 1. Environment Installation

To compile the code, test it, and work on it, you need to create an isolated Conda environment with all the necessary dependencies (CMake, CUDA, HDF5, FFTW, etc.). 

Use the `bliss_environment.yml` file present in this folder:

```bash
# Create the environment
conda env create -f bliss_environment.yml

# Activate the environment
conda activate bliss


---

## 2. Compilation and Installation (Build from Source)

Once the Conda environment is activated, you must compile the C++/CUDA source code. Make sure you are in the main project folder.

### Procedure:

1. **Prepare the build folder:**

```bash
mkdir build && cd build

```

2. **Configure with CMake:**

> **WARNING:** The paths specified in this command (`/usr/bin/gcc-9`, `/usr/local/cuda/...`) are specific to a particular machine and Ubuntu version. **You will need to adapt them to your system.**
> Read the comments next to each line to know which command to run in the terminal to find the correct path.

```bash
cmake .. \
  -DCMAKE_PREFIX_PATH=$CONDA_PREFIX \
  -DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.10 \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc \      # Find the path by running: which nvcc
  -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++-9 \          # Find the path by running: which g++-9
  -DCMAKE_C_COMPILER=/usr/bin/gcc-9 \                  # Find the path by running: which gcc-9
  -DCMAKE_CXX_COMPILER=/usr/bin/g++-9 \                # Find the path by running: which g++-9
  -DCMAKE_CUDA_ARCHITECTURES=86 \                      # GPU Architecture (e.g., 86 for RTX 3000 series/A100, 89 for RTX 4000 series, 75 for RTX 2000)
  -DCMAKE_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu       # Standard path for system libraries on Ubuntu/Debian. To find it run: gcc -print-multiarch

```

*(The `-DCMAKE_PREFIX_PATH` and `-DCMAKE_INSTALL_PREFIX` flags ensure that CMake finds libraries like HDF5 inside the Conda environment and installs the final binaries there, keeping the system clean).*

3. **Compile and Install:**

```bash
# Compile using all available cores to speed up the process
make -j$(nproc)

# Install BLISS binaries in the Conda environment (in the $CONDA_PREFIX/bin folder)
# needed to run bliss from any folder, however the executable file is present even without this step
make install

```

After installation, you will be able to use the BLISS executables (e.g., `bliss_event_search`) from any folder on your computer, as long as the Conda environment is active.

---

```

```
