############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
from collections import OrderedDict

from ACAT.Core import CoreUtils as cu

OPERATOR_MASK = 0x5fc0
DIRECTION = 'direction'
CONNECTED_TO_ID = 'connected_to_id'
IS_REAL = 'is_real'
EP_TITLE = 'ep_title'
OPERATOR_NUM_MASK = 0X3F
STREAM_EP_OP_BIT = 0x4000
STREAM_EP_SINK_BIT = 0x8000
STREAM_EP_OPID_MASK = 0x1FC0
IS_OPERATOR = 'is_operator'
ELEMENTS = 'elements'
ALL_DETAILS = 'all_details'

try:
    from graphviz import Digraph
    graphviz_available = True
except ImportError:
    graphviz_available = False


def is_grapviz_available():
    '''
    @brief function call to check if graphviz is available
    @param[out] return boolean indicating if graphviz is available
    '''
    return graphviz_available


def create_ep_img(type):
    """
    @brief create the endpoint image
    @param[in] type specifying source or sink
    @param[out] return the subgraph (source or sink) and the invisible node
    needed to make the subgraphs allign
    """
    current_index = cu.get_non_rep_nuber()
    invis_node = type + '_invis' + str(current_index)

    sub_graph = Digraph(name='cluster_' + str(current_index))
    sub_graph.attr(
        'graph',
        label='',
        style='invis',
    )
    sub_graph.attr('node')
    sub_graph.node(invis_node, style='invis', shape='point')
    return sub_graph, invis_node


def add_ep_img(
        ep_id, details, sub_graph, ep_title, all_details, current_proc,
        detailed_tooltip
    ):
    """
    @brief Add endpoint nodes to the source or sink subgraph
    @param[in] ep_id The endpoint ID string in hex
    @param[in] details containing the required endpoint details
    @param[in] subgraph specifying whether to add to source or sink sub graph
    @param[in] ep_title which is the link to the details of the endpoint
    @param[in] current processor being analysed
    @param[in] detailed_tooltip If true, the tooltip will contain a lot more
        information.
    """
    if detailed_tooltip:
        tooltip = all_details.replace("\n", "&#13;")
    else:
        tooltip = ep_title
    # Save the parameters into a dictionary
    kwargs = {
        "name": ep_id,
        "href": '    #' + current_proc + "_" + ep_title.replace(" ", ""),
        "tooltip": tooltip,
        "shape": 'square',
        "style": 'filled'
    }
    # Convert the endpoint id to an integer
    ep_id_hex = int(ep_id, 16)
    # Set the color depending on the endpointe type. This can change if the
    # endpoint is real.
    if ep_id_hex & STREAM_EP_SINK_BIT:
        kwargs["color"] = 'cyan4'
    else:
        kwargs["color"] = 'chartreuse1'
    # Override the color and shape if the endpoint is a real one.
    if details[IS_REAL]:
        kwargs["color"] = 'red'

    # set the label for operator endpoints
    if ep_id_hex & STREAM_EP_OP_BIT:
        kwargs["label"] = str(ep_id_hex & OPERATOR_NUM_MASK) + "\\n" + ep_id

    # Finally create the node
    sub_graph.node(**kwargs)


def create_graph_dict(endpoints, operators):
    '''
    @brief Create graph dictionary that will contain the operator and endpoint
    details needed to draw the graph
    @param[in] endpoints containing the endpoint details
    @param[out] graph_dict containing operator and endpoint details for drawing
    graph
    '''
    from ..Analysis.Stream import Stream
    from ..Analysis.Stream import OperatorEndpoint
    graph_dict = OrderedDict()
    endpoints = sorted(endpoints, key=lambda ep: ep.id)

    # add operators to graph_dict (useful for operators that are not connected
    # to anything)
    for op in operators:
        op_name = op.cap_data.name + " " + hex(op.op_ep_id)
        graph_dict[op_name] = OrderedDict()
        graph_dict[op_name][ALL_DETAILS] = op.desc_str
        graph_dict[op_name][ELEMENTS] = OrderedDict()
        graph_dict[op_name][IS_OPERATOR] = True

    # add endpoints to graph_dict
    for ep in endpoints:
        if isinstance(ep, OperatorEndpoint):
            operator_id = (ep.id & OPERATOR_MASK)
            cluster_name = ep.name + " 0x%04x" % (operator_id)
        else:
            cluster_name = ep.name
        if cluster_name not in graph_dict:
            graph_dict[cluster_name] = OrderedDict()
            graph_dict[cluster_name][ELEMENTS] = OrderedDict()

        # shadow endpoints are input to the system so its faked as real in
        # dictionary
        try:
            connected_to_id = "0x%04x" % (int(ep.connected_to.id))
        except AttributeError:
            # In case the ep is not connected to anything
            connected_to_id = ' None '
        graph_dict[cluster_name][ELEMENTS]["0x%04x" % (ep.id)] = {
            DIRECTION: ep.direction,
            IS_REAL: Stream._ep_is_real_or_shadow(ep),
            EP_TITLE: ep.title_str,
            CONNECTED_TO_ID: connected_to_id,
            ALL_DETAILS: ep.desc_str
        }
        graph_dict[cluster_name][IS_OPERATOR] = isinstance(
            ep, OperatorEndpoint
        )
    return graph_dict


