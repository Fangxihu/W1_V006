############################################################################
# CONFIDENTIAL
#
# Copyright (c) 2012 - 2017 Qualcomm Technologies International, Ltd.
#
############################################################################
"""
Module responsible for analysing Kymera Streams.
"""
import re
import os
import subprocess
import sys
import shutil
import threading
import traceback
import time
try:
    # Python 3
    import builtins
except ImportError:
    # Python 2
    import __builtin__ as builtins

from ACAT.Core import CoreUtils as cu
from ACAT.Core import CoreTypes as ct
from ACAT.Display.HtmlFormatter import HtmlFormatter
from ACAT.Display.InteractiveFormatter import InteractiveFormatter
from ACAT.Display.PlainTextFormatter import PlainTextFormatter
from . import Analysis
from ..Core import Graphviz

VARIABLE_DEPENDENCIES = {
    'strict': (
        'L_kick_object_list', '$_transform_list', '$_source_endpoint_list',
        '$_sink_endpoint_list', 'L_system_stream_rate', 'L_system_kick_period'
    ),
    'not_strict': (
        '$_stream_connect_fault',
        'L_rm_list',
    )
}
ENUM_DEPENDENCIES = {
    'strict': ('ENDPOINT_DIRECTION', 'STREAM_CONNECT_FAULT', 'ENDPOINT_TYPE')
}
TYPE_DEPENDENCIES = {
    'ENDPOINT': (
        'key', 'id', 'can_be_closed', 'can_be_destroyed',
        'destroy_on_disconnect', 'direction', 'is_real', 'con_id', 'cbops',
        'ep_to_kick', 'state'
    ),
    'TRANSFORM': (
        'id', 'source', 'sink', 'created_buffer', 'shared_buffer', 'buffer'
    ),
    'RATEMATCH_PAIR': ('ep1', 'ep2', 'enacting_ep', 'next'),
    'KICK_OBJECT': ('sched_ep', 'kick_ep')
}

FILE_LOCATION = os.path.join(os.getcwd(), 'temp', 'graph')


