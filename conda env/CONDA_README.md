
### `README_CONDA.md`

Istruzioni su come io installo BLISS in questo specifico ambiente

```markdown
# Gestione dell'Ambiente e Compilazione (Conda)

## 1. Installanzione dell'ambiente

Per compilare il codice, testarlo e lavorarci, devi creare un ambiente Conda isolato con tutte le dipendenze necessarie (CMake, CUDA, HDF5, FFTW, ecc.). 

Usa il file `bliss_environment.yml` presente in questa cartella:

```bash
# Crea l'ambiente
conda env create -f bliss_environment.yml

# Attiva l'ambiente
conda activate bliss

---


## 2. Compilazione e Installazione (Build from Source)

Una volta attivato l'ambiente Conda, devi compilare il codice sorgente C++/CUDA. Assicurati di essere nella cartella principale del progetto.

### Procedura:

1. **Prepara la cartella di build:**
```bash
mkdir build && cd build

```


2. **Configura con CMake:**
> **ATTENZIONE:** I percorsi (path) specificati in questo comando (`/usr/bin/gcc-9`, `/usr/local/cuda/...`) sono specifici per una particolare macchina e versione di Ubuntu. **Dovrai adattarli al tuo sistema.**
> Leggi i commenti a lato di ogni riga per sapere quale comando lanciare nel terminale per trovare il percorso corretto.


```bash
cmake .. \
  -DCMAKE_PREFIX_PATH=$CONDA_PREFIX \
  -DCMAKE_INSTALL_PREFIX=$CONDA_PREFIX \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.10 \
  -DCMAKE_CUDA_COMPILER=/usr/local/cuda/bin/nvcc \     # Trova il path eseguendo: which nvcc
  -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++-9 \          # Trova il path eseguendo: which g++-9
  -DCMAKE_C_COMPILER=/usr/bin/gcc-9 \                  # Trova il path eseguendo: which gcc-9
  -DCMAKE_CXX_COMPILER=/usr/bin/g++-9 \                # Trova il path eseguendo: which g++-9
  -DCMAKE_CUDA_ARCHITECTURES=86 \                      # Architettura GPU (es: 86 per RTX serie 3000/A100, 89 per RTX serie 4000, 75 per RTX 2000)
  -DCMAKE_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu       # Path standard delle librerie di sistema su Ubuntu/Debian. Per trovarlo esegui: gcc -print-multiarch

```


*(I flag `-DCMAKE_PREFIX_PATH` e `-DCMAKE_INSTALL_PREFIX` assicurano che CMake trovi le librerie come HDF5 dentro l'ambiente Conda e vi installi i binari finali, mantenendo il sistema pulito).*
3. **Compila e Installa:**
```bash
# Compila usando tutti i core disponibili per velocizzare il processo
make -j$(nproc)

# Installa i binari di BLISS nell'ambiente Conda (nella cartella $CONDA_PREFIX/bin)
# serve per runnare bliss in ogni cartella, il file eseguibile è presente anche senza questo passaggio però
make install

```



Dopo l'installazione, potrai usare gli eseguibili di BLISS (es. `bliss_event_search`) da qualsiasi cartella del tuo computer, a patto che l'ambiente Conda sia attivo.

---

