# Delirium
some interesting experiments regarding 3d object coding, hopefully turns out useful.

THIS IS IDEALLY INNOVATION, i havent seen most likely even 5 percent of whats really out there, so dont crucify me for calling it john or whatever if this is already a thing with a different name.


starting files are mostly some stuff i tested out with uh, a dream i had with esoteric themes, lots of  M O T I V A T I O N , a not 100 percent sane mind, 20 bucks i had to spend on claude, twice that amount on energy drinks, no sleep for a couple of days, and some grok stuff as well.
whole point of this repo is to push optimization a cut above what it already is, because no im not spending 1k USD on a GPU what are you insane? point is thats not a good deal for me, nope, not at all.

key things to keep in mind as far as ive gotten (for now):
starting point is using inherent math equation surfaces instead of actual polygons, edges as a way to stitch the surfaces together, and ultimate individual points as both a way to pin in space and orient the surface (atleast 4 points per patch for both correct normal orientation and no redundancy), its worked pretty well so far.

whole point is to eventually incorporate color as just another algebraic sum of equations as well, kind of like how sums of individual patterns of R , G and B channel values can overlap to generate something coherent, or atleast close to that lol.
this is kind of hard to do with obj files, since they dont inherently have color.

should probably stick from and to fbx file since it seems more robust across different programs like blender and the likes.

the OBJ extensions are a little TEST to see how faithful the equations can be to the original object without breaking that high data compression advantage. (fbx experiments showed with color at most, maybe 100 to 200kb of extra info)

maybe you can tell, but just in case, the Level of Detail of these surfaces can be whatever you want them to be and wouldnt really be that hard, just pass whatever amount of points generated you need from said surfaces when dynamically making them.

also, since these are , purely speaking, a set of equations of which you ONLY store parameters, you could also change these parameters live and things like collisions up to even more advanced concepts like dynamic deformation, ballistic simulation, even demolition , could be inherently done from this plane of thought alone, and it would be cheaper to do so as well in terms of GPU power (the guys doing sims for EFD , FEM , thermo stuff, electrodynamics, optics and more, you guys are amazing, you can probably tell much better how good this could potentially be if at all, rock on and ride free wizards).

also, since the physics, and the collisions are easy, stuff like dynamic lighting and even raytracing could also be inherently placed and encoded into this, dont forget about this part this one is good trust me ill come back to it in a couple paragraphs.


ADVANTAGES SO FAR:

-way smaller file size (factor of atleast 6)
-faster and simpler LOD code
-faster and simpler physics
-faster and simpler lighting
-faster and simpler cool stuff you can put in your game without going into the unity store just to get depresso when you look at the prices for that really cool library from the youtube tutorial that promised the world to you (for a price).

you get the point i dont want to spend cash nor master (for the fifth time) some bizarre arcane language just to see some stick on the screen do a flip.




SECONDARY ADVANTAGES:
-wizards revealing secrets
-reality unfolds before us
-the power of friendship
-and teamwork
-yay




but what does this mean? why would you care? why do we live in a world of chaos and MADNESS?

you silly goose, this means you could eventually voxelize everything this way, with voxels so small theyre smaller than the pixels on your screen......which means

*drum rolls*

you get the same basic graphics, for a lower computational cost. HOORAY! 
🥳 🎉  that gtx1650 AINT DEAD YET BOY! REV THOSE FANS UP THERES STILL HOPE!








THINGS TO WORK ON AFTER THIS:

-use known 3d engines like godot to work on it, BUT, somehow code this into a working kernel for old gpus, otherwise im a coward a fool and not sticking to my goal.

-possibly just avoid 3d engines all together as a way to PLAY the game, and what i mean by this is do the reverse of raytracing by just flattening all these surfaces with respect to a given coordinate position, view angle and FOV and spit out the flat out RGB values.

-maybe make it so the code isnt absolute trash in the eyes of the wizards.

-a test of fire in terms of efficiency so to speak, like making a decent graphic output with some old hardware that would evidently struggle otherwise.

-a test of fire in terms of utility, such that things like building destruction or ERA simulations are feasible and faster.


THINGS THAT COULD POSSIBLY WORK REALLY WELL WITH THIS (mostly curiosity for now):

afaik, analog computation elements could do more complex  evaluations faster for a given set of equation rules and changing the parameters dynamically, just fruit for thought, nothing else.




files: SRC 1 through 4 are just initial mumble and stumble to see how far this could go and contextualize the line of thought, the blender addons are where the real deal is at