class Endpoint(object):
    """
    Base class to represent an endpoint in Kymera.
    """
    raw_data = None  # Endpoint-shaped variable we used to create this object
    # transform =  None # Used for finding the transform

    # The key of the endpoint.
    key = 0

    # The id of the endpoint. This is unique across all endpoint of the same
    # direction, i.e. it is possible for a source and a sink endpoint to have
    # the same id.
    id = 0  # Always represented in External format

    # Flag to say if the endpoint can be closed
    can_be_closed = None

    # Flag to say if the endpoint can be destroyed
    can_be_destroyed = None

    # Flag to say if an endpoint is created by streams
    internally_created = None

    # Flag to say if an endpoint is a source or a sink
    direction = None

    # Enum to say how the endpoint should be connected
    # type = 0 is already specified by the type of the object

    # Flag to say if an endpoint is a 'real' endpoint
    is_real = None

    # Flag to say if an endpoint has been started
    # (is processing data).
    is_enabled = None

    # Connection id of the owner of this endpoint
    con_id = None

    # Pointer to cbops_manager that encapsulates the cbops
    # information associated with the endpoint
    cbops = None

    # Pointer to the endpoint that this endpoint has a connection to.
    _connected_to = None

    # Pointer to the endpoint to kick
    _ep_to_kick = None

    # Pointer to next endpoint in the list.
    # next = 0 Additional information for the list not used in the stream
    # analysis

    # Information for latency control purposes.
    latency_ctrl_info = None

    state = None

    def __init__(self, ep, name, stream, is_state_type_equal_to_name=True):
        try:
            cbops_analysis = stream.interpreter.get_analysis(
                "cbops", stream.chipdata.processor
            )
        except KeyError:
            cbops_analysis = None
        self.raw_data = ep
        self.stream = stream

        self.address = ep.address
        self.key = ep.get_member('key').value
        self.hardware = None
        self.instance = None
        self.codec = None

        if name == "audio":
            stream_device = self.stream.debuginfo.get_enum("STREAM_DEVICE")
            hardware_value = (self.key & 0xFF00) >> 8
            self.instance = (self.key & 0xF0) >> 4
            self.channel = self.key & 0x000F

            for key, value in stream_device.items():
                if value == hardware_value:
                    self.hardware = key

        self.id = Stream.convert_ep_id(ep.get_member('id').value, 1)
        self.can_be_closed = ep.get_member('can_be_closed').value
        if self.can_be_closed == 0:
            self.can_be_closed = False
        else:
            self.can_be_closed = True

        self.can_be_destroyed = ep.get_member('can_be_destroyed').value
        if self.can_be_destroyed == 0:
            self.can_be_destroyed = False
        else:
            self.can_be_destroyed = True

        self.destroy_on_disconnect = ep.get_member(
            'destroy_on_disconnect'
        ).value
        if self.destroy_on_disconnect == 0:
            self.destroy_on_disconnect = False
        else:
            self.destroy_on_disconnect = True

        self.direction = stream.debuginfo.get_enum(
            'ENDPOINT_DIRECTION', ep.get_member('direction').value
        )[0]
        self.is_real = ep.get_member('is_real').value
        if self.is_real == 0:
            self.is_real = False
        else:
            self.is_real = True

        self.con_id = ep.get_member('con_id')
        if self.con_id is not None:
            self.con_id = self.con_id.value

        # cbops mgr
        self.cbops = ep.get_member('cbops')
        if self.cbops.value != 0:
            self.cbops = stream.chipdata.cast(self.cbops.value, "cbops_mgr")
            if stream.cbopsanalysis_on and not cbops_analysis is None:
                self.cbops = cbops_analysis.analyse_endpoints_cbops(self.cbops)
            else:
                self.cbops = str(self.cbops)
        else:
            self.cbops = str(self.cbops)

        self._connected_to = ep.get_member("connected_to").value
        self._ep_to_kick = ep.get_member('ep_to_kick').value

        self.state = ep.get_member('state')

        self.name = name

        self.is_state_type_equal_to_name = is_state_type_equal_to_name
        if is_state_type_equal_to_name:
            setattr(self, name, self.state.get_member(name))

    def __str__(self):
        """
        @brief Returns a tidy string representation of an endpoint object
        @param[in] self Pointer to the current object
        """
        return self.title_str + '\n' + self.desc_str

    @property
    def title_str(self):
        """
        @brief Returns the title string of the endpoint.
        @param[in] self Pointer to the current object
        """
        return self.compact_name()

    @property
    def desc_str(self):
        """
        @brief Returns the description string of the endpoint.
        @param[in] self Pointer to the current object
        """
        ep_str = "Address: " + cu.hex(self.address) + "\n"
        ep_str = ep_str + "Key: " + cu.hex(self.key) + "\n"
        # it's enough to check with one variable that it's the AudioEP
        if self.instance is not None:
            ep_str += "Device: " + str(self.hardware) + "\n"
            ep_str += "Instance: " + str(self.instance) + "\n"
            ep_str += "Channel: " + str(self.channel) + "\n"

        ep_str = ep_str + "is_real: " + \
            str(self.is_real) + " can_be_closed: " + \
            str(self.can_be_closed) + "\n"
        ep_str = ep_str + "can_be_destroyed: " + \
            str(self.can_be_destroyed) + " destroy_on_disconnect: " + \
            str(self.destroy_on_disconnect) + "\n"
        if self.con_id is not None:
            ep_str = ep_str + "Connection ID: " + str(self.con_id) + "\n"

        ep_str = ep_str + "connected_to: "
        try:
            connected_to = self.connected_to
            if connected_to is None:
                ep_str += "NULL\n"
            else:
                ep_str += cu.hex(connected_to.id) + \
                    "(" + connected_to.compact_name() + ") \n"
        except AttributeError:
            ep_str += (
                "0x%08x is not a valid endpoint address\n" % self._connected_to
            )

        ep_str += "ep_to_kick: "
        try:
            endpoint_to_kick = self.ep_to_kick
            if endpoint_to_kick is None:
                ep_str += "NULL\n"
            else:
                ep_str += cu.hex(endpoint_to_kick.id) + \
                    "(" + endpoint_to_kick.compact_name() + ") \n"
        except AttributeError:
            ep_str += (
                "0x%08x is not a valid endpoint address\n" % self._ep_to_kick
            )
        ep_str = ep_str + str(self.cbops)

        if self.is_state_type_equal_to_name:
            ep_str = ep_str + \
                str(getattr(self, self.name))

        return ep_str

    @property
    def connected_to(self):
        """
        @brief Returns the connected_to endpoint as an object.
        @param[in] self Pointer to the current object
        """
        return self.stream.get_endpoint_by_address(self._connected_to)

    @property
    def ep_to_kick(self):
        """
        @brief Returns the ep_to_kick endpoint as an object.
        @param[in] self Pointer to the current object
        """
        return self.stream.get_endpoint_by_address(self._ep_to_kick)

    def compact_name(self):
        """
        @brief Returns a tidy and short string representation of an endpoint object
        @param[in] self Pointer to the current object
        """
        ep_str = self.name.title() + " " + self.direction + " " + cu.hex(self.id)

        return ep_str


class AudioEndpoint(Endpoint):
    """
    @brief Audio endpoint is treated differently to display the associated timed
    playback module.
    """
    audio = None

    @property
    def desc_str(self):
        """
        @brief Returns the description string of the endpoint.
        @param[in] self Pointer to the current object
        """
        # Call the base class property function cannot be done otherwise
        ep_str = Endpoint.desc_str.fget(self)  # pylint: disable=no-member

        tp_ptr_var = self.audio.get_member("timed_playback")
        if tp_ptr_var:
            # timed playback is enabled in the build
            timed_playback_ptr = tp_ptr_var.value

            if timed_playback_ptr != 0:
                # the endpoint has a timed playback instance.
                timed_playback_var = self.stream.chipdata.cast(
                    addr=timed_playback_ptr,
                    type_def="TIMED_PLAYBACK",
                    elf_id=self.stream.debuginfo.get_kymera_debuginfo().elf_id
                )
                ep_str = ep_str.replace(
                    "timed_playback: " + cu.hex(timed_playback_ptr),
                    str(timed_playback_var).replace("\n", "\n  ")
                )

        return ep_str


