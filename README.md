# Mexican Train CE

A native TI-84 Plus CE C/GraphX Mexican Train project.

## Current build

- Double-12 domino set
- 2-8 players
- Per-player HUMAN/CPU selection
- Distinct player colors
- CPU turns
- Mexican Train
- Personal trains
- Open train markers
- Drawing from the boneyard
- Doubles / forced-double state
- Animated play/draw feedback
- Score/round screen
- 320x240 GraphX UI

## Controls

### Setup
- LEFT/RIGHT: change player count or HUMAN/CPU
- UP/DOWN: select player
- 2ND: switch between player count and player-type editing
- ENTER: start

### Game
- LEFT/RIGHT: choose domino
- UP/DOWN: choose target train
- ENTER: play selected domino
- GRAPH: draw
- CLEAR: pass / mark your train open

## Build

The GitHub Action downloads the official CEdev nightly toolchain, runs `make`, and uploads the resulting `.8xp` as a workflow artifact.

The project uses the official CE Programming GraphX template Makefile style.
