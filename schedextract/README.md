# schedextract - tool for extracting scheduling data from AEMF files

This tool operates on `.aemf` files produced by Xilinx Vivado HLS. Usually, this
will be something like `projectname/solution1/.autopilot/db/mynetwork.aemf`.

For now, this tool outputs GEXF files, which are representations of the
extracted scheduling graphs. GEXF files can be visualized using
[Gephi](https://gephi.org/).

It can also output a Gantt chart showing the schedule using the `--gant` flag.
see `python3 schedextract/schedextract.py --help` for more.


## Example usage:

```
python3 schedextract/schedextract.py vitis_skeleton/proj_netscheduler_skeleton/solution1/.autopilot/db/mynetwork.aemf -G out.pdf -g out.gexf
```


