# Seeking

Seeking in GStreamer means configuring the pipeline for playback of the
media between a certain start and stop time, called the playback
segment. By default a pipeline will play from position 0 to the total
duration of the media at a rate of 1.0.

A seek is performed by sending a `SEEK` event to the sink elements of a
pipeline. Sending the `SEEK` event to a bin will by default forward the
event to all sinks in the bin.

When performing a seek, the start and stop values of the segment can be
specified as absolute positions or relative to the currently configured
playback segment. Note that it is not possible to seek relative to the
current playback position. To seek relative to the current playback
position, one must query the position first and then perform an absolute
seek to the desired position.

If a seek operation is requested using the `GST_SEEK_FLAG_FLUSH` flag, all
pending data in the pipeline is discarded and playback starts from the new
position immediately.  If this flag is not set, the seek is queued to be
executed as soon as possible, which might be after all queues are emptied.

Seeking can be performed in different formats such as time, frames or
samples.

The seeking can be performed to a nearby key unit or to the exact
(estimated) unit in the media (`GST_SEEK_FLAG_KEY_UNIT`). See below
for more details on this.

The seeking can be performed by using an estimated target position or in
an accurate way (`GST_SEEK_FLAG_ACCURATE`). For some formats this can
result in having to scan the complete file in order to accurately find
the target unit. See below for more details on this.

Non segment seeking will make the pipeline emit EOS when the configured
segment has been played.

Segment seeking (using the `GST_SEEK_FLAG_SEGMENT`) will not emit an
EOS at the end of the playback segment but will post a `SEGMENT_DONE`
message on the bus. This message is posted by the element driving the
playback in the pipeline, typically a demuxer. After receiving the
message, the application can reconnect the pipeline or issue other seek
events in the pipeline. Since the message is posted as early as possible
in the pipeline, the application has some time to issue a new seek to
make the transition seamless. Typically the allowed delay is defined by
the buffer sizes of the sinks as well as the size of any queues in the
pipeline.

The seek can also change the playback speed of the configured segment. A
speed of 1.0 is normal speed, 2.0 is double speed. Negative values mean
backward playback.

When performing a seek, several trickmode flags can be used to instruct
decoders and demuxers that they are allowed to skip decoding in various
ways. This is most useful when changing to a playback rate different
to 1.0 and helps when resource consumption is more important than
accurately producing all frames.

The trickmode flags are:

  - [](GST_SEEK_FLAG_TRICKMODE_KEY_UNITS): Only decode/display key frames
  - [](GST_SEEK_FLAG_TRICKMODE_FORWARD_PREDICTED): Skip B-frames
  - [](GST_SEEK_FLAG_TRICKMODE_NO_AUDIO): Don't decode audio

In some pipelines, it is possible to control the playback rate instantly
by sending a seek with the [](GST_SEEK_FLAG_INSTANT_RATE_CHANGE)
flag. This flag does not work for all pipelines, in which case it is necessary to
send a full flushing seek to change the playback rate. When using this
flag, the seek event is only allowed to change the current rate and can
modify the trickmode flags, but it is not possible to change the current
playback position or flush.

Instant rate changing is handled in the pipeline in a specific sequence.

1. The application creates and sends a seek event with the
[](GST_SEEK_FLAG_INSTANT_RATE_CHANGE) flag and `start_type = stop_type = GST_SEEK_TYPE_NONE`.
2. When the seek event reaches an element that will perform the seek
operation, that element calculates a rate multiplier according to the
requested playback rate, divided by the element's current output rate
(from the most recent seek, but usually 1.0). It also extracts the
new trickmode flags from the seek event - the set of flags in
[](GST_SEGMENT_INSTANT_FLAGS)
3. The element sends a downstream [](GST_EVENT_INSTANT_RATE_CHANGE) containing
the rate multiplier, and the flags subset, and copying the seqnum from the
seek event.
4. Downstream elements which handle the instant-rate-change event will
update their trickmode flags, and (if they sync to the clock) send a
[](GST_MESSAGE_INSTANT_RATE_REQUEST) message on the bus, with the event seqnum.
5. The pipeline handles the message on the bus and responds with a
[](GST_EVENT_INSTANT_RATE_SYNC_TIME)
event into the pipeline, which informs all elements to switch to the new
playback rate multiplier, and with a running-time and upstream-running-time
from which the new rate applies.  All elements now start synchronising to
the clock using a new multiplied playback rate effective from the
indicated running-time. The difference between running-time (the clock
time at which the switch happens) and upstream-running-time is equal to
the amount of accumulated extra playback due to chained instant-rate-changes.

<!-- FIXME # Seeking in push based elements-->


## Generating seeking events

A seek event is created with `gst_event_new_seek()`.

## Seeking variants

The different kinds of seeking methods and their internal workings are
described below.

### FLUSH seeking

This is the most common way of performing a seek in a playback
application. The application issues a seek on the pipeline and the new
media is immediately played after the seek call returns.

### seeking without FLUSH

This seek type is typically performed after issuing segment seeks to
finish the playback of the pipeline.

Performing a non-flushing seek in a `PAUSED` pipeline blocks until the
pipeline is set to playing again, since all data passing is blocked in
the prerolled sinks.

### segment seeking with FLUSH

This seek is typically performed when starting seamless looping.

### segment seeking without FLUSH

