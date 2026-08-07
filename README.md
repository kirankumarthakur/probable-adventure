# Probable Adventure

A C++ multiplayer-style tactical survival simulation driven by a Multi-Layer Perceptron (MLP) Neural Network, built with an Entity Component System (**EnTT**), **SFML 3**, and **Dear ImGui**.

---

> [!NOTE]
> This README.md is generated with the help of AI (Google Gemini)

## Prerequisites

Before building, ensure you have the following installed on your system:

* A compiler supporting **C++23** (GCC 13+, Clang 16+, or MSVC 2022+)
* **CMake** (version 3.20 or higher)
* Development libraries for **SFML 3** dependencies (OpenGL, X11/Wayland, UDev, FreeType, etc. on Linux systems)

---

## Building Instructions

Follow these steps to configure and build the project from source:

1. **Clone or open the repository terminal** in the project root directory.
2. **Create a build directory:**
```bash
mkdir build
cd build

```


3. **Configure the project with CMake:**
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..

```


4. **Compile the executables:**
```bash
cmake --build . --config Release

```


---

## Running the Application

Once built, the binaries will be located in your `build` directory (or subdirectories depending on your generator).

### 1. Training / Simulation Engine (`probable_adventure`)

Runs the genetic algorithm and trains the neural network across multiple generations, saving the best brains and simulation snapshots.

```bash
./probable_adventure

```

**Optional Command-Line Arguments:**
You can customize the training parameters directly via arguments:

* `--pop <int>`: Population size per generation (default: `10`)
* `--gen <int>`: Total number of generations to train (default: `20`)
* `--mut-rate <float>`: Mutation probability rate (default: `0.1`)
* `--mut-str <float>`: Mutation weight shift strength (default: `0.5`)

*Example:*

```bash
./probable_adventure --pop 30 --gen 50 --mut-rate 0.15

```

### 2. Visual Replay Viewer (`probable_adventure_viewer`)

Launches the SFML window with ImGui playback controls to watch the trained agent, toggle diagnostic hitboxes, raycasts, and track simulation stats.

```bash
./probable_adventure_viewer

```
