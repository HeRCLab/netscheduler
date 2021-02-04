# adbwrangle - tool for extracting scheduling data from ADB files

This tool operates on `.adb` files produced by Xilinx Vivado HLS. Usually, this
will be something like `projectname/solution1/.autopilot/db/mynetwork.adb`.

For now, this tool outputs GEXF files, which are representations of the
extracted scheduling graphs. GEXF files can be visualized using
[Gephi](https://gephi.org/).


## Example usage:

```
python3 adbwrangle/adbwrangle.py vitis_skeleton/proj_netscheduler_skeleton/solution1/.autopilot/db/mynetwork.adb -o out1.gexf
```


