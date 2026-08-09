# Example: the Crow

The bird the plugin was built against, start to finish. **Its content is not included** - this is a
reference for what a finished setup looks like, not something to open.

Steps are in [`README.md`](README.md); property meanings are in [`FLOCK.md`](FLOCK.md). This is the filled-in
version of both.

---

## What it started as

| | |
|---|---|
| `SKM_Crow` | 13 bones, 832 tris, 1 LOD, no morph targets |
| 18 sequences | all 30 fps, all on the one skeleton |
| `M_Flock` | an existing base colour texture and roughness parameter |

Everything below was produced from those three.

<!-- TODO(screenshot): the crow's content folder, before the bake -->

---

## 1. Material

`M_Flock` got **Use Material Attributes**, **Num Customized UVs 1** and **Used with Instanced Static
Meshes**, its two nodes went into a `MakeMaterialAttributes`, and `MF_BoneAnimation` went between that and
the output. `MI_Crow_VAT` was made from it.

<!-- TODO(screenshot): M_Flock's graph, showing MakeMaterialAttributes into MF_BoneAnimation -->
<!-- TODO(screenshot): the material's details panel, with the three settings ticked -->

---

## 2. Bake

`DA_Species_Crow` was created empty first, so the recipe had somewhere to live.

| Bake window | |
|---|---|
| Species | `DA_Species_Crow` |
| Source Skeletal Mesh | `SKM_Crow` |
| Output Path | `/Game/Characters/Creatures/Crow/Flock` |
| Asset Name | `Crow` |
| Clips | all 18 sequences |
| Sample Rate | `30` |
| Bone Material Instances | `MI_Crow_VAT` |

**Prepare Asset Set**, then `MI_Crow_VAT` onto `SM_Crow_VAT`'s material slot, then **Bake**.

<!-- TODO(screenshot): the bake window, filled in as above -->

It produced `SM_Crow_VAT`, `TX_Crow_BonePosition`, `TX_Crow_BoneRotation`, `TX_Crow_BoneWeight` and
`DA_Crow_BoneAnimation`.

<!-- TODO(screenshot): the Flock folder after the bake -->
<!-- TODO(screenshot): DA_Crow_BoneAnimation, showing NumBones 13 and the Animations array -->

---

## 3. Clips

What this bake's indices came out as. **They are per-bake** - the order is the data asset's enabled
sequence list, not the order the clips were added, so re-check them after any re-bake.

| Index | Mapped to | |
|---|---|---|
| 0 | **Fly** | loop |
| 1 | **Idle** | loop, **Random Start Phase** |
| 2 | **Land** | one-shot |
| 14 | **Take Off** | one-shot |
| 15 | **Turn Left** | loop |
| 16 | **Turn Right** | loop |
| 17 | **Walk** | loop |

The five optional clips - **Take Off Loop**, **Land Loop**, **Glide**, **Bank Left** and **Bank Right** -
were all left unmapped, so the crow uses **Fly** for the rest of a climb, the rest of a descent, and every
turn. Worth authoring if you have the animations; a glide in particular is most of what a descent looks like.

<!-- TODO(screenshot): the species' Clips map, expanded -->

Indices 3-13 are the rest breaks.

---

## 4. Rest breaks

Eleven animations across eight entries, weighted so the small movements are common and the conspicuous ones
are a treat.

| Name | Index | Weight | |
|---|---|---|---|
| Preen | 9 | 3.0 | mirrors 10 |
| Head Cock | 6 | 3.0 | mirrors 7 |
| Caw | 4 | 2.0 | **Audio Trigger** `Caw`, plus a **Sound Delay** to put it where the beak opens |
| Wing Shuffle | 11 | 1.5 | |
| Ground Peck | 5 | 1.5 | |
| Body Shake | 3 | 1.0 | |
| Hop | 8 | 0.8 | |
| Wing Stretch | 12 | 0.4 | mirrors 13 |

A mirrored pair is one entry, so a wing stretch left and right share that 0.4 between them rather than
getting 0.4 each.

<!-- TODO(screenshot): the Rest Breaks array, collapsed, showing the eight names -->
<!-- TODO(screenshot): the Caw entry expanded, showing its sounds, trigger and delay -->

---

## 5. Sound

`MSS_FlockBed_Crow` in **Flock Bed**, taking `Distance`, `BirdCount`, `Alert` and `AirborneRatio` and
deciding its own mix, with the `Caw` trigger coming off the rest break above.

<!-- TODO(screenshot): MSS_FlockBed_Crow's graph, or at least its input parameters -->
<!-- TODO(screenshot): the species' audio and VFX slots -->

<!-- TODO(jared): note any Perception, Flight or Idle values you moved off default, and why -->

---

## 6. In the level

A **Flock Volume** sized to the area the birds should own, at ground height, with **Species** set and
**Spawn Count** raised until it read right.

<!-- TODO(screenshot): the volume in the viewport, box visible, with its details panel -->

Perches came from **Flock Perch Components** on the things birds should land on.

<!-- TODO(screenshot): a perch component on a fence or roof, slots drawn -->
<!-- TODO(screenshot): flock.Debug.Slots 1 in play, slots green/yellow/red -->

---

## Result

<!-- TODO(video): the flock idling, then scattering when the player walks in -->
<!-- TODO(screenshot): flock.Debug.Perception 1, showing states and clip names -->
