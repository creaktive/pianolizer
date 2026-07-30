//! Benchmark: SlidingDFT throughput on synthetic waveforms.
//!
//! Run with: `cargo run --release --bin benchmark`

use pianolizer::{PianoTuning, SlidingDFT};

fn generate_sine_wave(frequency: f64, sample_rate: u32, samples_per_buffer: usize) -> Vec<f32> {
    let mut buffer = Vec::with_capacity(samples_per_buffer);
    for i in 0..samples_per_buffer {
        let t = i as f64 / sample_rate as f64;
        buffer.push((frequency * 2.0 * std::f64::consts::PI * t).sin() as f32);
    }
    buffer
}

fn generate_sawtooth_wave(frequency: f64, sample_rate: u32, samples_per_buffer: usize) -> Vec<f32> {
    let mut buffer = Vec::with_capacity(samples_per_buffer);
    for i in 0..samples_per_buffer {
        let t = i as f64 / sample_rate as f64;
        let phase = (frequency * t).fract();
        buffer.push((2.0 * phase - 1.0) as f32);
    }
    buffer
}

fn generate_square_wave(frequency: f64, sample_rate: u32, samples_per_buffer: usize) -> Vec<f32> {
    let mut buffer = Vec::with_capacity(samples_per_buffer);
    for i in 0..samples_per_buffer {
        let t = i as f64 / sample_rate as f64;
        buffer.push(if (frequency * t).fract() < 0.5 { 1.0 } else { -1.0 });
    }
    buffer
}

fn run_benchmark(
    name: &str,
    frequency: f64,
    sample_rate: u32,
    samples_per_buffer: usize,
    wave_fn: fn(f64, u32, usize) -> Vec<f32>,
    iterations: usize,
) {
    let tuning = PianoTuning::with_defaults(sample_rate);
    let mut sdft = SlidingDFT::new(&tuning, -1.0); // fast moving average

    let samples = wave_fn(frequency, sample_rate, samples_per_buffer);
    let start = std::time::Instant::now();

    for _ in 0..iterations {
        let levels = sdft.process(&samples, 0.05);
        let _first_level = levels[0]; // prevent optimization
    }

    let elapsed = start.elapsed();
    println!(
        "{:40} | {:8.2} ms | {:.1} kHz/s",
        name,
        elapsed.as_secs_f64() * 1000.0,
        (iterations as f64 * samples_per_buffer as f64) / elapsed.as_secs_f64() / 1000.0
    );
}

fn main() {
    println!("Pianolizer Benchmark — SlidingDFT with Fast Moving Average");
    println!("==========================================================\n");

    let sample_rate = 44100;
    let samples_per_buffer = 128;
    let iterations = 10_000;

    run_benchmark(
        "C2 (65.4 Hz) sine wave",
        65.4,
        sample_rate,
        samples_per_buffer,
        generate_sine_wave,
        iterations,
    );
    run_benchmark(
        "A4 (440 Hz) sine wave",
        440.0,
        sample_rate,
        samples_per_buffer,
        generate_sine_wave,
        iterations,
    );
    run_benchmark(
        "C7 (2093 Hz) sawtooth wave",
        2093.0,
        sample_rate,
        samples_per_buffer,
        generate_sawtooth_wave,
        iterations,
    );
    run_benchmark(
        "E4 (329.6 Hz) square wave",
        329.6,
        sample_rate,
        samples_per_buffer,
        generate_square_wave,
        iterations,
    );

    println!("\nDone.");
}
