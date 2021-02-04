import lxml.etree
import argparse
import pathlib
import networkx as nx

def main():

    parser = argparse.ArgumentParser("Tool for extracting data from Vivado HLS ADB files.")

    parser.add_argument("input", type=pathlib.Path, help="Input file to process.")

    parser.add_argument("--gexf", "-g", default=None, type=pathlib.Path, help="If specified, dump read data to GEXF formatted graph file.")

    args = parser.parse_args()

    tree = None
    with open(args.input, "rb") as f:
            tree = lxml.etree.XML(f.read())
    assert tree is not None

    # We expect to see soemthing like boost_serialization/syndb/cdfg/nodes,
    # which containst he nodes of the scheduling diagram. cdfg_nodes is now
    # a handle to theparent element of all of the node objects.
    #
    #  <?xml version="1.0" encoding="UTF-8" standalone="yes" ?>
    #  <!DOCTYPE boost_serialization>
    #  <boost_serialization signature="serialization::archive" version="15">
    #    <syndb class_id="0" tracking_level="0" version="0">
    #      <userIPLatency>-1</userIPLatency>
    #      <userIPName/>
    #      <cdfg class_id="1" tracking_level="1" version="0" object_id="_0">
    #        <name>mynetwork</name>
    #        <ret_bitwidth>0</ret_bitwidth>
    #        ...
    #        <nodes class_id="8" tracking_level="0" version="0">
    #          <count>33</count>
    #          <item_version>0</item_version>
    #         ...
    #        </nodes>
    #        ...
    #        <edges class_id="19" tracking_level="0" version="0">
    #           <count>64</count>
    #           <item_version>0</item_version>
    #           ...
    #        </edges>
    #        ...
    cdfg_nodes = tree.xpath("//syndb/cdfg/nodes")
    assert len(cdfg_nodes) == 1
    cdfg_nodes = cdfg_nodes[0]

    cdfg_edges = tree.xpath("//syndb/cdfg/edges")
    assert len(cdfg_edges) == 1
    cdfg_edges = cdfg_edges[0]

    # We will process all of the nodes in the ADB file to a graph.
    G = nx.Graph()

    # Create a graph node for each node in the ADB file.
    #  <item class_id_reference="9" object_id="_27">
    #            <Value>
    #              <Obj>
    #                <type>0</type>
    #                <id>44</id>
    #                <name>node7</name>
    #                <fileName>network.c</fileName>
    #                <fileDirectory>..</fileDirectory>
    #                <lineNumber>18</lineNumber>
    #                <contextFuncName>mynetwork</contextFuncName>
    #                <contextNormFuncName>mynetwork</contextNormFuncName>
    #                <inlineStackInfo>
    #                  <count>1</count>
    #                  <item_version>0</item_version>
    #                  <item>
    #                    <first>/home/cad/f/src/netscheduler/vitis_skeleton</first>
    #                    <second>
    #                      <count>1</count>
    #                      <item_version>0</item_version>
    #                      <item>
    #                        <first>
    #                          <first>network.c</first>
    #                          <second>mynetwork</second>
    #                        </first>
    #                        <second>18</second>
    #                      </item>
    #                    </second>
    #                  </item>
    #                </inlineStackInfo>
    #                <originalName>node7</originalName>
    #                <rtlName>fmul_32ns_32ns_32_4_max_dsp_1_U9</rtlName>
    #                <control>auto</control>
    #                <opType>fmul</opType>
    #                <implIndex>maxdsp</implIndex>
    #                <coreName>FMul_maxdsp</coreName>
    #                <coreId>30</coreId>
    #              </Obj>
    #              <bitwidth>32</bitwidth>
    #            </Value>
    #            <oprand_edges>
    #              <count>2</count>
    #              <item_version>0</item_version>
    #              <item>104</item>
    #              <item>105</item>
    #            </oprand_edges>
    #            <opcode>fmul</opcode>
    #            <m_Display>0</m_Display>
    #            <m_isOnCriticalPath>0</m_isOnCriticalPath>
    #            <m_isLCDNode>0</m_isLCDNode>
    #            <m_isStartOfPath>0</m_isStartOfPath>
    #            <m_delay>2.32</m_delay>
    #            <m_topoIndex>22</m_topoIndex>
    #            <m_clusterGroupNumber>-1</m_clusterGroupNumber>
    #          </item>
    for item in cdfg_nodes.xpath("item"):
        # The node ID we want to use in networkx is actuall item/Value/Obj/id,
        # the more obvious object_id seems to be a Boost thing, and will result
        # in invalid references when we process the edges later.
        nodeid = int(item.xpath("Value/Obj/id")[0].text)
        nodeattr = {}

        # Extract some seemingly relevant parameters from the ADB...

        # The name of the object - this appears to be the name of the object
        # either in the HLS IR, or in the generated RTL(?).
        nodeattr["name"] = item.xpath("Value/Obj/name")[0].text

        # The file from which the object was generated.
        nodeattr["file"] = item.xpath("Value/Obj/fileName")[0].text

        # The line number within that file.
        nodeattr["line"] = int(item.xpath("Value/Obj/lineNumber")[0].text)

        # Bit width of the object.
        nodeattr["bitwidth"] = int(item.xpath("Value/bitwidth")[0].text)

        # Opcode of the object.
        nodeattr["opcode"] = item.xpath("opcode")[0].text

        # Delay, not sure what the units are.
        nodeattr["m_delay"] = item.xpath("m_delay")[0].text

        # We might end up wanting this later - I think these are globally
        # unique, whereas the id field is only unique within a specific
        # object type (?).
        nodeattr["object_id"] = item.attrib["object_id"]

        for k in nodeattr.keys():
            if nodeattr[k] is None:
                nodeattr[k] = str(nodeattr[k])

        # Insert it into our networkx graph.
        G.add_node(nodeid, **nodeattr)

    # Now we need to process all of the edges...
    #
    #  <item class_id="20" tracking_level="1" version="0" object_id="_48">
    #    <id>60</id>
    #    <edge_type>2</edge_type>
    #    <source_obj>25</source_obj>
    #    <sink_obj>17</sink_obj>
    #    <is_back_edge>0</is_back_edge>
    #  </item>
    for item in cdfg_edges.xpath("item"):
        edgeattr = {}
        edgeattr["id"] = int(item.xpath("id")[0].text)
        edgeattr["edge_type"] = int(item.xpath("edge_type")[0].text)
        source = int(item.xpath("source_obj")[0].text)
        sink = int(item.xpath("sink_obj")[0].text)
        edgeattr["is_back_edge"] = int(item.xpath("is_back_edge")[0].text)

        G.add_edge(source, sink, **edgeattr)

    if args.gexf is not None:
        nx.write_gexf(G, str(args.gexf))

if __name__ == "__main__":
    main()
