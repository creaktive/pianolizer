//! Integration tests — ported from cpp/test.cpp

use pianolizer::{PianoTuning, RingBuffer, Tuning};
#[cfg(feature = "default-moving-average")]
use pianolizer::{FastMovingAverage, HeavyMovingAverage, MovingAverage, SlidingDFT};
use std::f64::consts::PI;

const ABS_ERROR: f64 = 1e-4;
const SAMPLE_RATE: u32 = 44100;

fn oscillator(s: u32, wave_type: u8) -> f64 {
    match wave_type {
        0 => (PI / 50.0 * s as f64).sin(), // SINE
        1 => ((s % 100) as f64 / 50.0) - 1.0, // SAWTOOTH
        2 => if (s % 100) < 50 { 1.0 } else { -1.0 }, // SQUARE
        _ => panic!("unknown oscillator type"),
    }
}

#[test]
fn ring_buffer_tiny() {
    let mut rb = RingBuffer::new(16);

    assert_eq!(rb.size(), 16, "RingBuffer size correct");
    assert_eq!(rb.read(0), 0.0, "initialized to zeroes");

    // write one single element
    rb.write(1.0);
    assert_eq!(rb.read(0), 1.0, "insertion succeeded");
    assert_eq!(rb.read(1), 0.0, "boundary shifted");

    // write 9 more elements
    for i in 2..10 {
        rb.write(i as f32);
    }

    // read in sequence
    for i in 0..10u32 {
        assert_eq!(rb.read(9 - i), i as f32, "sequence value matches");
    }

    // overflow
    for i in 10..20 {
        rb.write(i as f32);
    }

    assert_eq!(rb.read(0), 19.0, "head as expected");
    assert_eq!(rb.read(15), 4.0, "tail as expected");

    // reading beyond the capacity wraps around
    assert_eq!(rb.read(16), 19.0, "wrap back to 0");
    assert_eq!(rb.read(17), 18.0, "wrap back to 1");
}

#[test]
fn dft_oscillators() {
    // SINE
    test_dft(0, 0.99999994039535522, 0.70710676908493042, -3.0103001594543457);

    // SAWTOOTH
    test_dft(1, 0.60800313949584961, 0.57740825414657593, -6.931281566619873);

    // SQUARE
    test_dft(2, 0.81083619594573975, 0.99999988079071045, -0.91066890954971313);
}

fn test_dft(wave_type: u8, expected_nas: f64, expected_rms: f64, expected_log: f64) {
    let n = 1700u32;
    // k=17 for ~441Hz at 44100Hz with N≈1700
    let mut bin = pianolizer::DFTBin::new(17, n);
    let mut rb = RingBuffer::new(n);

    assert_eq!(rb.size(), 2048, "RingBuffer size correct (next power of two)");

    for i in 0..2000u32 {
        let current_sample = oscillator(i, wave_type);
        rb.write(current_sample as f32);
        let previous_sample = rb.read(n) as f64;
        bin.update(previous_sample, current_sample);
    }

    let prefix = format!("oscillator #{}; ", wave_type);
    assert!(
        (expected_nas - bin.normalized_amplitude_spectrum()).abs() < ABS_ERROR,
        "{}normalized amplitude spectrum",
        prefix
    );
    assert!(
        (expected_rms - bin.rms()).abs() < ABS_ERROR,
        "{}RMS",
        prefix
    );
    assert!(
        (expected_log - bin.logarithmic_unit_decibels()).abs() < ABS_ERROR,
        "{}log dB",
        prefix
    );
}

#[cfg(feature = "default-moving-average")]
#[test]
fn moving_average_fast_and_heavy() {
    let mut fma = FastMovingAverage::new(2, SAMPLE_RATE);
    fma.set_average_window_in_seconds(0.01);

    let mut hma = HeavyMovingAverage::new(2, SAMPLE_RATE, 500);
    hma.set_average_window_in_seconds(0.01);

    for i in 0..500u32 {
        let sample = [oscillator(i, 0) as f32, oscillator(i, 1) as f32]; // sine, sawtooth
        fma.update(&sample);
        hma.update(&sample);
    }

    assert!(
        (fma.read(0) - (-0.024506002326671227)).abs() < ABS_ERROR as f32,
        "sine fast average"
    );
    assert!(
        (fma.read(1) - 0.01886483060529713).abs() < ABS_ERROR as f32,
        "sawtooth fast average"
    );

    assert!(
        (hma.read(0) - (-0.06714661267338967)).abs() < ABS_ERROR as f32,
        "sine heavy average"
    );
    assert!(
        (hma.read(1) - 0.04485260926676986).abs() < ABS_ERROR as f32,
        "sawtooth heavy average"
    );
}

#[test]
fn piano_tuning_dft_values() {
    let pt = PianoTuning::with_defaults(SAMPLE_RATE);
    let m = pt.mapping();

    assert_eq!(m.len(), 61, "mapping size");

    assert_eq!(m[0].k, 17, "C2 k");
    assert_eq!(m[0].n, 11462, "C2 N");

    assert_eq!(m[33].k, 17, "A4 k");
    assert_eq!(m[33].n, 1704, "A4 N");

    assert_eq!(m[60].k, 17, "C7 k");
    assert_eq!(m[60].n, 358, "C7 N");
}

#[cfg(feature = "default-moving-average")]
#[test]
fn sliding_dft_integration_benchmark() {
    let tuning = PianoTuning::with_defaults(SAMPLE_RATE);
    let mut sdft = SlidingDFT::new(&tuning, -1.0); // fast moving average

    let buffer_size: usize = 128;
    let mut input = [0.0f32; 128];
    let mut output = vec![0.0f32; 61];

    let total_iterations = (buffer_size * 10_000) as u32;
    let start = std::time::Instant::now();

    for i in 0..total_iterations {
        let j = (i % buffer_size as u32) as usize;
        input[j] = oscillator(i, 1) as f32; // sawtooth
        if j == buffer_size - 1 {
            output = sdft.process(&input, 0.05);
        }
    }

    let elapsed = start.elapsed();
    let samples_per_sec = total_iterations as f64 / elapsed.as_secs_f64();
    eprintln!(
        "# benchmark: {} samples per second",
        (samples_per_sec.round() as u64)
    );

    let test_values: &[(usize, f32)] = &[
        (21, 0.0000176868834387),
        (33, 0.6048020720481872),
        (45, 0.1517260670661926),
        (52, 0.0671683400869369),
        (57, 0.0384454987943172),
    ];

    for &(key, expected) in test_values {
        assert!(
            (output[key] - expected).abs() < ABS_ERROR as f32,
            "sawtooth, key #{}",
            key
        );
    }
}