def create_legend():
    """
    @Bbrief create legend for the graph image
    @param[out] legend Digraph
    """
    leg = Digraph(name='cluster_' + str(cu.get_non_rep_nuber()))
    leg.attr(
        'graph',
        tooltip='legend',
        href='#Legend',
        label='Legend',
        rankdir='LR',
        title='Legend',
        labelloc='t'
    )

    # create real endpoint cluster
    leg_real_clust = Digraph(name='cluster_' + str(cu.get_non_rep_nuber()))
    leg_real_clust.attr(
        'graph',
        label='&lt;Real endpoint cluster name&gt;',
        style='filled',
        fillcolor='gold',
        color='black'
    )
    leg_real_clust_source, real_source_invis = create_ep_img('source')
    leg_real_clust_sink, real_sink_invis = create_ep_img('sink')
    leg_real_clust_source.node(
        'real_ep_source_id',
        shape='rectangle',
        label='&lt;real endpoint source id&gt;',
        height='0.01',
        color='red',
        style='filled'
    )
    leg_real_clust_sink.node(
        'real_ep_sink_id',
        shape='rectangle',
        label='&lt;real endpoint sink id&gt;',
        height='0.01',
        color='red',
        style='filled'
    )
    leg_real_clust.edge(real_sink_invis, real_source_invis, style='invis')
    leg_real_clust.subgraph(leg_real_clust_source)
    leg_real_clust.subgraph(leg_real_clust_sink)
    leg.subgraph(leg_real_clust)

    # create operator cluster
    leg_op_clust_params = {
        'style': 'filled',
        'height': '0.01',
        'shape': 'rectangle'
    }
    leg_op_clust = Digraph(name='cluster_' + str(cu.get_non_rep_nuber()))
    leg_op_clust.attr(
        'graph',
        label='&lt;Operator Name&gt;',
        style='filled',
        fillcolor='lightblue3',
        color='black',
        title='Operator'
    )
    leg_op_clust_source, _ = create_ep_img('source')
    leg_op_clust_sink, _ = create_ep_img('sink')
    leg_op_clust_source.node(
        'op_source',
        color='chartreuse1',
        label='&lt;source terminal number&gt;\\n &lt;source endpoint id&gt;',
        **leg_op_clust_params
    )
    leg_op_clust_sink.node(
        'op_sink',
        color='cyan4',
        label='&lt;sink terminal number&gt;\\n &lt;sink endpoint id&gt;',
        **leg_op_clust_params
    )
    leg_op_clust.edge('op_sink', 'op_source', style='invis')
    leg_op_clust_source.node(
        'op_source_real',
        label='&lt;sink terminal '
        'number&gt;\\n &lt;real source endpoint id&gt;',
        color='red',
        **leg_op_clust_params
    )
    leg_op_clust_sink.node(
        'op_sink_real',
        label='&lt;sink terminal '
        'number&gt;\\n &lt;real sink endpoint id&gt;',
        color='red',
        **leg_op_clust_params
    )
    leg_op_clust.edge('op_sink_real', 'op_source_real', style='invis')
    leg_op_clust.subgraph(leg_op_clust_source)
    leg_op_clust.subgraph(leg_op_clust_sink)

    # create edge between real endpoint source and sink operator endpoint
    leg.edge(
        'real_ep_source_id',
        'op_sink',
        label='&lt;percentage buffer '
        'filled&gt;\n&lt;total buffer size&gt;\n&lt;terminal id&gt;'
    )

    leg.subgraph(leg_op_clust)
    return leg


def get_operator_tooltip(op_name, op_details, detailed_tooltip=False):
    """
    @brief Returns the tooltip for a transform
    @param[in] op_name Operator name
    @param[in] op_details details about the operator
    @param[in] detailed_tooltip If true, the tooltip will contain a lot more
        information.
    @param[out] transform tooltip string
    """
    if detailed_tooltip:
        return op_details.replace("\n", "&#13;")
    else:
        return op_name


