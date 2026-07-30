//! Pianolizer — Real-time music spectral analysis using the Sliding Discrete Fourier Transform (SDFT) algorithm.
//!
//! Detects 61 piano keys (C2–C7) from audio input and outputs normalized amplitude values in `[0.0, 1.0]`.
//!
//! # Example
//! ```ignore
//! use pianolizer::{PianoTuning, SlidingDFT};
//!
//! let tuning = PianoTuning::with_defaults(44100);
//! let mut sdft = SlidingDFT::new(&tuning, -1.0); // fast moving average
//!
//! let samples: Vec<f32> = vec![0.0; 128];
//! let levels = sdft.process(&samples, 0.05);
//! ```

// ─── Ring Buffer ──────────────────────────────────────────────────────────────
// Power-of-two sized circular buffer using bitwise masking for fast modulo.

pub struct RingBuffer {
    mask: usize,
    index: usize,
    buffer: Vec<f32>,
}

impl RingBuffer {
    /// Creates a new RingBuffer, sizing internally to the next power of two.
    pub fn new(requested_size: u32) -> Self {
        let size = if requested_size == 0 {
            1
        } else if requested_size & (requested_size - 1) == 0 {
            // already a power of two
            requested_size
        } else {
            let bits = (32 - requested_size.leading_zeros()) as u32;
            1u32 << bits
        };

        Self {
            mask: (size - 1) as usize,
            index: 0,
            buffer: vec![0.0; size as usize],
        }
    }

    /// Returns the actual allocated size of the buffer.
    pub fn size(&self) -> u32 {
        self.buffer.len() as u32
    }

    /// Write a value into the buffer, advancing the write pointer.
    pub fn write(&mut self, value: f32) {
        let idx = self.index & self.mask;
        self.buffer[idx] = value;
        self.index += 1;
    }

    /// Read a value at a given position behind the current write head.
    /// `position=0` returns the most recently written value.
    pub fn read(&self, position: u32) -> f32 {
        let abs_pos = self.index.wrapping_sub((position + 1) as usize) & self.mask;
        self.buffer[abs_pos]
    }
}

impl Default for RingBuffer {
    /// Creates a ring buffer with 1024 capacity.
    fn default() -> Self {
        Self::new(1024)
    }
}

// ─── Complex Number ───────────────────────────────────────────────────────────
// In-place arithmetic — no allocations per operation.

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Complex {
    re: f64,
    im: f64,
}

impl Complex {
    pub fn new(re: f64, im: f64) -> Self {
        Self { re, im }
    }

    /// In-place addition.
    pub fn add_assign(&mut self, other: &Complex) {
        self.re += other.re;
        self.im += other.im;
    }

    /// In-place subtraction.
    pub fn sub_assign(&mut self, other: &Complex) {
        self.re -= other.re;
        self.im -= other.im;
    }

    /// Multiply two complex numbers (returns new Complex).
    pub fn mul_complex(self, other: &Complex) -> Complex {
        Complex::new(
            self.re * other.re - self.im * other.im,
            self.re * other.im + self.im * other.re,
        )
    }

    /// Returns the real part.
    pub fn re(&self) -> f64 {
        self.re
    }

    /// Returns the imaginary part.
    pub fn im(&self) -> f64 {
        self.im
    }

    /// Squared magnitude (norm).
    pub fn norm(&self) -> f64 {
        self.re * self.re + self.im * self.im
    }
}

impl std::ops::Add for &Complex {
    type Output = Complex;
    fn add(self, other: Self) -> Complex {
        Complex::new(self.re + other.re, self.im + other.im)
    }
}

impl std::ops::Sub for &Complex {
    type Output = Complex;
    fn sub(self, other: Self) -> Complex {
        Complex::new(self.re - other.re, self.im - other.im)
    }
}

// ─── DFT Bin ──────────────────────────────────────────────────────────────────
// Discrete Fourier Transform for a single frequency bin.

pub struct DFTBin {
    total_power: f64,
    r: f64,
    coeff: Complex,
    dft: Complex,
    k: u32,
    n: u32,
}