class OperatorEndpoint(Endpoint):
    """
    @brief operator
    """
    operator = None

    def __init__(self, ep, name, stream):
        Endpoint.__init__(self, ep, 'operator', stream, False)
        self.operator = self.state.get_member('operator')
        self.name = self._get_op_name()

    # We don't need to override title_str as that delegate to compact_name

    # We don't need to override __str__ as that delegates to title_str and
    # desc_str.

    @property
    def desc_str(self):
        """
        @brief Returns the description string of the operator.
        @param[in] self Pointer to the current object
        """
        # Call the base class property function cannot be done otherwise
        ep_str = Endpoint.desc_str.fget(self)  # pylint: disable=no-member
        ep_str = ep_str + self.operator.__str__()

        return ep_str

    def _get_op_name(self):
        opmgr = self.stream.interpreter.get_analysis(
            "opmgr", self.stream.chipdata.processor
        )
        # for each operator ep, it gets the associated operator type
        name = opmgr.get_operator(self.id).cap_data.name
        return name


RE_ENDPOINT_TYPE = re.compile(r'endpoint_(\S+)')


class Transform(object):
    """
    @brief A structure for storing transform info
    """
    raw_data = None  # Transform-shaped variable we used to create this object

    # source endpoint of the transform
    source = None

    # source endpoint of the transform
    sink = None

    # Unique ID of the transform
    # Always represented in External format.
    id = 0

    # Indicating whether the transform created this buffer
    created_buffer = 0

    # transform buffer
    buffer = None

    def __init__(self, t, stream):
        # Populate the variables common to every transform
        self.raw_data = t
        self.address = t.address
        self.id = Stream.convert_transform_id(t.get_member('id').value, 1)

        src = t.get_member('source')
        self.source = stream.get_endpoint_by_address(src.value)

        sink = t.get_member('sink')
        self.sink = stream.get_endpoint_by_address(sink.value)

        self.created_buffer = t.get_member('created_buffer').value
        if self.created_buffer == 0:
            self.created_buffer = False
        else:
            self.created_buffer = True

        self.shared_buffer = t.get_member('shared_buffer').value
        if self.shared_buffer == 0:
            self.shared_buffer = False
        else:
            self.shared_buffer = True

        self.buffer = t.get_member('buffer').value
        buffers = stream.interpreter.get_analysis(
            "buffers", stream.chipdata.processor
        )
        self.cbuffer = buffers.inspect_cbuffer(self.buffer)

    def __str__(self):
        """
        @brief Returns a tidy string representation of a transform object
        @param[in] self Pointer to the current object
        """
        return self.short_title_str + '\n' + self.desc_str

    @property
    def title_str(self):
        """
        @brief Returns the title string of the transform.
        @param[in] self Pointer to the current object
        """
        return self.short_title_str + ' from ' + self.direction_str

    @property
    def direction_str(self):
        """
        @brief Returns the direction string of the transform.
        @param[in] self Pointer to the current object
        """
        return self.source.compact_name().replace(' SOURCE ', ' ') + ' to ' + \
            self.sink.compact_name().replace(' SINK ', ' ')

    @property
    def short_title_str(self):
        """
        @brief Returns a shorter title than title_str.
        @param[in] self Pointer to the current object
        """
        return "Transform " + cu.hex(self.id)

    @property
    def desc_str(self):
        """
        @brief Returns the description string of the transform.
        @param[in] self Pointer to the current object
        """
        tf_str = "Address: " + cu.hex(self.address) + "\n"
        tf_str = tf_str + "Source: " + self.source.compact_name() + "\n"
        tf_str = tf_str + "Sink: " + self.sink.compact_name() + "\n"
        if self.created_buffer:
            tf_str = tf_str + "The transform created the buffer \n"
        else:
            tf_str = tf_str + "The buffer was supplied by an endpoint \n"
        if self.shared_buffer:
            tf_str = tf_str + "The buffer shared \n"
        else:
            tf_str = tf_str + "The buffer is not shared \n"

        tf_str = tf_str + 'Buffer: ' + cu.hex(self.buffer) + '\n'

        return tf_str


