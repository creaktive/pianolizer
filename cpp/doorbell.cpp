/*
 * g++ -Ofast -std=c++14 -o doorbell doorbell.cpp -lasound -lcurl -pthread
 */
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include <alsa/asoundlib.h>
#include <curl/curl.h>

#include "pianolizer.hpp"

#define SAMPLE_RATE     8000  // 8 kHz is enough for doorbells
#define BUFFER_SIZE     64    // 8 ms latency at 8 kHz
#define BANDWIDTH       5.    // Hz
#define AVERAGE_WINDOW  .05   // seconds
#define SENSITIVITY     .1    // threshold for detection
#define COOLDOWN        10    // seconds
#define PUSHSAFER_KEY   "<YOUR_PRIVATE_KEY>"

using namespace std;

class DoorbellTuning : public Tuning {
  public:
    DoorbellTuning(
      const unsigned sampleRate_
    ) : Tuning{ sampleRate_, 2 }
    {}

    const vector<tuningValues> mapping() {
      return {
        frequencyAndBandwidthToKAndN(730., BANDWIDTH),
        frequencyAndBandwidthToKAndN(977., BANDWIDTH),
      };
    }
};

volatile sig_atomic_t alarmTriggered = 0;
volatile sig_atomic_t shouldExit = 0;

void alarmHandler(int sig) {
  alarmTriggered = 0;
}

void alarmReset() {
  alarmTriggered = 1;
  alarm(0);
  alarm(COOLDOWN);
}

void signalHandler(int sig) {
  shouldExit = 1;
}

void pushNotification(const string message) {
  cerr << message << endl;

  thread([message]() {
    CURL *curl;
    CURLcode res;

    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, "https://www.pushsafer.com/api");

    struct curl_httppost *formpost = NULL;
    struct curl_httppost *lastptr = NULL;

    // Private key
    curl_formadd(&formpost, &lastptr,
      CURLFORM_COPYNAME, "k",
      CURLFORM_COPYCONTENTS, PUSHSAFER_KEY,
      CURLFORM_END);

    // Critical priority
    curl_formadd(&formpost, &lastptr,
      CURLFORM_COPYNAME, "pr",
      CURLFORM_COPYCONTENTS, "2",
      CURLFORM_END);

    // Message
    curl_formadd(&formpost, &lastptr,
      CURLFORM_COPYNAME, "m",
      CURLFORM_COPYCONTENTS, message.c_str(),
      CURLFORM_END);

    curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

    res = curl_easy_perform(curl);

    curl_formfree(formpost);
    curl_easy_cleanup(curl);
  }).detach();
}

void monitorDoorbell() {
  // pushNotification("TEST");

  // Sliding DFT setup
  auto sdft = SlidingDFT(make_shared<DoorbellTuning>(SAMPLE_RATE), -AVERAGE_WINDOW);

  size_t len;
  vector<float> input(BUFFER_SIZE);
  const float *output = nullptr;

  // ALSA setup
  snd_pcm_t *handle;
  snd_pcm_hw_params_t *params;
  int err;

  if ((err = snd_pcm_open(&handle, "plug:dsnoop", SND_PCM_STREAM_CAPTURE, 0)) < 0)
    throw runtime_error("Cannot open audio device: " + string(snd_strerror(err)));

  snd_pcm_hw_params_alloca(&params);
  snd_pcm_hw_params_any(handle, params);
  snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_FLOAT_LE);
  snd_pcm_hw_params_set_channels(handle, params, 1);
  snd_pcm_hw_params_set_rate(handle, params, SAMPLE_RATE, 0);

  if ((err = snd_pcm_hw_params(handle, params)) < 0)
    throw runtime_error("Cannot set hardware parameters: " + string(snd_strerror(err)));

  snd_pcm_prepare(handle);

  // Signal handling
  signal(SIGALRM, alarmHandler);
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  while (!shouldExit) {
    if ((err = snd_pcm_readi(handle, input.data(), BUFFER_SIZE)) != BUFFER_SIZE) {
      if (err < 0) {
        err = snd_pcm_recover(handle, err, 0);
        if (err < 0)
          throw runtime_error("Read error: " + string(snd_strerror(err)));
      }
      continue;
    }

    if ((output = sdft.process(input.data(), BUFFER_SIZE, AVERAGE_WINDOW)) == nullptr)
      throw runtime_error("sdft.process() returned nothing");

    if (alarmTriggered) continue;

    // cout << fixed << setprecision(3) << output[0] << "\t" << output[1] << endl;
    if (output[0] >= SENSITIVITY) {
      alarmReset();
      pushNotification("DOWNSTAIRS DOORBELL");
    } else if (output[1] >= SENSITIVITY) {
      alarmReset();
      pushNotification("UPSTAIRS DOORBELL");
    }
  }

  snd_pcm_close(handle);
  alarm(0);
}

int main(int argc, char *argv[]) {
  try {
    monitorDoorbell();
  } catch (const exception& e) {
    cerr << "ERROR: " << e.what() << endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