impl DFTBin {
    /// Creates a new DFTBin.
    /// `k` — frequency divided by bandwidth.
    /// `n` — sample rate divided by bandwidth.
    pub fn new(k: u32, n: u32) -> Self {
        if k == 0 {
            panic!("k=0 (DC) not implemented");
        }
        if n == 0 {
            panic!("n=0 is not supported");
        }

        let q = 2.0 * std::f64::consts::PI * k as f64 / n as f64;
        let r = 2.0 / n as f64;
        let coeff = Complex::new(q.cos(), -q.sin());

        Self {
            total_power: 0.0,
            r,
            coeff,
            dft: Complex::default(),
            k,
            n,
        }
    }

    /// Returns the frequency bin index k.
    pub fn k(&self) -> u32 {
        self.k
    }

    /// Returns the sample count N.
    pub fn n(&self) -> u32 {
        self.n
    }

    /// Update the DFT bin with a new sample pair.
    pub fn update(&mut self, previous_sample: f64, current_sample: f64) {
        self.total_power += current_sample * current_sample;
        self.total_power -= previous_sample * previous_sample;

        let delta_re = current_sample - previous_sample;
        // dft = coeff * (dft + Complex(delta_re, 0))
        let temp = Complex::new(self.dft.re + delta_re, self.dft.im);
        self.dft = self.coeff.mul_complex(&temp);
    }

    /// Root Mean Square.
    pub fn rms(&self) -> f64 {
        (self.total_power / self.n() as f64).sqrt()
    }

    /// Amplitude spectrum in volts RMS.
    pub fn amplitude_spectrum(&self) -> f64 {
        std::f64::consts::SQRT_2 * self.dft.norm().sqrt() / self.n() as f64
    }

    /// Normalized amplitude spectrum, always in `[0.0, 1.0]`.
    pub fn normalized_amplitude_spectrum(&self) -> f64 {
        if self.total_power > 0.0 {
            self.r * self.dft.norm() / self.total_power
        } else {
            0.0
        }
    }


}

impl Default for DFTBin {
    /// Creates a default DFTBin for A4 (k=17, n=1704 at 44100Hz).
    fn default() -> Self {
        Self::new(17, 1704)
    }
}

// ─── Moving Average Trait ─────────────────────────────────────────────────────

pub trait MovingAverage {
    fn update(&mut self, levels: &[f32]);
    fn read(&self, n: usize) -> f32;
    fn average_window_in_seconds(&self) -> f32;
    fn set_average_window_in_seconds(&mut self, value: f32);
}

// ─── Fast Moving Average (cfg-gated) ──────────────────────────────────────────
// Exponential approximation of the moving average — minimal memory.

pub struct FastMovingAverage {
    channels: usize,
    sample_rate: u32,
    average_window: i32,
    target_average_window: i32,
    sum: Vec<f32>,
}

impl FastMovingAverage {
    pub fn new(channels: u32, sample_rate: u32) -> Self {
        Self {
            channels: channels as usize,
            sample_rate,
            average_window: -1,
            target_average_window: 0,
            sum: vec![0.0; channels as usize],
        }
    }

    fn update_average_window(&mut self) {
        if self.target_average_window > self.average_window {
            self.average_window += 1;
        } else if self.target_average_window < self.average_window {
            self.average_window -= 1;
        }
    }
}

impl MovingAverage for FastMovingAverage {
    fn update(&mut self, levels: &[f32]) {
        self.update_average_window();
        for n in 0..self.channels {
            let current_sum = self.sum[n];
            self.sum[n] = if self.average_window > 0 {
                current_sum + levels[n] - current_sum / self.average_window as f32
            } else {
                levels[n]
            };
        }
    }

    fn read(&self, n: usize) -> f32 {
        self.sum[n] / self.average_window.max(1) as f32
    }

    fn average_window_in_seconds(&self) -> f32 {
        self.average_window as f32 / self.sample_rate as f32
    }

    fn set_average_window_in_seconds(&mut self, value: f32) {
        self.target_average_window = (value * self.sample_rate as f32).round() as i32;
        if self.average_window == -1 {
            self.average_window = self.target_average_window;
        }
    }
}

// ─── Heavy Moving Average (cfg-gated) ─────────────────────────────────────────
// Proper implementation using RingBuffers per channel — more memory, exact average.

pub struct HeavyMovingAverage {
    channels: usize,
    sample_rate: u32,
    average_window: i32,
    target_average_window: i32,
    sum: Vec<f32>,
    history: Vec<RingBuffer>,
}

