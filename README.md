# Tetru

Tetru is a fast, terminal-based falling-block arcade game written in pure C using `ncurses`. It features a custom TUI engine, responsive sliding controls, multiple game modes, an adaptive AI opponent, and persistent local stats tracking.

Made by **Eskrid**.

---

## Features

- **Dynamic Animated TUI**: Live drifting starfield background, animated rainbow title banner, frame pulse effects, and clean borders.
- **Visual Feedback & Effects**:
  - Spark particle bursts on line clears and hard drops.
  - Screen shake on multi-line clears and incoming attacks.
  - White flash row animations.
  - Action banners for doubles, triples, Tetru quads, and combo streaks.
  - Dynamic ghost piece with color-matched outlines.
- **Multiple Game Modes**:
  1. **Classic Mode**: Endless score chase with progressive level speedups.
  2. **40-Line Sprint**: Speedrun challenge with live stopwatch tracking.
  3. **Survival Rush**: Bottom-up garbage attacks with escalating pressure.
  4. **VS Computer (AI Battle)**: Real-time duel against an AI opponent featuring attack cancelation and garbage delivery.
- **5 AI Difficulty Levels**:
  - Beginner
  - Easy
  - Medium
  - Hard
  - Impossible
- **Fluid & Responsive Controls**: 7-bag randomizer, wall kicks, and a 450ms lock delay for sliding adjustments.
- **Persistent Local Records**: Tracks career stats, high scores, best sprint times, wins, and losses in `tetru_stats.dat`.

---

## Controls

| Key | Action |
| --- | --- |
| `Left` / `A` | Move Left |
| `Right` / `D` | Move Right |
| `Up` / `W` | Rotate Piece (Wall Kicks enabled) |
| `Down` / `S` | Soft Drop |
| `Space` | Hard Drop |
| `C` | Hold Piece |
| `P` | Pause / Resume |
| `R` | Restart Game |
| `Q` | Return to Menu / Quit |

---

## Requirements

- GCC or Clang compiler
- GNU Make
- `libncurses` (ncurses development headers)

### Debian / Ubuntu
```bash
sudo apt update
sudo apt install build-essential libncurses-dev
```

### Arch Linux / Manjaro
```bash
sudo pacman -S base-devel ncurses
```

### Fedora / RHEL
```bash
sudo dnf install gcc make ncurses-devel
```

---

## Building and Running

Clone or navigate to the project directory, then run:

```bash
make
./tetru
```

To clean build artifacts:
```bash
make clean
```

---

## License

This project is licensed under the MIT License. See [LICENSE](file:///run/media/eskrid/44/404/projects/Tetru/LICENSE) for full details.
