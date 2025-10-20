# Planet Gravity in Super Mario 64 DS

The code in this repository implements a planet gravity system in Super Mario 64 DS.
It enables SM64DS hacks to use planets as part of their level design,
similar to Super Mario Galaxy.
The system allows ROM hack developers to define gravity fields easily without modifying the code,
and makes different aspects of the game
(actors, models, mesh colliders, cylinder colliders, particles, etc.)
act according to those fields,
effectively changing the directions of "up" and "down" based on their locations.
It also includes a fully custom camera system designed to work well on planets
and allows smooth and seamless camera transitions when moving between gravity fields.

## Tech Demo

A tech demo showcasing the gravity engine has been released.
You can download it on the [Releases page](https://github.com/pants64DS/Gravity64DS/releases).

## Gravity fields

A gravity field defines three properties for each point in the 3D space:
whether the point is affected by the gravity field,
the direction of "up" at that point,
and an altitude value.
In particular, a gravity field defines the up-direction of each actor that's affected by it.
An actor is only affected by one gravity field at a time,
except for a few frames when moving between fields.
Each field has a priority value;
when an actor is in multiple fields at the same time,
the field with the highest priority takes precedence.
When the priorities are equal,
the altitude is used as a tie-breaker.

### The default gravity field

In every level, there always exists a default gravity field
that spans the entire 3D space.
It's an example of a *trivial gravity field*,
which is a gravity field where the up-direction is the same as in vanilla SM64DS,
and everything generally functions as if the gravity system was not present.
The default gravity field has a lower priority than any other field,
and it has the special property that once
an actor has entered another field,
it won't be affected by the default field again (but it can be affected by other trivial fields).
If such an actor goes outside of all gravity fields except the default field,
it will keep acting according to the up-direction determined
by the most recent user-defined field.

### User-defined fields

With the [SM64DS Editor](https://github.com/Gota7/SM64DSe-Ultimate),
gravity fields can be added to a custom level using path objects.
Each path in SM64DS has three parameters,
which are treated as follows:

- #### Path parameter 1: Gravity field ID
  Determines if the path defines a gravity field and which type of gravity field it is.
  The values for gravity fields start at `40` (see below).

- #### Path parameter 2: Priority
  The priority value is used to decide which gravity field will be effective in regions where multiple fields overlap.
  The field with the highest priority value is chosen.
  If two or more fields have an equal priority,
  the one where the given point has the lowest altitude is chosen.

- #### Path parameter 3: Planet camera setting ID
  Determines the planet camera setting that will be used by default when the player enters the gravity field.
  Set this to `FF` if you don't want the camera setting to change.
  See [Planet camera](#planet-camera).

Path nodes are used to define the location, shape and size of a gravity field.
They are interpreted in different ways depending on the gravity field ID.
The following gravity field types are currently supported:

### Radial field

<img width="1221" height="995" alt="A path that defines a radial field in SM64DSe" src="https://github.com/user-attachments/assets/d21fa3f7-4aa1-43ae-8b23-da0da3c3726b" />

A path defines a radial field when the gravity field ID is `40`
and it has exactly two path nodes.
In a radial field, the direction of gravity is always towards a fixed point,
defined by the first path node.
The shape of the field is spherical.
The radius is defined by the distance between the two path nodes.
The field contains all points inside the sphere with the given center and radius.

### Axial field

<img width="1375" height="986" alt="A path that defines multiple axial fields in SM64DSe" src="https://github.com/user-attachments/assets/f007f27a-76f0-486a-9632-ba4a1f9d124a" />

An axial field also uses the gravity field ID `40`,
but it requires three or more path nodes.
When there are three path nodes,
the first two nodes define a line segment,
and the distance between the second and the third node
defines the radius of the field.
In this case, the direction of gravity is always towards
the closest point on the line segment,
and the field contains all points whose distance
to the line segment is not greater than the radius.

When there are more than three path nodes,
each consecutive pair of nodes defines its own axial gravity field,
excluding the last node.
All such fields defined by the same path share the same radius,
which is defined by the distance between the last two nodes in the path.
Each field works the same as in the three-node case.

### Homogeneous cylinder field

<img width="1221" height="995" alt="A path that defines a homogeneous cylinder field field in SM64DSe" src="https://github.com/user-attachments/assets/b21e9846-6d49-46ac-908c-f38c03896c91" />

A homogeneous cylinder field is defined by the gravity field ID `41` and three path nodes.
The first two nodes define the axis of the cylinder.
The first node is the center of the bottom of the cylinder,
and the second node is the center of the top of the cylinder.
The radius of the cylinder is defined by the distance
between the axis of the cylinder (a line segment between the first two nodes) and the third node.
The direction of gravity is the same everywhere in the field,
towards the bottom of the cylinder.
The field contains all points inside the cylinder.

### Trivial cylinder field

<img width="1229" height="988" alt="A path that defines a trivial cylinder field field in SM64DSe" src="https://github.com/user-attachments/assets/159e8ae5-3d5e-42d3-a1c7-e8edb54e2a69" />

A trivial cylinder field is defined by the gravity field ID `42` and two path nodes.
The first path node defines the center of the bottom of the cylinder,
and the second path node is on the top rim..
The axis of the cylinder is vertical with respect to the coordinate system.
Like the name implies, this field is a trivial field,
so the direction of gravity is always towards the bottom of the cylinder.

## Planet camera

Since the original SM64DS camera wouldn't work very well with planets,
the gravity engine includes an entirely custom camera.
Like the vanilla camera, it can be configured separately for each collision type,
but it has more elaborate settings.
With [SM64DSe](https://github.com/Gota7/SM64DSe-Ultimate) v3.5.0 or later,
one may define an array of planet camera settings for a given level
by clicking the "Planet Cam" button in the level editor.
Once at least one entry has been added,
the CLPS editor will show a column for the planet camera settings ID.
The ID is stored in bits 40-47 in each CLPS entry.

<img width="976" height="569" alt="Planet camera settings in SM64DSe" src="https://github.com/user-attachments/assets/6ac3076c-48ef-4201-b054-fae09f0c654c" />

A new planet camera is created every time the player moves from one gravity field to another.
When that happens, the cameras are interpolated as smoothly as possible.
In contrast, when the planet camera settings change within the current gravity field,
the settings are interpolated without switching to a new camera.
The duration of this interpolation can be configured in the "frames to transition"
column in the planet camera settings.

A view object can be used to influence the spawn position of the planet camera,
but in this case, the view parameters are ignored.
A view can also be used to define the pause camera center point,
similar to the vanilla camera.
The planet camera settings ID of a gravity field allows changing the planet camera settings
immediately when entering the field, before touching any collision.

## Current limitations

These are the current limitations of the gravity engine,
specifically in non-trivial gravity fields:

- Yoshi's mechanics aren't supported yet.
  This includes everything to do with the tongue and egg-throwing,
  as well as the flutter jump.
  Because of this, it's currently not recommended to
  let Yoshi enter levels with non-trivial gravity fields.
- Support for specific objects in non-trivial gravity fields varies.
  Users are encouraged to test which ones work and which ones don't.
  If there's a particular object that you really want to use but doesn't work yet,
  feel free to open an issue on this repository.
- Water generally doesn't work at all; don't use it outside of trivial gravity fields.

In trivial gravity fields, everything should still work as usual.

## Side effects

The only noticeable side effect of the gravity engine should be that fall damage is disabled everywhere.
The mechanic would need some patches to work properly in non-trivial gravity fields,
but even then it would feel out of place in terms of game design.

## Memory usage

The gravity engine takes around 27 KiB when compiled with the `-Os` flag.
In addition, each actor takes an additional 192 bytes on the actor heap,
and a small amount of memory on the main heap will be used by the planet camera.
To make up for these memory costs,
please consider using the [heap expansion code](https://github.com/pants64DS/SM64DS-Heap-Expansion).

## Inserting the code

The gravity engine is linked to the original code of the game using
[NSMBe](https://github.com/pants64DS/NSMB-Editor) patches.
Only the European version of SM64DS is supported.
Before compiling the code, make sure you have [devkitPro](https://devkitpro.org/) installed.
You may use any existing setup for inserting code with NSMBe,
but it's recommended that your linker script is similar to
[this one](https://github.com/pants64DS/Misc-SM64DS-Patches/blob/master/linker.x).
If you use Git for your project, it's highly recommended to use this repository as a
[Git submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules).

The [Misc-SM64DS-Patches](https://github.com/pants64DS/Misc-SM64DS-Patches)
repository gives a template for a possible build setup.
To include the gravity engine in the NSMBe patches using its
[makefile](https://github.com/pants64DS/Misc-SM64DS-Patches/blob/master/Makefile),
the [source](source) and [include](include) directories of this repository should be added to the `SOURCE` and `INCLUDES`
variables on lines
[17](https://github.com/pants64DS/Misc-SM64DS-Patches/blob/f3de190a166c9d8003df1add8aaaa2733d08e3bc/Makefile#L17)
and
[56](https://github.com/pants64DS/Misc-SM64DS-Patches/blob/f3de190a166c9d8003df1add8aaaa2733d08e3bc/Makefile#L56),
respectively, as follows:
```
	@make --no-print-directory SOURCE='source/patches Gravity64DS/source' BUILD=build/patches
```
```
  INCLUDES := include include/MOM source SM64DS-PI/include Gravity64DS/include
```
Before inserting the code,
make sure you have opened it in
[SM64DSe](https://github.com/Gota7/SM64DSe-Ultimate)
and "toggled it suitable" for NSMBe patching
by clicking on "ASM Hacking" and then "Toggle Suitability for NSMBe ASM Patching".
To compile and insert the NSMBe patches,
put your SM64DS ROM in the same directory as the makefile,
open it in NSMBe and click on "Run 'make' and insert" in the "Tools/Options" tab.

## Pseudo-mesh sphere

Planets typically have hundreds of triangles,
which takes a lot of memory and may sometimes cause lag when
the game has to check too many collision triangles each frame.
Because of this,
it's recommended to use the
[pseudo-mesh sphere](https://github.com/pants64DS/Misc-SM64DS-Patches/tree/master/source/DLs/pseudo_mesh_sphere)
object instead of collision triangles for any spherical shapes whenever possible.
The object acts as a mesh collider in the game,
even though it isn't made of triangles,
which can save huge amounts of CPU cycles and memory.
The pseudo-mesh sphere should be compiled with the `-DGRAVITY64DS`
flag to make it work properly with the gravity engine.