impl HeavyMovingAverage {
    pub fn new(channels: u32, sample_rate: u32, max_window: u32) -> Self {
        let channels = channels as usize;
        let max_window = if max_window > 0 { max_window } else { sample_rate };
        let mut history = Vec::with_capacity(channels);
        for _ in 0..channels {
            history.push(RingBuffer::new(max_window));
        }

        Self {
            channels,
            sample_rate,
            average_window: -1,
            target_average_window: 0,
            sum: vec![0.0; channels],
            history,
        }
    }

    fn update_average_window(&mut self) {
        if self.target_average_window > self.average_window {
            self.average_window += 1;
        } else if self.target_average_window < self.average_window {
            self.average_window -= 1;
        }
    }
}

impl MovingAverage for HeavyMovingAverage {
    fn update(&mut self, levels: &[f32]) {
        for n in 0..self.channels {
            let value = levels[n];
            self.history[n].write(value);
            self.sum[n] += value;

            if self.target_average_window == self.average_window {
                self.sum[n] -= self.history[n].read(self.average_window as u32);
            } else if self.target_average_window < self.average_window {
                self.sum[n] -= self.history[n].read(self.average_window as u32);
                self.sum[n] -= self.history[n].read(self.average_window as u32 - 1);
            }
        }
        self.update_average_window();
    }

    fn read(&self, n: usize) -> f32 {
        self.sum[n] / self.average_window.max(1) as f32
    }

    fn average_window_in_seconds(&self) -> f32 {
        self.average_window as f32 / self.sample_rate as f32
    }

    fn set_average_window_in_seconds(&mut self, value: f32) {
        self.target_average_window = (value * self.sample_rate as f32).round() as i32;
        if self.average_window == -1 {
            self.average_window = self.target_average_window;
        }
    }
}

// ─── Tuning Trait ─────────────────────────────────────────────────────────────

pub struct TuningValues {
    pub k: u32,
    pub n: u32,
}

// ─── Piano Tuning ─────────────────────────────────────────────────────────────
// Maps 61 piano keys (C2–C7) to DFT parameters based on equal temperament.

pub struct PianoTuning {
    pub sample_rate: u32,
    pub bands: usize,
    reference_key: i32,
    pitch_fork: f64,
    tolerance: f64,
    pub mapping_cache: Vec<TuningValues>,
}

impl PianoTuning {
    /// Creates a new PianoTuning.
    pub fn new(sample_rate: u32, keys_num: u32, reference_key: u32, pitch_fork: f64, tolerance: f64) -> Self {
        let mut tuning = Self {
            sample_rate,
            bands: keys_num as usize,
            reference_key: reference_key as i32,
            pitch_fork,
            tolerance,
            mapping_cache: Vec::new(),
        };
        // Compute and cache the mapping eagerly so it's done once.
        tuning.mapping_cache = tuning.compute_mapping();
        tuning
    }

    /// Creates a default PianoTuning for the given sample rate (61 keys, A4=440Hz).
    pub fn with_defaults(sample_rate: u32) -> Self {
        Self::new(sample_rate, 61, 33, 440.0, 1.0)
    }

    /// Convert a piano key index to its fundamental frequency in Hz.
    pub fn key_to_freq(&self, key: f64) -> f64 {
        self.pitch_fork * 2.0f64.powf((key - self.reference_key as f64) / 12.0)
    }

    fn compute_mapping(&self) -> Vec<TuningValues> {
        let mut output = Vec::with_capacity(self.bands);
        for key in 0..self.bands {
            let frequency = self.key_to_freq(key as f64);
            let bandwidth = 2.0 * (self.key_to_freq(key as f64 + 0.5 * self.tolerance) - frequency);
            output.push(self.frequency_and_bandwidth_to_k_and_n(frequency, bandwidth));
        }
        output
    }
}

impl PianoTuning {
    fn frequency_and_bandwidth_to_k_and_n(&self, frequency: f64, bandwidth: f64) -> TuningValues {
        let mut n = (self.sample_rate as f64 / bandwidth).floor() as u32;
        let k = (frequency / bandwidth).floor() as u32;

        // Find such N that (sampleRate * (k / N)) is closest to frequency.
        let mut delta = (self.sample_rate as f64 * (k as f64 / n as f64) - frequency).abs();
        for i in (1..n).rev() {
            let tmp_delta = (self.sample_rate as f64 * (k as f64 / i as f64) - frequency).abs();
            if tmp_delta < delta {
                delta = tmp_delta;
                n = i;
            } else {
                return TuningValues { k, n };
            }
        }
        TuningValues { k, n }
    }
}

