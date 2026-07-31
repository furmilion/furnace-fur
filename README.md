This is a fork of Furnace made made mainly to merge other forks or pull requests
before the upstream does.

Here's jumpers to branches:

[klattsch](https://github.com/furmilion/furnace-fur/tree/polepos_wsg) (from [#2851](https://github.com/tildearrow/furnace/pull/2851), Furnace, tildearrow)

[YM2609](https://github.com/furmilion/furnace-fur/tree/ym2609) (from [Furnace-B, YM2609 branch](https://github.com/LTVA1/furnace/tree/YM2609), LTVA)

[SGU-1](https://github.com/furmilion/furnace-fur/tree/sgu4furnace) (from [Furnace](https://github.com/X65/furnace/tree/sgu4furnace), smokku, X65) (commits up to 5b07826)

[SGU-1](https://github.com/furmilion/furnace-fur/tree/sgu_new) (from [Furnace](https://github.com/X65/furnace/tree/sgu4furnace), smokku, X65) (post 5b07826)

# SCSP TODO list
Alright so im pretty sure my ssd is alive and as soon as i can i will check and if so then i could work on the todo.

updates.

- ~~allow for use of 8-bit samples~~
 - ~~8-bit samples dont fuck up the alignment of 16-bit ones (sends the last sample to the stratosphere if it doesnt align)~~
- ~~actully treat 8-bit samples as 8-bit samples during playback~~
- ~~byteswap 8-bit samples~~
- ~~implement full 8-bit volume range of scsp~~
- remove feedback and add a second input slot instead
- fix max total level in instrument editor
- fix max volume in macro editor
- fix volume macro crash
- fix pan (how??)
- offset pan macro by -15?
- get Sound Direct bit working if it does what i think it does
- fix LPSLNK check box?? It just mutes sound when on which shouldnt happen
- change the ins editor ui??
- get SSCTL working??
‐ replace hz fixed frequency with Block+Fnum
- possibly fm wave preview????
- also maybe make so that fm doesnt assume that your wave is 1024 samples long and actually uses the wave length
- rework the ui a bit so its nicer
- ???

the list goes top down from most priority.