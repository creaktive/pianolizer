//! Benchmark — ported from cpp/benchmark.cpp
//!
//! Usage: `cargo run --release --bin benchmark [sampleRate]`

use pianolizer::{PianoTuning, SlidingDFTNoMA};
use std::env;
use std::time::Instant;

const ABS_ERROR: f64 = 1e-4;

fn oscillator(s: u32) -> f32 {
    ((s % 100) as f32 / 50.0) - 1.0 // sawtooth wave, 441Hz at 44100Hz sample rate
}

fn main() {
    let args: Vec<String> = env::args().collect();
    let mut sample_rate: u32 = 44100;

    if args.len() == 2 {
        sample_rate = match args[1].parse::<u32>() {
            Ok(sr) => sr,
            Err(_) => {
                eprintln!("sampleRate must be between 8000 and 200000 Hz");
                std::process::exit(1);
            }
        };
        if sample_rate < 8000 || sample_rate > 200000 {
            eprintln!("sampleRate must be between 8000 and 200000 Hz");
            std::process::exit(1);
        }
    }

    eprintln!("sampleRate: {}", sample_rate);

    let tuning = PianoTuning::with_defaults(sample_rate);
    let mut sdft = SlidingDFTNoMA::new(&tuning); // no moving average for benchmark parity

    let buffer_size: usize = 128;
    let mut input = [0.0f32; 128];
    let mut output = vec![0.0f32; 61];

    let total_iterations = (buffer_size * 10_000) as u32;
    let start = Instant::now();

    for i in 0..total_iterations {
        let j = (i % buffer_size as u32) as usize;
        input[j] = oscillator(i);
        if j == buffer_size - 1 {
            output = sdft.process(&input);
        }
    }

    let elapsed = start.elapsed();
    let samples_per_sec = total_iterations as f64 / elapsed.as_secs_f64();
    println!("benchmark: {} samples per second", (samples_per_sec.round() as u64));

    // Validation against known output values at 44100Hz
    if sample_rate == 44100 {
        let test_values: &[(usize, f32)] = &[
            (21, 0.000041),
            (33, 0.605242),
            (45, 0.152685),
            (52, 0.069327),
            (57, 0.036673),
        ];

        let mut errors = 0;
        for &(key, expected) in test_values {
            let diff = (output[key] - expected).abs();
            if diff > ABS_ERROR as f32 {
                eprintln!(
                    "output for key #{} is {}; expected {}",
                    key, output[key], expected
                );
                errors += 1;
            }
        }

        if errors > 0 {
            std::process::exit(1);
        }
    }
}
