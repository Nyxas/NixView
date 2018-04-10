import nixio as nix
import lif
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.mlab as mlab
from PIL import Image as img
from IPython import embed


def fake_neuron():
    lif_model = lif.lif()
    t, v, spike_times = lif_model.run_const_stim(10000, 0.005)
    return t, v, spike_times


def create_1d_sampled(f, b):
    sample_interval = 0.00005
    t = np.arange(0., 0.5, sample_interval)
    f1 = 100.
    f2 = 200.
    a1 = 0.825
    a2 = 0.4
    eod = a1 * np.sin(t * f1 * 2 * np.pi) + a2 * np.sin(f2 * t * 2 * np.pi)
    da = b.create_data_array('eod', 'nix.regular_sampled', data=eod)
    da.definition = "Recording of an electric fish's electric organ discharge. \
    Demontrates the use of DataArrays to store 1-D data that is \
    regularly sampled in time. The DataArray contains one dimension \
    descriptor that defines how the time-axis is resolved."
    da.unit = 'mV/cm'
    da.label = 'electric field'
    da.description = ""

    d = da.append_sampled_dimension(sample_interval)
    d.unit = 's'
    d.label = 'time'

    sec = f.create_section('in vivo 1', 'setup')
    hw = sec.create_section('amplifier', 'hardware.amplifier')
    hw['model'] = 'EXT 2F'
    hw['manufacturer'] = 'npi electronics'
    hw['gain'] = 1000

    subj = f.create_section('2015_albi_10', 'subject.animal')
    subj['species'] = 'Apteronotus albifrons'
    subj['sex'] = 'male'

    src1 = b.create_source('setup', 'nix.source.setup')
    src1.metadata = sec

    src2 = b.create_source('subject', 'nix.source.subject')
    src2.metadata = subj


def create_2d(f, b, trials=10):
    # create multiple responses of a lif model neuron
    voltages = []
    for t in range(trials):
        time, voltage, _ = fake_neuron()
        voltages.append(voltage)

    voltages = np.asarray(voltages).T

    da = b.create_data_array("membrane voltages", "nix.regular_sampled.series",
                             dtype=nix.DataType.Double, data=voltages)
    d = da.append_sampled_dimension(time[1]-time[0])
    d.label = "time"
    d.unit = "s"
    da.label = "voltage"
    da.unit = "mV"
    da.append_set_dimension()

    average_v = b.create_data_array("average response", "nix.regular_sampled",
                                    dtype=nix.DataType.Double, data=np.mean(voltages, axis=1))
    average_v.label = "voltage"
    average_v.unit = "mV"
    dim = average_v.append_sampled_dimension(time[1] - time[0])
    dim.unit = "s"
    dim.label = "time"

    tag = b.create_tag("average response", "nix.epoch", [0.0])
    tag.extent = [time[-1]]
    tag.definition = "Average repsonse of the model neuron. The original responses\
    are referenced and the average response is linked as a feature of these."
    tag.references.append(da)
    tag.create_feature(average_v, nix.LinkType.Untagged)


def create_3d(f, b):
    # taken from nix tutorial
    image = img.open('lena.bmp')
    img_data = np.array(image)
    channels = list(image.mode)
    data = b.create_data_array("lena", "nix.image.rgb", data=img_data)
    # add descriptors for width, height and channels
    height_dim = data.append_sampled_dimension(1)
    height_dim.label = "height"
    width_dim = data.append_sampled_dimension(1)
    width_dim.label = "width"
    color_dim = data.append_set_dimension()
    color_dim.labels = channels


def create_1d_range(f, b):
    da = b.data_arrays['eod']
    eod = da[:]
    time = np.asarray(da.dimensions[0].axis(len(eod)))
    shift_eod = np.roll(eod, 1)
    xings = time[(eod > 0) & (shift_eod < 0)]

    range_da = b.create_data_array('zero crossings', 'nix.event', data=xings)
    range_da.definition = "1-D data that is irregularly sampled in time. That \
    is, the time between consecutive sampling points is not regular. Here we \
    store the times at which a signal crossed the zero line. The content of the \
    DataArray itself defines the time-axis the only dimension descriptor is thus \
    an \"aliasRange\" dimension."
    d = range_da.append_alias_range_dimension()
    d.unit = 's'
    d.label = 'time'


