# Flocking

A flock of boids in Unreal Engine 5.8. They leave home, travel together, and come back. You can change how tightly they stick together while they are moving.

## What it does

- Spawns the flock when play starts, unless one is already in the level.
- Steers each boid with separation, alignment, and cohesion.
- Runs the trip out and back, then holds at home.
- Frames an overview camera on the flock.

## Controls

| Key | Action |
| --- | --- |
| 1 or Numpad 1 | Raise separation. Hold Shift to lower it. |
| 2 or Numpad 2 | Raise alignment. Hold Shift to lower it. |
| 3 or Numpad 3 | Raise cohesion. Hold Shift to lower it. |
| R | Send the flock out again with the current weights. |

## Run it

1. Install Unreal Engine 5.8.
2. Open `IceTask4.uproject` and let the editor compile.
3. Press Play on `Lvl_ThirdPerson`. The flock is created when the level starts.

The C++ for the flock is in `Source/IceTask4/Flock`.
