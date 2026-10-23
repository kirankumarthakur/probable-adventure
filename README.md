# Portable Adventure

A 2D simulation where an adventurer learns to survive and fight slimes in an arena using a neural network (MLP) trained with a genetic algorithm. Built in C++23 using SFML 3, EnTT, and Clay UI.

## Demos

### Training View
Shows the training process and metrics across generations (fitness score, error, survival time, kills, damage dealt, and weight norm).

<video src="https://github.com/user-attachments/assets/aec0eff4-86cf-4819-bfd5-52073746320a" controls width="100%"></video>

### Simulation / Inspector View
Lets you load saved model checkpoints, watch the policy run live in the arena, adjust simulation speed, or trigger abilities manually.

<video src="https://github.com/user-attachments/assets/015a0909-b971-4cb4-9a15-dbb9fca50b6e" controls width="100%"></video>

## Requirements

* C++23 compiler (MSVC 2022, GCC 13+, or Clang 16+)
* CMake 3.20 or newer

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The binary will be built in:
* **Windows**: `build/Release/probable_adventure.exe`
* **Linux/macOS**: `build/probable_adventure`

## Running

Make sure to run the executable from the repository root so it can locate the `assets/` directory:

```powershell
.\build\Release\probable_adventure.exe
```

### Headless Training
You can also run training directly in the terminal without opening the window:

```powershell
.\build\Release\probable_adventure.exe --headless --pop 30 --gen 50
```

CLI options:
* `--pop <int>`: Population size per generation (default: `30`)
* `--gen <int>`: Number of generations (default: `50`)
* `--mut-rate <float>`: Mutation rate (default: `0.10`)
* `--mut-str <float>`: Mutation strength (default: `0.35`)

## How It Works

* **Inputs & Outputs**: The neural network takes 28 sensor inputs (HP, stamina, skill cooldowns, wall distance, 11 vision rays, and 8 sound rays). It outputs movement direction (X, Y) and decisions for 3 abilities: Cleave, Shield, and Warcry.
* **Scoring**: Agents are rewarded for dealing damage, killing slimes, using shields to block attacks, and surviving. They are penalized for taking damage and hugging walls.
* **Checkpoints**: Top models are saved as `.safetensors` in the `checkpoints/` folder and can be reloaded in the Analyze tab.

## Controls (Analyze View)

* `Space`: Play / Pause simulation
* `Q`: Cleave
* `W`: Shield
* `E`: Warcry
* `1`, `2`, `3`: Switch tabs (Train / Analyze / Configure)
