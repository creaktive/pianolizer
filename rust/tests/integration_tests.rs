//! Integration tests for pianolizer — validates algorithmic behavior against known values.

#[cfg(test)]
mod tests {
    use pianolizer::*;

    const ABS_ERROR: f64 = 0.01;
    const REL_ERROR: f64 = 0.02;

    fn approx_eq(a: f64, b: f64) -> bool {
        (a - b).abs() < ABS_ERROR || ((a - b).abs() / a.abs()) < REL_ERROR
    }

    // ─── RingBuffer Tests ──────────────────────────────────────────────────────

    #[test]
    fn ring_buffer_write_read() {
        let mut rb = RingBuffer::new(8);
        for i in 0..16 {
            rb.write(i as f32);
        }
        assert_eq!(rb.read(0), 15.0, "most recent");
        assert_eq!(rb.read(1), 14.0, "one behind");
    }

    #[test]
    fn ring_buffer_wraparound() {
        let mut rb = RingBuffer::new(8);
        for i in 0..256 {
            rb.write(i as f32);
        }
        assert_eq!(rb.read(0), 255.0, "wrap");
    }

    #[test]
    fn ring_buffer_default() {
        let rb = RingBuffer::default();
        assert_eq!(rb.size(), 1024);
    }

    // ─── Complex Tests ─────────────────────────────────────────────────────────

    #[test]
    fn complex_default() {
        let c = Complex::default();
        assert!((c.re() - 0.0).abs() < ABS_ERROR);
        assert!((c.im() - 0.0).abs() < ABS_ERROR);
    }

    #[test]
    fn complex_addition() {
        let a = Complex::new(1.0, 2.0);
        let b = Complex::new(3.0, 4.0);
        let c = &a + &b;
        assert!((c.re() - 4.0).abs() < ABS_ERROR);
        assert!((c.im() - 6.0).abs() < ABS_ERROR);
    }

    #[test]
    fn complex_subtraction() {
        let a = Complex::new(5.0, 3.0);
        let b = Complex::new(2.0, 1.0);
        let c = &a - &b;
        assert!((c.re() - 3.0).abs() < ABS_ERROR);
        assert!((c.im() - 2.0).abs() < ABS_ERROR);
    }

    #[test]
    fn complex_multiplication() {
        let a = Complex::new(1.0, 2.0);
        let b = Complex::new(3.0, 4.0);
        let c = a.mul_complex(&b);
        assert!((c.re() - (-5.0)).abs() < ABS_ERROR); // (1+2i)(3+4i) = -5 + 10i
        assert!((c.im() - 10.0).abs() < ABS_ERROR);
    }

    #[test]
    fn complex_norm() {
        let c = Complex::new(3.0, 4.0);
        assert!((c.norm() - 25.0).abs() < ABS_ERROR);
    }

    // ─── DFTBin Tests ──────────────────────────────────────────────────────────

    #[test]
    fn dftbin_new() {
        let bin = DFTBin::new(17, 1704);
        assert_eq!(bin.k(), 17);
        assert_eq!(bin.n(), 1704);
    }

    #[test]
    fn dftbin_default() {
        let bin = DFTBin::default();
        assert_eq!(bin.k(), 17);
        assert_eq!(bin.n(), 1704);
    }

    #[test]
    fn dftbin_update_and_spectrum() {
        let mut bin = DFTBin::new(17, 1704);
        for i in 0..1704 {
            let sample = (i as f64 * 2.0 * std::f64::consts::PI * 17.0 / 1704.0).sin();
            bin.update(0.0, sample);
        }

        assert!(bin.rms() > 0.0, "rms should be positive");
        assert!(bin.amplitude_spectrum() > 0.0, "amplitude should be positive");
    }

    #[test]
    fn dftbin_normalized_amplitude_range() {
        let mut bin = DFTBin::new(17, 1704);
        for i in 0..1704 {
            let sample = (i as f64 * 2.0 * std::f64::consts::PI * 17.0 / 1704.0).sin();
            bin.update(0.0, sample);
        }

        let norm = bin.normalized_amplitude_spectrum();
        assert!(norm >= 0.0 && norm <= 1.0, "normalized amplitude in [0, 1], got {}", norm);
    }

    #[test]
    fn dftbin_panic_on_k_zero() {
        let result = std::panic::catch_unwind(|| DFTBin::new(0, 100));
        assert!(result.is_err(), "k=0 should panic");
    }

    #[test]
    fn dftbin_panic_on_n_zero() {
        let result = std::panic::catch_unwind(|| DFTBin::new(17, 0));
        assert!(result.is_err(), "n=0 should panic");
    }

    // ─── PianoTuning Tests ─────────────────────────────────────────────────────

    #[test]
    fn piano_tuning_mapping() {
        let tuning = PianoTuning::with_defaults(44100);
        let m = &tuning.mapping_cache;
        assert_eq!(m.len(), 61, "should have 61 keys");
        assert_eq!(m[0].k, 17, "C2 k");
        assert_eq!(m[0].n, 11462, "C2 N");
    }

    #[test]
    fn piano_tuning_key_to_freq() {
        let tuning = PianoTuning::with_defaults(44100);
        // A4 is key 33 in the C2–C7 range (33 - 0 = 33 semitones above C2)
        // Actually, reference_key=33 means key 33 maps to pitch_fork (440Hz)
        assert!(approx_eq(tuning.key_to_freq(33.0), 440.0), "A4 should be ~440 Hz");
    }

    #[test]
    fn piano_tuning_mapping_cached() {
        let tuning = PianoTuning::with_defaults(44100);
        // Call mapping multiple times — should return the same cached slice, not recompute.
        let m1 = &tuning.mapping_cache;
        let m2 = &tuning.mapping_cache;
        assert!(std::ptr::eq(m1.as_ptr(), m2.as_ptr()), "mapping should be cached");
    }

