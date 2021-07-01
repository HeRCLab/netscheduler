import lxml.etree
import argparse
import pathlib
import networkx as nx

def main():

    parser = argparse.ArgumentParser("Tool for extracting data from Vivado HLS generated files.")

    parser.add_argument("input", type=pathlib.Path, help="Input file to process. Expects an AEMF file, such as solution1/.autopilot/db/mynetwork.aemf.")

    parser.add_argument("--gexf", "-g", default=None, type=pathlib.Path, help="If specified, dump read data to GEXF formatted graph file.")

    parser.add_argument("--gantt", "-G", default=None, type=pathlib.Path, help="If specified, dump the read schedule to a graphical Gannt chart saved to this file.")

    args = parser.parse_args()

    tree = None
    with open(args.input, "rb") as f:
            tree = lxml.etree.XML(f.read())
    assert tree is not None

    # Extract the nodes.
    cdfg_nodes = tree.xpath("//regions/basic_blocks/node_objs")
    cdfg_nodes = cdfg_nodes

    # Extract the edges.
    cdfg_edges = tree.xpath("//edges")
    cdfg_edges = cdfg_edges

    # We will process all of the nodes in the file to a graph.
    G = nx.Graph()

    for node in cdfg_nodes:
        nodeid = int(node.attrib["id"])
        nodeattr = {}

        nodeattr["name"] = node.attrib["name"]
        nodeattr["file"] = ""
        nodeattr["line"] = 0
        if "file" in node.attrib.keys():
            nodeattr["file"] = node.attrib["fileName"]
            nodeattr["line"] = int(node.attrib["lineNumber"])
        nodeattr["opcode"] = node.attrib["opcode"]
        nodeattr["m_delay"] = 0.0
        if "m_delay" in node.attrib.keys():
            nodeattr["m_delay"] = float(node.attrib["m_delay"])

        nodeattr["sched_start"] = float(node.attrib["nodeLabel"])

        G.add_node(nodeid, **nodeattr)

    for edge in cdfg_edges:
        edgeattr = {}
        edgeattr["id"] = int(edge.attrib["id"])
        edgeattr["type"] = ""
        if "edge_type" in edge.attrib.keys():
            edgeattr["edge_type"] = edge.attrib["edge_type"]
        edgeattr["source_obj"] = edge.attrib["source_obj"]
        edgeattr["sink_obj"] = edge.attrib["sink_obj"]

        # NOTE: note clear if this is sfe - may need to construct an XPath
        # query to select the correct region and basic block.
        edgeattr["source_id"] = int(edge.attrib["source_obj"].split(".")[-1])
        edgeattr["sink_id"] = int(edge.attrib["sink_obj"].split(".")[-1])

        G.add_edge(edgeattr["source_id"], edgeattr["sink_id"], **edgeattr)

    if args.gexf is not None:
        nx.write_gexf(G, str(args.gexf))


    schedule_items = []
    for node in G.nodes(data=True):
        attr = node[1]
        if "name" in node[1]:
            schedule_items.append( (attr["sched_start"], attr["m_delay"], attr["name"]) )

    schedule_items = list(reversed(list(sorted(schedule_items))))

    if args.gantt is not None:
        # https://www.geeksforgeeks.org/python-basic-gantt-chart-using-matplotlib/

        import matplotlib.pyplot as plt

        plt.tight_layout()

        fig, gnt = plt.subplots()
        fig.set_size_inches(18.5, 10.5, forward=True)


        # Setting labels for x-axis and y-axis
        gnt.set_xlabel('clock cycle')
        gnt.set_ylabel('operation')

        rowheight = 50
        barheight = rowheight/2

        gnt.set_yticks([rowheight * i for i in range(len(schedule_items))])
        gnt.set_yticklabels([item[2] for item in schedule_items])

        # Setting graph attribute
        gnt.grid(True)

        index = 0
        for item in schedule_items:
            print(item, index)
            if item[1] < 0.01:
                gnt.broken_barh([(item[0],0.5)], (index*rowheight - barheight/2, barheight), facecolors=("tab:red"))
            else:
                gnt.broken_barh([(item[0],item[1])], (index*rowheight - barheight/2, barheight), facecolors=("tab:blue"))
            index += 1


        plt.savefig(str(args.gantt), bbox_inches = "tight")



if __name__ == "__main__":
    main()
