/*
 * g++ -o dft dft.cpp
 */
#define _USE_MATH_DEFINES
#include <cassert>
#include <cmath>
#include <complex>
#include <vector>

using namespace std;

const complex<double> discreteFourierTransform (
  const vector<complex<double>>& x,
  const double k, const unsigned N
) {
  if (x.size() < N)
    throw invalid_argument("x vector should have at least N samples");
  const double q = 2. * M_PI * k / N;
  complex<double> Xk = complex<double>(0., 0.);
  for (unsigned n = 0; n < N; n++)
    Xk += x[n] * complex<double>(cos(q * n), -sin(q * n));
  return Xk;
}

int main() {
  vector<complex<double>> signal;
  for (unsigned i = 0; i < 3000; i++)
    signal.push_back(complex<double>(
      sin(M_PI / 50. * i),    // sine wave, 441Hz
      0.
    ));

  const double sampleRate = 44100;
  const double frequency = 441;
  const double bandwidth = 21;

  const double k = frequency / bandwidth;
  const double N = sampleRate / bandwidth;
  auto dft = discreteFourierTransform(signal, k, N);
  const double magnitude = sqrt(norm(dft));

  assert(k == 21);
  assert(N == 2100);
  assert(round(dft.imag()) == -1050);
  assert(round(magnitude) == 1050);

  return EXIT_SUCCESS;
}