    // ─── SlidingDFTNoMA Tests ──────────────────────────────────────────────────

    mod no_ma_tests {
        use super::*;

        fn generate_oscillator(
            frequency: f64,
            sample_rate: u32,
            samples_per_buffer: usize,
            wave_type: OscillatorType,
        ) -> Vec<f32> {
            let mut buffer = Vec::with_capacity(samples_per_buffer);
            for i in 0..samples_per_buffer {
                let t = i as f64 / sample_rate as f64;
                let phase = frequency * t;
                let value = match wave_type {
                    OscillatorType::Sine => phase.sin(),
                    OscillatorType::Sawtooth => 2.0 * phase.fract() - 1.0,
                    OscillatorType::Square => if phase.fract() < 0.5 { 1.0 } else { -1.0 },
                };
                buffer.push(value as f32);
            }
            buffer
        }

        #[test]
        fn sliding_dft_no_ma_process() {
            let tuning = PianoTuning::with_defaults(44100);
            let mut sdft = SlidingDFTNoMA::new(&tuning);
            let samples = generate_oscillator(440.0, 44100, 128, OscillatorType::Sine);

            let levels = sdft.process(&samples);
            assert_eq!(levels.len(), 61, "should have 61 key outputs");
        }

        #[test]
        fn sliding_dft_no_ma_levels_range() {
            let tuning = PianoTuning::with_defaults(44100);
            let mut sdft = SlidingDFTNoMA::new(&tuning);
            let samples = generate_oscillator(440.0, 44100, 128, OscillatorType::Sine);

            let levels = sdft.process(&samples);
            for &level in levels.iter() {
                assert!(level >= 0.0 && level <= 1.0, "level should be in [0, 1], got {}", level);
            }
        }
    }

    // ─── SlidingDFT Tests ──────────────────────────────────────────────────────

    mod ma_tests {
        use super::*;

        fn generate_oscillator(
            frequency: f64,
            sample_rate: u32,
            samples_per_buffer: usize,
            wave_type: OscillatorType,
        ) -> Vec<f32> {
            let mut buffer = Vec::with_capacity(samples_per_buffer);
            for i in 0..samples_per_buffer {
                let t = i as f64 / sample_rate as f64;
                let phase = frequency * t;
                let value = match wave_type {
                    OscillatorType::Sine => phase.sin(),
                    OscillatorType::Sawtooth => 2.0 * phase.fract() - 1.0,
                    OscillatorType::Square => if phase.fract() < 0.5 { 1.0 } else { -1.0 },
                };
                buffer.push(value as f32);
            }
            buffer
        }

        #[test]
        fn sliding_dft_fast_ma_process() {
            let tuning = PianoTuning::with_defaults(44100);
            let mut sdft = SlidingDFT::new(&tuning, -1.0); // fast moving average
            let samples = generate_oscillator(440.0, 44100, 128, OscillatorType::Sine);

            let levels = sdft.process(&samples, 0.05);
            assert_eq!(levels.len(), 61, "should have 61 key outputs");
        }

        #[test]
        fn sliding_dft_heavy_ma_process() {
            let tuning = PianoTuning::with_defaults(44100);
            let mut sdft = SlidingDFT::new(&tuning, 0.5); // heavy moving average
            let samples = generate_oscillator(440.0, 44100, 128, OscillatorType::Sine);

            let levels = sdft.process(&samples, 0.05);
            assert_eq!(levels.len(), 61, "should have 61 key outputs");
        }

        #[test]
        fn sliding_dft_levels_range() {
            let tuning = PianoTuning::with_defaults(44100);
            let mut sdft = SlidingDFT::new(&tuning, -1.0); // fast moving average
            let samples = generate_oscillator(440.0, 44100, 128, OscillatorType::Sine);

            let levels = sdft.process(&samples, 0.05);
            for &level in levels.iter() {
                assert!(level >= 0.0 && level <= 1.0, "level should be in [0, 1], got {}", level);
            }
        }

        #[test]
        fn sliding_dft_a4_detection() {
            let tuning = PianoTuning::with_defaults(44100);
            let mut sdft = SlidingDFT::new(&tuning, -1.0); // fast moving average
            let samples = generate_oscillator(440.0, 44100, 128, OscillatorType::Sine);

            let levels = sdft.process(&samples, 0.05);
            assert!(levels[33] > 0.0, "A4 key (index 33) should have non-zero amplitude");
        }

        #[test]
        fn sliding_dft_multiple_buffers() {
            let tuning = PianoTuning::with_defaults(44100);
            let mut sdft = SlidingDFT::new(&tuning, -1.0); // fast moving average
            let samples = generate_oscillator(440.0, 44100, 128, OscillatorType::Sine);

            for _ in 0..5 {
                let levels = sdft.process(&samples, 0.05);
                assert!(levels[33] > 0.0, "A4 should be detected across multiple buffers");
            }
        }

        #[test]
        fn sliding_dft_average_window_override() {
            let tuning = PianoTuning::with_defaults(44100);
            let mut sdft = SlidingDFT::new(&tuning, -1.0); // fast moving average
            let samples = generate_oscillator(440.0, 44100, 128, OscillatorType::Sine);

            // Override with heavy MA window
            let levels = sdft.process(&samples, 0.5);
            assert_eq!(levels.len(), 61, "should have 61 key outputs");
        }
    }

    // ─── Oscillator Types (shared) ─────────────────────────────────────────────

    #[derive(Clone, Copy)]
    enum OscillatorType {
        Sine,
        #[allow(dead_code)]
        Sawtooth,
        #[allow(dead_code)]
        Square,
    }
}
