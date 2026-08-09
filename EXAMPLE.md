# Example: the Crow

The bird the plugin was built against, start to finish. **Its content is not included** - this is a
reference for what a finished setup looks like, not something to open.

Steps are in [`README.md`](README.md); property meanings are in [`FLOCK.md`](FLOCK.md). This is the filled-in
version of both.

---

## What it started as

| | |
|---|---|
| `SKM_Crow` | 18 bones, 832 tris, 1 LOD, no morph targets |
| 21 sequences | all 30 fps, all on the one skeleton |
| `M_Flock` | an existing base colour texture and roughness parameter |

Everything below was produced from those three.

<!-- TODO(screenshot): the crow's content folder, before the bake -->

### Full Animation Set

[Crow.webm](https://github.com/user-attachments/assets/b780ff2a-714d-4bbd-99eb-1978606823d5)

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
| Clips | all 21 sequences |
| Sample Rate | `30` |
| Bone Material Instances | `MI_Crow_VAT` |

**Prepare Asset Set**, then `MI_Crow_VAT` onto `SM_Crow_VAT`'s material slot, then **Bake**.

<!-- TODO(screenshot): the bake window, filled in as above -->

It produced `SM_Crow_VAT`, `TX_Crow_BonePosition`, `TX_Crow_BoneRotation`, `TX_Crow_BoneWeight` and
`DA_Crow_BoneAnimation`.

<!-- TODO(screenshot): the Flock folder after the bake -->
<!-- TODO(screenshot): DA_Crow_BoneAnimation, showing NumBones 18 and the Animations array -->

---

## 3. Clips

What this bake's indices came out as. **They are per-bake** - the order is the data asset's enabled
sequence list, not the order the clips were added, so re-check them after any re-bake.

| Index | Source | Mapped to | |
|---|---|---|---|
| 0 | `CrowFly` | **Fly** | loop |
| 1 | `CrowIdle` | **Idle** | loop, **Random Start Phase** |
| 2 | `CrowLand` | **Land** | one-shot |
| 14 | `CrowTakeOff` | **Take Off** | one-shot |
| 15 | `CrowTurn_L` | **Turn Left** | loop |
| 16 | `CrowTurn_R` | **Turn Right** | loop |
| 17 | `CrowWalk` | **Walk** | loop |
| 18 | `CrowTakeOffLoop` | **Take Off Loop** | loop |
| 19 | `CrowLandLoop` | **Land Loop** | loop |
| 20 | `CrowGlide` | **Glide** | loop |

Indices 3-13 are the rest breaks, `CrowRB_` prefixed.

**Bank Left** and **Bank Right** are the only clips left unmapped. The bird leans through its transform
instead, which is continuous rather than an on/off pose - see [Flying](FLOCK.md#flying).

<!-- TODO(screenshot): the species' Clips map, expanded -->

---

## 4. Rest breaks

Eleven animations across eight entries, weighted so the small movements are common and the conspicuous ones
are a treat.

| Name | Index | Weight | |
|---|---|---|---|
| Preen | 9 | 3.0 | mirrors 10 |
| Head Cock | 6 | 3.0 | mirrors 7 |
| Caw | 4 | 25.0 | `MS_Crow_Caw` in **Sounds** |
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
deciding its own mix. The caw is a **Sound** on its rest break rather than a bed trigger, so it comes from
the bird that opened its beak.

<!-- TODO(screenshot): MSS_FlockBed_Crow's graph, or at least its input parameters -->
<!-- TODO(screenshot): the species' audio and VFX slots -->

---

## 6. Tuning moved off default

| | | |
|---|---|---|
| **Takeoff Time** | `1.0` | `CrowTakeOff` runs 0.93s. Under that the bird is already flying when the clip ends and **Take Off Loop** never gets a turn |
| Caw **Weight** | `25` | crows caw constantly; at the 2.0 it started on it was a rarity |

<!-- TODO(jared): add any Perception, Flight or Idle values you moved off default, and why -->

---

## 7. In the level

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