This seek is typically performed when continuing seamless looping.

## `KEY_UNIT` and `ACCURATE` flags

This section aims to explain the behaviour expected by an element with
regard to the `KEY_UNIT` and `ACCURATE` seek flags, using a parser
or demuxer as an example.

### DEFAULT BEHAVIOUR:

When a seek to a certain position is requested, the demuxer/parser will
do two things (ignoring flushing and segment seeks, and simplified for
illustration purposes):

  - send a segment event with a new start position

  - start pushing data/buffers again

To ensure that the data corresponding to the requested seek position can
actually be decoded, a demuxer or parser needs to start pushing data
from a keyframe/keyunit at or before the requested seek position.

Unless requested differently (via the `KEY_UNIT` flag), the start of the
segment event should be the requested seek position.

So by default a demuxer/parser will then start pushing data from
position DATA and send a segment event with start position `SEG_START`,
and `DATA ??? SEG_START`.

If `DATA < SEG_START`, a well-behaved video decoder will start decoding
frames from DATA, but take into account the segment configured by the
demuxer via the segment event, and only actually output decoded video
frames from `SEG_START` onwards, dropping all decoded frames that are
before the segment start and adjusting the timestamp/duration of the
buffer that overlaps the segment start ("clipping"). A
not-so-well-behaved video decoder will start decoding frames from DATA
and push decoded video frames out starting from position DATA, in which
case the frames that are before the configured segment start will
usually be dropped/clipped downstream (e.g. by the video sink).

###  `GST_SEEK_FLAG_KEY_UNIT`

If the `KEY_UNIT` flag is specified, the demuxer/parser should adjust the
segment start to the position of the key frame closest to the requested
seek position and then start pushing out data from there. The nearest
key frame may be before or after the requested seek position, but many
implementations will only look for the closest keyframe before the
requested position.

Most media players and thumbnailers do (and should be doing) `KEY_UNIT`
seeks by default, for performance reasons, to ensure almost-instant
responsiveness when scrubbing (dragging the seek slider in `PAUSED` or
`PLAYING` mode). This works well for most media, but results in suboptimal
behaviour for a small number of *odd* files (e.g. files that only have
one keyframe at the very beginning, or only a few keyframes throughout
the entire stream). At the time of writing, a solution for this still
needs to be found, but could be implemented demuxer/parser-side, e.g.
make demuxers/parsers ignore the `KEY_UNIT` flag if the position
adjustment would be larger than 1/10th of the duration or somesuch.

Flags can be used to influence snapping direction for those cases where
it matters. `SNAP_BEFORE` will select the preceding position to the seek
target, and `SNAP_AFTER` will select the following one. If both flags are
set, the nearest one to the seek target will be used. If none of these
flags are set, the seeking implemention is free to select whichever it
wants.

#### Summary:

  - if the `KEY_UNIT` flag is **not** specified, the demuxer/parser
    should start pushing data from a key unit preceding the seek
    position (or from the seek position if that falls on a key unit),
    and the start of the new segment should be the requested seek
    position.

  - if the `KEY_UNIT` flag is specified, the demuxer/parser should start
    pushing data from the key unit nearest the seek position (or from
    the seek position if that falls on a key unit), and the start of the
    new segment should be adjusted to the position of that key unit
    which was nearest the requested seek position (ie. the new segment
    start should be the position from which data is pushed).

### `GST_SEEK_FLAG_ACCURATE`

If the `ACCURATE` flag is specified in a seek request, the demuxer/parser
is asked to do whatever it takes (!) to make sure the position
seeked to is accurate in relation to the beginning of the stream. This
means that it is not acceptable to just approximate the position (e.g.
using an average bitrate). The achieved position must be exact. In the
worst case, the demuxer or parser needs to push data from the beginning
of the file and let downstream clip everything before the requested
segment start.

The `ACCURATE` flag does not affect what the segment start should be in
relation to the requested seek position. Only the `KEY_UNIT` flag (or its
absence) has any effect on that.

Video editors and frame-stepping applications usually use the `ACCURATE`
flag.

#### Summary:

  - if the `ACCURATE` flag is **not** specified, it is up to the
    demuxer/parser to decide how exact the seek should be. In this case,
    the expectation is that the demuxer/parser does a
    resonable best effort attempt, trading speed for accuracy. In the
    absence of an index, the seek position may be approximated.

  - if the `ACCURATE` flag **is** specified, absolute accuracy is required,
    and speed is of no concern. It is not acceptable to just approximate
    the seek position in that case.

  - the `ACCURATE` flag does not imply that the segment starts at the
    requested seek position or should be adjusted to the nearest
    keyframe, only the `KEY_UNIT` flag determines that.

### `ACCURATE` and `KEY_UNIT` combinations:

All combinations of these two flags are valid:

  - neither flag specified: segment starts at seek position, send data
    from preceding key frame (or earlier), feel free to approximate the
    seek position

  - only `KEY_UNIT` specified: segment starts from position of nearest
    keyframe, send data from nearest keyframe, feel free to approximate
    the seek position

  - only `ACCURATE` specified: segment starts at seek position, send data
    from preceding key frame (or earlier), do not approximate the seek
    position under any circumstances

  - `ACCURATE | KEY_UNIT` specified: segment starts from position of
    nearest keyframe, send data from nearest key frame, do not
    approximate the seek position under any circumstances
