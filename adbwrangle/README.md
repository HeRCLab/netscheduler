# adbwrangle - tool for extracting scheduling data from ADB files

**NOTE**: it turned out that the AEMF file seems to have the data we're
interested in. The ADB has some of it, but seems to be missing the clock cycle
in which things are scheduled. This code is not used anymore, you probably want
to go use [schedextract](../schedextract) instead.

This tool operates on `.adb` files produced by Xilinx Vivado HLS. Usually, this
will be something like `projectname/solution1/.autopilot/db/mynetwork.adb`.

For now, this tool outputs GEXF files, which are representations of the
extracted scheduling graphs. GEXF files can be visualized using
[Gephi](https://gephi.org/).


## Example usage:

```
python3 adbwrangle/adbwrangle.py vitis_skeleton/proj_netscheduler_skeleton/solution1/.autopilot/db/mynetwork.adb -o out1.gexf
```


