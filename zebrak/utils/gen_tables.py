#!/usr/bin/env python3
import getopt
import sys
import collections
import numpy
import math
import matplotlib.pyplot as plt
from pathlib import Path

Table = collections.namedtuple('Table', ['name', 'data'])

wave_tables = []
env_tables = []
render_graphs = False

def plot_raw(path, data):
    fig = plt.figure()
    ax = fig.add_subplot(1,1,1)
    ax.plot(data, linewidth=1)
    ax.axhline(y=0, color='k', linewidth=1)
    ax.set_xlim(0, len(data))
    ax.set_ylim(min(data), max(data))
    fig.savefig(path, bbox_inches='tight')
    plt.close()

def print_table(type, per_row, name, fmt, t):
    on_row = 0
    print('{} {}[{}] = {{'.format(type, name, len(t)))
    for x in t:
        print(('\t' + fmt + ',').format(x), end='')
        on_row = (on_row + 1) % per_row
        if on_row == 0:
            print()
    if on_row != 0:
        print()
    print('};')

def scale(t, min=0, max=255):
    tmin = t.min()
    tmax = t.max()
    return numpy.round((t - tmin) / (tmax - tmin) * (max - min) + min)

def print_uint16_table(name, t):
    print_table('const PROGMEM uint16_t', 4, name, '0x{:04x}', t)


def add_wave(name, data):
    wave_tables.append(Table('waveform_' + name, data))

def add_env(name, data):
    env_tables.append(Table('envelope_' + name, data))

def read_raw(path, signed=True):
    with open(path, 'rb') as f:
        bin = f.read()
    bin_view = memoryview(bin)
    return list(bin_view.cast('b' if signed else 'B'))

def minty_wave_conv(data):
    assert(len(data) == 256)
    return data + [data[0]]

def upsample2(y):
    x1 = numpy.arange(0, len(y))
    x2 = numpy.arange(0, len(y), 0.5)
    return list(numpy.round(numpy.interp(x2, x1, y)).astype(numpy.uint8))

def minty_env_conv(data):
    assert(len(data) == 128)
    print('in', data)
    return upsample2(data) + [data[-1]]


### parse arguments
optlist, args = getopt.getopt(sys.argv[1:], 'g')
for (opt, optval) in optlist:
    if opt == '-g':
        render_graphs = True

### generate waveforms
# waveforms, 8-bit signed

space = numpy.linspace(0, 1.0, 257)
sin_lut = numpy.sin(space * 2 * numpy.pi)

add_wave('sin', scale(sin_lut, min=-127, max=127).astype(numpy.int8))
add_wave('saw', ((0.5 - space) * 255).astype(numpy.int8))
for i in range(1, 15):
    name = 'w{}'.format(i)
    path = Path('waves', name).with_suffix('.raw')
    wave = minty_wave_conv(read_raw(path))
    add_wave(name, wave)

# envelopes, 8-bit unsigned

exp_env = numpy.exp(-1.75 * space)
scaled_exp_env = scale(exp_env)
add_env('exp', scaled_exp_env.astype(numpy.uint8))
add_env('exp2', scale(numpy.exp(-4.21 * space)).astype(numpy.uint8))
add_env('triangle', scale(1 - numpy.abs(2 * space - 1)).astype(numpy.uint8))
add_env('linear', scale(1 - space).astype(numpy.uint8))
add_env('atan', scale(numpy.arctan(-(space * 2 - 1) * 12)).astype(numpy.uint8))
add_env('damped_sine', scale(scaled_exp_env * (1 + numpy.cos(space * 2 * numpy.pi * 3))/2).astype(numpy.uint8))
for i in range(1, 1):
    name = 'e{}'.format(i)
    path = Path('waves', name).with_suffix('.raw')
    env = minty_env_conv(read_raw(path, signed=False))
    add_env(name, env)

### lookup tables, 16-bit unsigned

F_CPU = 16e6
sample_rate = F_CPU / 256 / 4
control_rate = sample_rate / 32

# pitch table, half semi-tone per step
base_note = 69
base_freq = 440
midi_notes = numpy.linspace(0, 128, 257)
freqs = base_freq * 2 ** ((midi_notes - base_note) / 12)
midi_to_inc = 0x10000 * freqs / sample_rate
# filter out all aliasing notes
midi_to_inc = numpy.minimum(midi_to_inc, 0x7fff)
print_uint16_table('note_phase_inc_lut', midi_to_inc.astype(numpy.uint16))

# envelope length (stolen from MI/anushri)
min_time = 4.0 / control_rate
max_time = 4.0 # seconds
gamma = 0.25
min_increment = 0x10000 / (max_time * control_rate)
max_increment = 0x10000 / (min_time * control_rate)
rates = numpy.linspace(numpy.power(max_increment, -gamma),
                       numpy.power(min_increment, -gamma), 257)
values = numpy.power(rates, -1 / gamma)
print_uint16_table('env_phase_inc_lut', values.astype(numpy.uint16))

# filter cutoff to DAC conversion

space = numpy.linspace(0, 4, 257);
values = numpy.power(2, space);
# min value has to be above transistor threshold
print_uint16_table('cutoff_to_dac_lut', scale(values, min=65535*0.7/5, max=65535).astype(numpy.uint16))

### print/draw wave/env tables

for (name, table) in wave_tables:
    print_table('const PROGMEM int8_t', 8, name, '{}', table)
    if render_graphs:
        png_path = Path('waves', name).with_suffix('.png')
        plot_raw(png_path, table)

print_table('const int8_t *const PROGMEM', 1, 'wav_tables', '{}', [name for (name, _) in wave_tables])

for (name, table) in env_tables:
    print_table('const PROGMEM uint8_t', 8, name, '{}', table)
    if render_graphs:
        png_path = Path('waves', name).with_suffix('.png')
        plot_raw(png_path, table)

print_table('const uint8_t *const PROGMEM', 1, 'env_tables', '{}', [name for (name, _) in env_tables])