def create_1d_set(f, b):
    temp = [13.7, 16.3, 14.6, 11.6, 8.6, 5.7, 4., 2.6, 3., 4., 8.5, 13.1]
    labels = ["Sep", "Aug", "Jul", "Jun", "Mai", "April", "Mar", "Feb", "Jan", "Dec", "Nov", "Okt"]

    da = b.create_data_array("average temperature", "nix.catergorical", data=temp)
    da.definition = "1-D categorical data can also be stored in a DataArray entity. The dimension\
    descriptor is in this case a SetDimension. The labels stored in this dimension are used to \
    label the ticks of the x-axis."
    da.label = "temperature"
    da.unit = "C"

    d = da.append_set_dimension()
    d.labels = labels

    src = b.create_source("Data source", "nix.source")
    da.sources.append(src)

    s = f.create_section("Helgoland Weather data", "data_origin")
    s["period"] = "201509 - 201410"
    s["url"] = "http://www.dwd.de/DE/leistungen/klimadatendeutschland/klimadatendeutschland.html"
    src.metadata = s
    return src


def create_2d_set(f, b, source):
    temp = [13.7, 16.3, 14.6, 11.6, 8.6, 5.7, 4., 2.6, 3., 4., 8.5, 13.1]
    temp_min = [12.3, 13.8, 12.1, 9.9, 6.6, 1.4, 1.5, -0.2, -1.5, -1.4, 0.5, 9.4]
    temp_max = [18.7, 23.6, 25.9, 20., 16.6, 11.7, 9.5, 7.2, 9.8, 10.5, 15.8, 18.8]
    xlabels = ["Sep", "Aug", "Jul", "Jun", "Mai", "April", "Mar", "Feb", "Jan", "Dec", "Nov", "Okt"]
    ylabels = ["Min", "Avg", "Max"]

    da = b.create_data_array("2D set of temperatures", "nix.catergorical.series",
                             data=np.vstack([temp_min, temp, temp_max]))
    da.label = "temperature"
    da.unit = "C"
    d1 = da.append_set_dimension()
    d1.labels = ylabels
    d2 = da.append_set_dimension()
    d2.labels = xlabels
    da.sources.append(source)


def create_2d_sample(f, b):
    # stolen fom matplolib examples http://matplotlib.org/examples/pylab_examples/image_demo.html
    delta = 0.025
    x = y = np.arange(-3.0, 3.0, delta)
    X, Y = np.meshgrid(x, y)
    Z1 = mlab.bivariate_normal(X, Y, 1.0, 1.0, 0.0, 0.0)
    Z2 = mlab.bivariate_normal(X, Y, 1.5, 0.5, 1, 1)
    Z = Z2 - Z1  # difference of Gaussians

    da = b.create_data_array("difference of Gaussians", "nix.2d.heatmap", data=Z)
    d1 = da.append_sampled_dimension(delta)
    d1.label = "x"
    d1.offset = -3.
    d2 = da.append_sampled_dimension(delta)
    d2.label = "y"
    d2.offset = -3.


def create_m_tag(f, b):
    trace = b.data_arrays["eod"]
    event_times = b.data_arrays["zero crossings"]
    mt = b.create_multi_tag("special events", "nix.event_times", event_times)
    mt.definition = "A MultiTag entity is used to annotate multiple events or segments \
    in a number of referenced DataArrays. In this example, the events are the zero \
    crossings (see 1-D DataArrays) in the EOD. Thus, the one DataArray (zeros crossings) \
    is used to mark the time points in the other (EOD)."
    mt.references.append(trace)

    positions = b.create_data_array('epoch_starts', 'nix.event', data=[0.05, 0.35])
    positions.append_set_dimension()
    extents = b.create_data_array('epoch_ends', 'nix.event', data=[0.1, 0.1])
    extents.append_set_dimension()
    mtag = b.create_multi_tag("epochs", "nix.event_epochs", positions)
    mtag.references.append(trace)
    mtag.extents = extents

    feature_1 = b.create_data_array("feature 1", "nix.feature",
                                    data=np.sin(100 * 2 * np.pi * np.arange(0, 0.1, 0.00005)))
    d = feature_1.append_sampled_dimension(0.00005)
    d.unit = "s"
    d.label = "time"
    feature_1.unit = "mV"
    feature_1.label = "voltage"
    feature_2 = b.create_data_array("feature 2", "nix.feature",
                                    data=np.cos(150 * 2 * np.pi * np.arange(0, 0.1, 0.00005)))
    d = feature_2.append_sampled_dimension(0.00005)
    d.unit = "s"
    d.label = "time"
    feature_2.unit = "mV"
    feature_2.label = "voltage"
    mtag.create_feature(feature_1, nix.LinkType.Untagged)
    mtag.create_feature(feature_2, nix.LinkType.Untagged)