class Stream(Analysis.Analysis):
    """
    @brief An analysis plug in for ACAT. Extends the Analysis class to provide a capability
    for analysing Streams.
    """

    @staticmethod
    def convert_transform_id(id_to_convert, force_direction=None):
        """
        @brief Takes a transform ID, and returns the internal/external version of it.
        If 'force' is set to 1, we will force the ID to be external;
        if 'force' is set to 0, we will force the ID to be internal.
        If 'force' isn't set, we toggle the ID between internal/external.
        Defined as a static method so that other classes/analyses can use it.
        @param[in] id_to_convert
        @param[in] force_direction = None
        """
        # TRANSFORM_COOKIE is defined in stream_connect.c, so this code needs
        # to be kept in sync with it if anything ever changes.

        # define TRANSFORM_COOKIE 0x1C1E
        cookie = 0x1C1E

        if force_direction is not None:
            already_external = False
            if id_to_convert & cookie > 0xff:
                already_external = True
            if ((force_direction == 0 and already_external) or
                    (force_direction == 1 and not already_external)):
                id_to_convert = id_to_convert ^ cookie
        else:
            id_to_convert = id_to_convert ^ cookie

        return id_to_convert

    @staticmethod
    def convert_ep_id(id_to_convert, force_direction=None):
        """
        @brief This does roughly the same as the firmware macro
        TOGGLE_EP_ID_BETWEEN_INT_AND_EXT: takes an id, and returns the
        internal/external version of it.
        If 'force' is set to 1, we will force the ID to be external;
        if 'force' is set to 0, we will force the ID to be internal.
        If 'force' isn't set, we toggle the ID between internal/external.
        Defined as a static method so that other classes/analyses can use it.
        @param[in] id_to_convert
        @param[in] force_direction = None
        """
        # All of this stuff is defined in stream.h and needs to be kept in
        # sync with it if anything ever changes.

        # In general, we don't need to do anthing to convert operator EPIDs
        # since the internal and external representations are the same.
        # Real endpoints have a 'cookie' applied, though.

        # define SOURCE_EP_COOKIE 0x0EA1
        # define SINK_EP_COOKIE   0x0A5E
        # define Stream.convert_ep_id(id_to_convert) \
        #    if ((id_to_convert & STREAM_EP_TYPE_MASK) == STREAM_EP_EXT_SOURCE) \
        #        id_to_convert ^= SOURCE_EP_COOKIE; \
        #    else if ((id_to_convert & STREAM_EP_TYPE_MASK) == STREAM_EP_EXT_SINK) \
        #        id_to_convert ^= SINK_EP_COOKIE;
        stream_ep_type = id_to_convert & 0xe000
        if stream_ep_type == 0x2000:
            # Real endpoint, source
            cookie = 0x0EA1
        elif stream_ep_type == 0xa000:
            # Real endpoint, sink
            cookie = 0x0A5E
        else:
            return id_to_convert  # operator EPID; bail out now

        if force_direction is not None:
            already_external = False
            if id_to_convert & cookie > 0xff:
                already_external = True
            if ((force_direction == 0 and already_external) or
                    (force_direction == 1 and not already_external)):
                id_to_convert = id_to_convert ^ cookie
        else:
            id_to_convert = id_to_convert ^ cookie

        return id_to_convert

    def __init__(self, **kwarg):
        """
        @brief Initialise the Sps Analysis object.
        @param[in] self Pointer to the current object
        @param[in] **kwarg
        """
        # Call the base class constructor.
        Analysis.Analysis.__init__(self, **kwarg)
        self.transforms = []
        self.endpoints = []
        self.cbopsanalysis_on = False

    def run_all(self, cbopsanalysis_on=False):
        """
        @brief Runs a complete analysis of Streams (transforms, endpoints, chains,
        ratematch pairs, kick objects).
        If cbopsanalysis_on == True, the CBOPs mgr will also be analysed and displayed.
        @param[in] self Pointer to the current object
        @param[in] cbopsanalysis_on = False
        """
        # self.cbopsanalysis_on decides on whether CBOPs are analysed or not
        self.cbopsanalysis_on = cbopsanalysis_on
        self.formatter.section_start("Streams")
        if Graphviz.is_grapviz_available():
            self.create_graph_img()
        self.analyse_system_configuration()
        self.analyse_stream_connect_fault()
        self.analyse_transforms(
            True, True
        )
        self.analyse_endpoints(self.cbopsanalysis_on)
        self.analyse_chains()
        self.plot_transforms_buffers(screen_width=80, live_plotting=False)
        self.analyse_rm_pairs()
        self.analyse_kick_objects()
        self.formatter.section_end()

    def get_endpoint(self, id_to_search):
        """
        @brief Finds the endpoint with id 'id_to_search'.
        'id_to_search' can be an endpoint ID in internal or external format.
        @param[in] self Pointer to the current object
        @param[in] id_to_search
        """
        self._read_all_endpoints()

        # Force to external representation
        id_to_search = Stream.convert_ep_id(id_to_search, 1)
        return self._find_endpoint(id_to_search)

    def get_endpoint_by_address(self, address):
        """
        @brief Finds the endpoint by address
        @param[in] self Pointer to the current object
        @param[in] address
        """
        self._read_all_endpoints()

        return self._find_endpoint_by_address(address)

    def get_endpoints(self):
        """
        @brief Returns a list with all available endpoints
        @param[in] self Pointer to the current object
        """
        self._read_all_endpoints()

        return self.endpoints

    @Analysis.cache_decorator
    def get_transform(self, id_to_search):
        """
        @brief Finds the transform with id 'id_to_search'.
        'id_to_search' can be a transform ID in internal or external format.
        @param[in] self Pointer to the current object
        @param[in] id_to_search
        """
        self._read_all_transforms()

        # Force to external representation
        id_to_search = Stream.convert_transform_id(id_to_search, 1)
        return self._find_transform(id_to_search)

    def get_transforms(self):
        """
        @brief Returns all the available transforms.
        @param[in] self Pointer to the current object
        """
        self._read_all_transforms()
        return self.transforms

    def get_ep_audio_sink_timed(self):
        """
        @brief Returns a list of all the audio sink endpoints which have time
        playback
        @param[in] self Pointer to the current object
        @param[out] list of all the audio sink endpoints which have time
        playback
        """

        ep_audio_sink_timed = []
        for ep in self.get_endpoints():
            if ep.name == 'audio' and ep.direction == 'SINK':
                if ep.audio.get_member('timed_playback').value != 0:
                    ep_audio_sink_timed.append(ep)

        return ep_audio_sink_timed

    def get_ep_audio_sink_timed_dict(self):
        """
        @brief Returns a dictionary which is used in ACATLab to draw graphs for
        audio sink endpoints which have timed playback
        @param[in] self Pointer to the current object
        @param[out] dictionary containing audio sink endpoint parameters and
        their addresses that need to be drawn in Matlab
        """

        ep_audio_sink_timed = self.get_ep_audio_sink_timed()
        ep_draw_dict = {}
        for ep in ep_audio_sink_timed:
            temp_dict = {}
            ep_draw_dict[ep.id] = temp_dict
            ep_chipdata = self.chipdata.cast(
                ep.audio.get_member('timed_playback').value, "TIMED_PLAYBACK"
            )
            current_tag = ep_chipdata.get_member('current_tag')
            playback_time = current_tag.get_member('playback_time').address
            timestamp = current_tag.get_member('timestamp').address

            temp_dict['error'] = current_tag.get_member('error').address
            temp_dict['timestamp_minus_playback_time'] = timestamp - \
                playback_time
            temp_dict['warp'] = ep_chipdata.get_member('pid_state').get_member(
                'warp'
            ).address
            temp_dict['sp_adjust'] = ep_chipdata.get_member(
                'tag_reframe'
            ).get_member('tag').get_member('sp_adjust').address
            ep_draw_dict[ep.id] = temp_dict

        return ep_draw_dict

    #######################################################################
    # Analysis methods - public since we may want to call them individually
    #######################################################################

    def analyse_stream_connect_fault(self):
        """
        @brief It checks if STREAM_CONNECT_FAULT_CODE is defined and if it is,
        its value is displayed.
        @param[in] self Pointer to the current object
        """
        try:
            faultvar = self.chipdata.get_var_strict('$_stream_connect_fault')
        except ct.DebugInfoNoVariable:
            # STREAM_CONNECT_FAULT_CODE probably not defined
            self.formatter.output(
                "STREAM_CONNECT_FAULT_CODE is not defined for this build."
            )
            return

        fault_str = self.debuginfo.get_enum(
            'STREAM_CONNECT_FAULT', faultvar.value
        )[0]  # enum name
        self.formatter.output("Stream connect fault code is " + fault_str)

    def analyse_transforms(
            self, display_buffer=False, analyse_metadata=False
        ):
        """
        @brief Displays the transform contents.
        @param[in] self Pointer to the current object
        @param[in] display_buffer If True the transform buffer is displayed.
                    False by default.
        @param[in] analyse_metadata If True the metadata of the transform buffer
                    is displayed. False by default.
        """
        self._read_all_transforms()
        if display_buffer:
            buffers_analysis = self.interpreter.get_analysis(
                "buffers", self.chipdata.processor
            )
        self.formatter.section_start("Transforms")
        for t in self.transforms:
            self.formatter.section_start(t.title_str)
            self.formatter.output(t.desc_str)
            if display_buffer:
                buffers_analysis.analyse_cbuffer(
                    t.buffer, t.title_str, analyse_metadata
                )
            self.formatter.section_end()

        self.formatter.section_end()

    def get_transforms_buffer_pointer(self):
        """
        @brief Retrun the transform buffers.
        @param[in] self Pointer to the current object
        @param[out] list of buffers
        """
        self._read_all_transforms()
        buffers = []
        for t in self.transforms:
            buffers.append((t.direction_str, t.buffer))
        return buffers

    def plot_transforms_buffers(self, screen_width=80, live_plotting=True):
        """
        @brief Displays the transform buffers usage in a console progress bar way [####    ].
        @param[in] self Pointer to the current object
        @param[in] screen width in characters.
        @param[in] live_plotting The live_plotting mode will continuously
            display the buffer usage until an exit event is received.
        """
        self._read_all_transforms()
        buffers = []
        for t in self.transforms:
            buffers.append((t.direction_str, t.buffer))

        if not isinstance(self.formatter, InteractiveFormatter):
            # live_plotting mode only available in the interactive formatter.
            live_plotting = False

        buffers_analysis = self.interpreter.get_analysis(
            "buffers", self.chipdata.processor
        )
        buffers_analysis.plot_buffers(buffers, screen_width, live_plotting)

    def analyse_service_priv(self, ep):
        """
        @brief Displays the service_priv info for audio data service endpoints.
        @param[in] self Pointer to the current object
        @param[in] ep Endpoint structure
        """
        try:
            service_priv_p = ep.__getattribute__(ep.name).get_member("service_priv").value
            service_priv = self.chipdata.cast(service_priv_p, "AUDIO_SINK_SOURCE")
        except (AttributeError, ct.InvalidDebuginfoType):
            # Do nothing if the field isn't present
            pass
        else:
            self.formatter.output(service_priv)
            self.formatter.output(
                self.chipdata.
                cast(service_priv.get_member("meta_desc").value, "META_DESC")
            )

    def analyse_endpoints(self, cbopsanalysis_on=False):
        """
        @brief Displays the contents of the endpoints.
        If cbopsanalysis_on == True, the CBOPs mgr analysis will also be displayed.
        @param[in] self Pointer to the current object
        @param[in] cbopsanalysis_on = False
        """
        self.cbopsanalysis_on = cbopsanalysis_on
        # Refresh our lists of endpoints and transforms
        self._read_all_endpoints()
        self._read_all_transforms()

        self.formatter.section_start("Endpoints")
        for endpoint in self.endpoints:
            self.formatter.section_start(endpoint.title_str)
            self.formatter.output(endpoint.desc_str)
            # Check the latency specific information
            if endpoint.name == "audio" and endpoint.direction == "SINK":
                running = endpoint.audio.get_member("running").value
                sync_started = endpoint.audio.get_member("sync_started").value
                latency_ctrl_info_var = endpoint.audio.get_member("latency_ctrl_info")
                silence_samples = latency_ctrl_info_var.get_member(
                    "silence_samples"
                ).value
                if running and sync_started != 0 and \
                        silence_samples != 0:
                    self.formatter.alert(
                        "Silence samples different from 0 for %s" %
                        (endpoint.compact_name())
                    )

            if endpoint.name in {"timestamped", "a2dp", "file", "usb"}:
                self.analyse_service_priv(endpoint)

            self.formatter.section_end()

        self.formatter.section_end()

    def analyse_endpoint(self, ep_id, cbopsanalysis_on=True):
        """
        @brief Displays the endpoint contents.
        @param[in] self Pointer to the current object
        @param[in] ep_id
        @param[in] cbopsanalysis_on = True
        """
        self.cbopsanalysis_on = cbopsanalysis_on
        # Refresh our lists of endpoints and transforms
        # (Need to do both, in this order, if we want endpoints to contain
        # a reference to attached transforms.)
        ep = self.get_endpoint(ep_id)

        self.formatter.output(str(ep))
        if ep.name == "audio" and ep.direction == "SINK":
            running = ep.audio.get_member("running").value
            sync_started = ep.audio.get_member("sync_started").value
            latency_ctrl_info_var = ep.audio.get_member("latency_ctrl_info")
            silence_samples = latency_ctrl_info_var.get_member(
                "silence_samples"
            ).value
            if running and sync_started != 0 and \
                    silence_samples != 0:
                self.formatter.alert(
                    "Silence samples different from 0 for %s" %
                    (ep.compact_name())
                )

        if ep.name in {"timestamped", "a2dp", "file", "usb"}:
            self.analyse_service_priv(ep)

    def analyse_chains(self):
        """
        @brief Draws each operator with the endpoints connected to endpoints of
        that operator
        @param[in] self Pointer to the current object

        """
        self._read_all_endpoints()
        self._read_all_transforms()
        try:
            opmgr = self.interpreter.get_analysis(
                "opmgr", self.chipdata.processor
            )
            # for each operator ep, it gets the associated operator type
            operators = opmgr.get_oplist("object")
        except KeyError:
            operators = []
        graph_dict = Graphviz.create_graph_dict(self.endpoints, operators)
        self.formatter.section_start("Chains")
        empty_text_sink = '        |      '
        empty_text_source = '      |'
        for cluster_name in graph_dict:
            output_text = '.          /--------------------\\\n'
            output_text += "." + cluster_name.center(42) + '\n'
            source_list = []
            sink_list = []
            for ep_id, details in graph_dict[cluster_name][Graphviz.ELEMENTS].items():
                if details[Graphviz.DIRECTION] == 'SOURCE':
                    source_list.append(
                        ep_id + '|->' + details[Graphviz.CONNECTED_TO_ID]
                    )
                else:
                    sink_list.append(
                        details[Graphviz.CONNECTED_TO_ID] + '->|' + ep_id
                    )

            for i in range(max(len(source_list), len(sink_list))):
                text_to_output = ' %s        %s' % (
                    sink_list[i]
                    if i < len(sink_list) else empty_text_sink, source_list[i]
                    if i < len(source_list) else empty_text_source
                )
                output_text += ('. ' + text_to_output + '\n')
            output_text += r'.          \--------------------/'
            output_text += '\n'
            self.formatter.output(output_text)
        self.formatter.section_end()

    def _create_and_output_graph(self, operators, detailed_tooltip):
        """
        @param[in] self Pointer to the current object
        @param[in] operators available operators.
        @param[in] detailed_tooltip If true, the tooltip will contain a lot more
            information.
        """
        graph = Graphviz.create_graph(
            self.endpoints, self.transforms, operators, self.formatter.proc,
            detailed_tooltip
        )
        try:
            if type(self.formatter) is InteractiveFormatter:
                graph.render(FILE_LOCATION, cleanup=True, view=True)
                self.formatter.output(" file in " + FILE_LOCATION)
            elif type(self.formatter) is HtmlFormatter:
                # Create legend for graph image when formatter is Html
                legend = Graphviz.create_legend()
                if not cu.global_options.under_test:
                    self.formatter.output_svg(builtins.str(graph.pipe(format='svg')))
                    self.formatter.output_svg(builtins.str(legend.pipe(format='svg')))
                else:
                    # display the graph content as a string.
                    self.formatter.output_raw(builtins.str(graph))
                    self.formatter.output_raw(builtins.str(legend))
        except RuntimeError:
            raise Exception(
                "Please install graphviz from www.graphviz.org\n"
                "Make sure the Graphviz is in the PATH"
            )

    def create_graph_img(self):
        """
        @brief Draws a graphical representation of streams chains using
        graphviz. This is done by first extracting the endpoint details from
        the dictionary and drawing nodes and edges from that dictionary
        @param[in] self Pointer to the current object
        """
        if not Graphviz.is_grapviz_available():
            self.formatter.output(
                "Graph image cannot be produced as graphviz python is not "
                "installed.\n Use pip install graphviz"
                ""
            )
            return
        if type(self.formatter) is PlainTextFormatter:
            # do not draw when in plain text mode
            return

        self._read_all_endpoints()
        self._read_all_transforms()

        opmgr = self.interpreter.get_analysis("opmgr", self.chipdata.processor)
        # for each operator ep, it gets the associated operator type
        operators = opmgr.get_oplist("object")

        try:
            # call function to create graphviz
            self._create_and_output_graph(operators, True)
        except subprocess.CalledProcessError:
            # Probably the detaild tooltip cause issues
            self._create_and_output_graph(operators, False)

    def create_live_graph_img(self, set_refresh_interval=3):
        """
        @brief Draws a live graphical representation of streams chains using
        graphviz. This is done by first extracting the endpoint details from
        the dictionary and drawing nodes and edges from that dictionary
        @param[in] self Pointer to the current object
        """
        filename = 'live_graph.html'
        html_file = open(filename, 'w+')
        live_html_text = '''
        <!DOCTYPE html>
        <html>
        <head>
        </head>
        <body onLoad = "javascript: setTimeout('document.location.reload();', %d)">
                <img src = "temp/graph_final.svg">
        </body>
        </html>
        ''' % (set_refresh_interval * 1000)
        html_file.write(live_html_text)
        html_file.close()

        # try to opend the html file
        if os.name == 'nt':
            os.system("start " + html_file.name)
        elif os.name == 'nt':
            os.system("open " + html_file.name)
        else:
            # Give up. Let the user open it.
            self.formatter.output("Please open " + html_file.name)

        # start_new_thread(self._live_graph, ())

        self.formatter.output("Press enter to exit from graph update.")
        # Create and run the GrapUdater
        exit_event = threading.Event()
        exit_event.clear()
        reader = GraphUpdate(
            exit_event=exit_event,
            helper=self
        )
        reader.start()
        # wait until a key is pressed.
        sys.stdin.read(1)
        exit_event.set()
        # wait for the task to finish
        reader.join()

    def analyse_rm_pairs(self):
        """
        @brief Function will print out the available ratematch pairs.
        @param[in] self Pointer to the current object
        """
        self.formatter.section_start("Ratematch pairs")
        pair_found = False
        try:
            _rm_list = self.debuginfo.get_var_strict('L_rm_list').address

            for rm_pair in self.parse_linked_list(_rm_list, 'next'):
                ep1 = self.get_endpoint_by_address(
                    rm_pair.get_member("ep1").value
                )
                ep2 = self.get_endpoint_by_address(
                    rm_pair.get_member("ep2").value
                )
                enacting_ep = self.get_endpoint_by_address(
                    rm_pair.get_member("enacting_ep").value
                )
                self.formatter.section_start("")
                self.formatter.output(
                    "    pair: %-34s %-34s" %
                    (ep1.compact_name(), ep2.compact_name())
                )
                self.formatter.output(
                    "enacting: %-34s" % (enacting_ep.compact_name())
                )
                self.formatter.section_end()
                pair_found = True
        except ct.DebugInfoNoVariable:
            pass

        if not pair_found:
            self.formatter.output("No ratematch pair found.")

        self.formatter.section_end()

    def analyse_kick_objects(self):
        """
        @brief Function will print out the available kick objects.
        @param[in] self Pointer to the current object
        """
        self.formatter.section_start("Kick objects")
        _kick_object_list = self.debuginfo.get_var_strict(
            'L_kick_object_list'
        ).address
        kick_object_found = False
        kcik_obj_list = self.parse_linked_list(
            _kick_object_list, 'next'
        )
        for kick_object in kcik_obj_list:
            sched_ep = self.get_endpoint_by_address(
                kick_object.get_member("sched_ep").value
            )
            kick_ep = self.get_endpoint_by_address(
                kick_object.get_member("kick_ep").value
            )
            self.formatter.section_start("")
            self.formatter.output(
                "     kick endpoint: %-34s" % (kick_ep.compact_name())
            )
            self.formatter.output(
                "scheduler endpoint: %-34s" % (sched_ep.compact_name())
            )
            self.formatter.section_end()
            kick_object_found = True

        if not kick_object_found:
            self.formatter.output("No kick object found.")

        self.formatter.section_end()

    def analyse_system_configuration(self):
        """
        @brief Prints out the system stream configuration.
        @param[in] self Pointer to the current object
        """
        try:
            system_stream_rate = self.chipdata.get_var_strict(
                "L_system_stream_rate"
            )
            system_kick_period = self.chipdata.get_var_strict(
                "L_system_kick_period"
            )
        except ct.DebugInfoNoVariable:
            self.formatter.output(
                "There is no system stream configuration in the build."
            )
            return
        system_stream_rate = system_stream_rate.value
        system_kick_period = system_kick_period.value
        output_str = "The system sample rate is %s" % (system_stream_rate)
        if system_stream_rate == 0:
            output_str += " (uninitialized)"

        output_str += "\nThe system kick period is %s us" % (system_kick_period)
        if system_kick_period == 0:
            output_str += " (uninitialized)"

        self.formatter.output(
            output_str
        )

    def _read_all_transforms(self):
        """
        @brief Read all transforms and populate self.transforms.
        @param[in] self Pointer to the current object
        """
        self.transforms = self._read_raw_transforms()

    @Analysis.cache_decorator
    def _read_raw_transforms(self):
        transforms = []
        for transform in self.parse_linked_list('$_transform_list', 'next'):
            transforms.append(self._read_transform(transform))

        return transforms

    #######################################################################
    # Private methods - don't call these externally.
    #######################################################################

    def _read_transform(self, transform):
        """
        @brief Takes a raw transform Variable (cast from the linked-list)
        and inspects it properly.
        @param[in] self Pointer to the current object
        @param[in] transform
        """
        return Transform(transform, self)

    def _read_all_endpoints(self):
        """
        @brief Read all endpoints and populate self.endpoints.
        @param[in] self Pointer to the current object
        """
        # Initialise the list so that this works on a live chip
        self.endpoints = self._read_raw_endpoints()

    @Analysis.cache_decorator
    def _read_raw_endpoints(self):
        endpoints = []
        for source in self.parse_linked_list('$_source_endpoint_list', 'next'):
            endpoints.append(self._read_endpoint(source))
        for sink in self.parse_linked_list('$_sink_endpoint_list', 'next'):
            endpoints.append(self._read_endpoint(sink))

        return endpoints

    def _read_endpoint(self, ep):
        """
        @brief Takes a raw endpoint Variable (cast from the linked-list)
        and inspects it properly.
        @param[in] self Pointer to the current object
        @param[in] ep
        """
        # Check the endpoint_type of the endpoint
        ep_type = self.debuginfo.get_enum(
            'ENDPOINT_TYPE', ep.get_member('stream_endpoint_type').value
        )[0]
        ep_type = RE_ENDPOINT_TYPE.search(ep_type).group(1)

        if ep_type == "operator":  # operator
            return OperatorEndpoint(ep, ep_type, self)
        # audio endpoint is treated differently to show the timedplayback
        # module.
        if ep_type == "audio":
            return AudioEndpoint(ep, ep_type, self)

        # Endpoint name from endpoint ID enum is equal to endpoint state
        # type.
        return Endpoint(ep, ep_type, self)

    @Analysis.cache_decorator
    def _find_endpoint(self, id_to_search):
        """
        @brief NB searches for a transform using its *external* ID
        @param[in] self Pointer to the current object
        @param[in] id_to_search
        """
        for ep in self.endpoints:
            if ep.id == id_to_search:
                return ep
        return None

    @Analysis.cache_decorator
    def _find_endpoint_by_address(self, address):
        """
        @brief NB searches for a endpoint by its address.
        @param[in] self Pointer to the current object
        @param[in] address
        """
        for ep in self.endpoints:
            if ep.address == address:
                return ep
        return None

    @staticmethod
    def _ep_is_real_or_shadow(ep):
        """
        @brief Checks if an endpoint is real or shadow.
        @param[in] ep
        @param[out] True if ep real or shadow, false otherwise.
        """
        return ep.is_real or ep.name == "shadow"

    def _find_real_and_shadow_endpoints(self):
        """
        @brief This method searches for real and shadow endpoint.
        @param[in] self Pointer to the current object
        @param[out] List of real and shadow endpoints.
        """
        real_ep = []
        for ep in self.endpoints:
            if self._ep_is_real_or_shadow(ep):
                real_ep.append(ep)

        return real_ep

    @Analysis.cache_decorator
    def _find_transform(self, id_to_search):
        """
        @brief NB searches for a transform using its *external* ID
        @param[in] self Pointer to the current object
        @param[in] id_to_search
        """
        for tfm in self.transforms:
            if tfm.id == id_to_search:
                return tfm
        return None