// ─── Sliding DFT — No Moving Average (compiled with --no-default-features) ────

pub struct SlidingDFTNoMA {
    bins: Vec<DFTBin>,
    levels: Vec<f32>,
    ring_buffer: RingBuffer,
}

impl SlidingDFTNoMA {
    /// Creates a new SlidingDFT without moving average.
    pub fn new(tuning: &PianoTuning) -> Self {
        let bands = tuning.bands;
        let mut bins = Vec::with_capacity(bands);
        let mapping = &tuning.mapping_cache;
        let mut max_n = 0u32;

        for band in mapping.iter() {
            bins.push(DFTBin::new(band.k, band.n));
            if band.n > max_n {
                max_n = band.n;
            }
        }

        Self {
            bins,
            levels: vec![0.0; bands],
            ring_buffer: RingBuffer::new(max_n),
        }
    }

    /// Process a batch of samples. Returns a reference to the latest amplitude values in `[0.0, 1.0]`.
    pub fn process(&mut self, samples: &[f32]) -> &[f32] {
        let bins_num = self.bins.len();

        for i in 0..samples.len() {
            let current_sample = samples[i];
            self.ring_buffer.write(current_sample);

            for band in 0..bins_num {
                let previous_sample = self.ring_buffer.read(self.bins[band].n());
                self.bins[band].update(previous_sample as f64, current_sample as f64);
                self.levels[band] = self.bins[band].normalized_amplitude_spectrum() as f32;
            }
        }

        &self.levels
    }
}

// ─── Sliding DFT — With Moving Average ────────────────────────────────────────

pub struct SlidingDFT {
    bins: Vec<DFTBin>,
    levels: Vec<f32>,
    ring_buffer: RingBuffer,
    moving_average: Option<Box<dyn MovingAverage>>,
}

impl SlidingDFT {
    /// Creates a new SlidingDFT.
    /// `max_average_window_in_seconds`: positive → HeavyMovingAverage, negative → FastMovingAverage, zero → disabled.
    pub fn new(tuning: &PianoTuning, max_average_window_in_seconds: f64) -> Self {
        let bands = tuning.bands;
        let sample_rate = tuning.sample_rate;
        let mut bins = Vec::with_capacity(bands);
        let mapping = &tuning.mapping_cache;
        let mut max_n = 0u32;

        for band in mapping.iter() {
            bins.push(DFTBin::new(band.k, band.n));
            if band.n > max_n {
                max_n = band.n;
            }
        }

        let moving_average: Option<Box<dyn MovingAverage>> = if max_average_window_in_seconds > 0.0 {
            Some(Box::new(HeavyMovingAverage::new(
                bands as u32,
                sample_rate,
                (sample_rate as f64 * max_average_window_in_seconds).round() as u32,
            )))
        } else if max_average_window_in_seconds < 0.0 {
            Some(Box::new(FastMovingAverage::new(bands as u32, sample_rate)))
        } else {
            None
        };

        Self {
            bins,
            levels: vec![0.0; bands],
            ring_buffer: RingBuffer::new(max_n),
            moving_average,
        }
    }

    /// Process a batch of samples with an optional average window override.
    pub fn process(&mut self, samples: &[f32], average_window_in_seconds: f64) -> &[f32] {
        if let Some(ref mut ma) = self.moving_average {
            ma.set_average_window_in_seconds(average_window_in_seconds as f32);
        }

        let bins_num = self.bins.len();

        for i in 0..samples.len() {
            let current_sample = samples[i];
            self.ring_buffer.write(current_sample);

            for band in 0..bins_num {
                let previous_sample = self.ring_buffer.read(self.bins[band].n());
                self.bins[band].update(previous_sample as f64, current_sample as f64);
                self.levels[band] = self.bins[band].normalized_amplitude_spectrum() as f32;
            }

            if let Some(ref mut ma) = self.moving_average {
                ma.update(&self.levels);
            }
        }

        // Snapshot after smoothing
        if let Some(ref ma) = self.moving_average {
            if ma.average_window_in_seconds() > 0.0 {
                for band in 0..bins_num {
                    self.levels[band] = ma.read(band);
                }
            }
        }

        &self.levels
    }
}