def create_m_tag_3d(f, b):
    data = [da for da in b.data_arrays if da.name == 'lena'][0]

    # some space for three regions-of-interest
    roi_starts = np.zeros((3, 3))
    roi_starts[0, :] = [250, 245, 0]
    roi_starts[1, :] = [250, 315, 0]
    roi_starts[2, :] = [340, 260, 0]

    roi_extents = np.zeros((3, 3))
    roi_extents[0, :] = [30, 45, 3]
    roi_extents[1, :] = [30, 40, 3]
    roi_extents[2, :] = [25, 65, 3]

    # create the positions DataArray
    positions = b.create_data_array("ROI positions", "nix.positions", data=roi_starts)
    positions.append_set_dimension()  # these can be empty
    positions.append_set_dimension()

    # create the extents DataArray
    extents = b.create_data_array("ROI extents", "nix.extents", data=roi_extents)
    extents.append_set_dimension()
    extents.append_set_dimension()

    # create a MultiTag
    multi_tag = b.create_multi_tag("Regions of interest", "nix.roi", positions)
    multi_tag.extents = extents
    multi_tag.references.append(data)


def create_epoch_tag(f, b):
    trace = b.data_arrays["eod"]
    p, f = mlab.psd(trace[:], Fs=1./trace.dimensions[0].sampling_interval, NFFT=4096,
                    noverlap=2048, sides="twosided")
    power = b.create_data_array("power spectrum", "nix.sampled.spectrum.psd", data=p)
    power.label = "power"
    power.unit = "mV^2/cm^2*Hz^-1"
    dim = power.append_sampled_dimension(np.mean(np.diff(f)))
    dim.offset = f[0]
    dim.label = "frequency"
    dim.unit = "Hz"

    tag = b.create_tag("interesting epoch", "nix.epoch", [0.1])
    tag.extent = [0.3]
    tag.definition = "This tag tags a region in the referenced DataArray (EOD). One feature\
    of the referenced epoch, or region, is the power spectrum of the EOD signal in that region."
    tag.references.append(trace)
    tag.create_feature(power, nix.LinkType.Untagged)


def create_point_tag(f, b):
    trace = b.data_arrays["eod"]
    tag = b.create_tag("interesting point", "nix.point", [0.05])
    tag.references.append(trace)


def create_test_file(filename):
    nix_file = nix.File.open(filename, nix.FileMode.Overwrite)

    s = nix_file.create_section('Recording session', 'recording')
    s['date'] = '2015-10-21'
    s['experimenter'] = 'John Doe'

    b = nix_file.create_block("1D data", "nix.recording_session")
    b.definition = "This Block contains 1D datasets and links between them. \
    These datasets show the use of regualrly sampled and irregularly sampled (range) dimensions."
    b.metadata = s
    b2 = nix_file.create_block("Categorical data", "nix.analysis_session")
    b2.definition = "This Block contains categorical data demonstrating the use of SetDimensions."

    create_1d_sampled(nix_file, b)
    create_1d_range(nix_file, b)
    src = create_1d_set(nix_file, b2)
    create_2d_set(nix_file, b2, src)
    create_m_tag(nix_file, b)
    create_epoch_tag(nix_file, b)
    create_point_tag(nix_file, b)

    s2 = nix_file.create_section('Lif recording', 'recording')
    s2['date'] = '2015-10-21'
    s2['experimenter'] = 'John Doe'
    s2['neuron'] = 'Leaky integrate and fire neuron'

    b2 = nix_file.create_block("2D data", "nix.recording_session")
    b2.metadata = s2
    b2.definition = "2-dimensional datasets e.g. for storing multiple time-series or image data."
    create_2d(nix_file, b2)
    create_2d_sample(nix_file, b2)
    b3 = nix_file.create_block("3D data", "nix.image_data")
    b3.metadata = s
    b3.definition = "3-D datasets like RGB image data and links into such datasets."
    create_3d(nix_file, b3)
    create_m_tag_3d(nix_file, b3)
    nix_file.close()

if __name__ == "__main__":
    create_test_file('nix_test.h5')