def get_transform_tooltip(transform, detailed_tooltip=False):
    """
    @brief Returns the tooltip for a transform
    @param[in] transform pointer to a transform object
    @param[in] detailed_tooltip If true, the tooltip will contain a lot more
        information.
    @param[out] transform tooltip string
    """
    if detailed_tooltip:
        return str(transform.cbuffer).replace("\n", "&#13;")
    else:
        return transform.short_title_str


def create_graph(
        endpoints, transforms, operators, proc, detailed_tooltip=False
    ):
    """
    @brief Create the graph provided the graph details
    @param[in] endpoints containing the endpoint details
    @param[in] transform containing the edge details for graph
    @param[in] processor number being analysed
    @param[in] detailed_tooltip If true, the tooltip will contain a lot more
        information.
    """
    current_proc = proc
    graph_dict = create_graph_dict(endpoints, operators)

    main_graph = Digraph(format='svg')
    main_graph.attr(
        'graph',
        compound='true',
        fontname="Verdana",
        rankdir='LR',
        href='#Graph',
        tooltip='Graph'
    )

    for cluster_name in graph_dict:

        # source endpoint subgraph configuration
        source, source_invis = create_ep_img('source')

        # sink endpoint subgraph configuration
        sink, sink_invis = create_ep_img('sink')

        contains_source = False
        contains_sink = False
        for ep_id, details in graph_dict[cluster_name][ELEMENTS].items():
            if details[DIRECTION] == 'SOURCE':
                # add source nodes
                contains_source = True
                add_ep_img(
                    ep_id, details, source, details[EP_TITLE],
                    details[ALL_DETAILS], current_proc, detailed_tooltip
                )
            elif details[DIRECTION] == 'SINK':
                # add sink nodes
                contains_sink = True
                add_ep_img(
                    ep_id, details, sink, details[EP_TITLE],
                    details[ALL_DETAILS], current_proc, detailed_tooltip
                )
            else:
                raise Exception("Direction not supported!")

        # configure operator subgraph
        if not graph_dict[cluster_name][IS_OPERATOR]:
            if contains_source:
                cap1 = Digraph(name='cluster_' + str(cu.get_non_rep_nuber()))
                # configure operator source graph attributes
                cap1.attr(
                    'graph',
                    label=cluster_name + ' source',
                    style='filled',
                    fillcolor='gold',
                    color='black',
                    tooltip=cluster_name
                )
                cap1.subgraph(source)
                main_graph.subgraph(cap1)
            if contains_sink:
                cap2 = Digraph(name='cluster_' + str(cu.get_non_rep_nuber()))
                # configure operator sink graph attributes
                cap2.attr(
                    'graph',
                    label=cluster_name + ' sink',
                    style='filled',
                    fillcolor='gold',
                    color='black',
                    tooltip=cluster_name
                )
                cap2.subgraph(sink)
                main_graph.subgraph(cap2)
        else:
            op_name = cluster_name.split(' ')[0]
            op_id = cluster_name.split(' ')[1]
            op_num = str(hex(((int(op_id, 16) & STREAM_EP_OPID_MASK) >> 6)))
            cap = Digraph(name='cluster_' + str(cu.get_non_rep_nuber()))
            # configure operator attributes
            href = '#%sOperator%s,ep_op_id%s,%s' % (
                (current_proc + '_', op_num, op_id, op_name)
            )

            tooltip = get_operator_tooltip(
                cluster_name, graph_dict[cluster_name][ALL_DETAILS],
                detailed_tooltip
            )
            cap.attr(
                'graph',
                label=cluster_name,
                style='filled',
                href=href,
                fillcolor='lightblue3',
                color='black',
                tooltip=tooltip
            )
            cap.subgraph(source)
            cap.subgraph(sink)
            main_graph.edge(sink_invis, source_invis, style='invis')
            # add operator subgraph to main graph
            main_graph.subgraph(cap)

    # make connections between endpoints
    for transform in transforms:
        tooltip = get_transform_tooltip(transform, detailed_tooltip)
        main_graph.edge(
            "0x%04x" % (transform.source.id),
            "0x%04x" % (transform.sink.id),
            labeltooltip=tooltip,
            label=str(
                100 * transform.cbuffer.amt_data //
                (transform.cbuffer.buffer_size - 1)
            ) + "%%\\n%d\\n0x%04x" %
            (transform.cbuffer.buffer_size - 1, transform.id),
            href="#" + current_proc + "_" +
            transform.title_str.replace(" ", "")
        )

    return main_graph