class GraphUpdate(threading.Thread):
    """
    @brief This class is responsible for creating graph images for the html
    file which displays the image.
    """

    def __init__(self, exit_event, helper):
        """
        @brief Initialisation function for a GraphUpdate object.

        @param[in] self Pointer to the current object
        @param[in] exit_event Event which stops the thread.
        @param[in] helper Pointer to the analyses
        """
        threading.Thread.__init__(self)
        self.exit_event = exit_event
        self.helper = helper

    def run(self):
        """
        @brief Run method for the thread.

        @param[in] self Pointer to the current object
        """
        # get the operator manager
        opmgr = self.helper.interpreter.get_analysis(
            "opmgr",
            self.helper.chipdata.processor
        )
        # run until an exit event is received.
        while not self.exit_event.is_set():
            try:
                self.helper._read_all_endpoints()
                self.helper._read_all_transforms()
                # for each operator ep, it gets the associated operator type
                operators = opmgr.get_oplist("object")
                graph = Graphviz.create_graph(
                    self.helper.endpoints,
                    self.helper.transforms,
                    operators,
                    self.helper.formatter.proc,
                    False
                )
                graph.render(FILE_LOCATION, cleanup=True)
                # To cut down on flashing image at the html the output is copied
                # to a final location. This reduces the flashing because moving
                # is faster than rendering, but it cannot completely avoid.
                shutil.move(FILE_LOCATION + '.svg', FILE_LOCATION + '_final.svg')
            except BaseException:
                self.helper.formatter.output(
                    "\n" + traceback.format_exc() +
                    "\nException raised! Continuing execution.\n"
                )
                # sleep a few second to avoid entering to the same error
                # too fast.
                time.sleep(2)
