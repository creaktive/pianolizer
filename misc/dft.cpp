#define _USE_MATH_DEFINES
#include <cmath>
#include <complex>
#include <iostream>
#include <vector>

using namespace std;

const complex<double> discreteFourierTransform (
  const vector<complex<double>>& x,
  const double k, const double N
) {
  const double q = 2. * M_PI * k / N;
  complex<double> Xk = complex<double>(0., 0.);
  for (unsigned n = 0; n < N; n++)
    Xk += x[n] * complex<double>(cos(q * n), -sin(q * n));
  return Xk;
}

int main() {
  vector<complex<double>> signal;
  for (unsigned i = 0; i < 2000; i++)
    signal.push_back(complex<double>(
      sin(M_PI / 50. * i),    // sine wave, 441Hz
      0.
    ));

  const double sampleRate = 44100.;
  const double frequency = 441.;
  const double bandwidth = 26.;

  const double k = frequency / bandwidth;
  const double N = sampleRate / bandwidth;
  const double magnitude = sqrt(norm(discreteFourierTransform(signal, k, N)));

  cout << k << endl;          // 16.9615 
  cout << N << endl;          // 1696.15
  cout << magnitude << endl;  // 849.945

  return EXIT_SUCCESS;
}