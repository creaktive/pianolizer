/*
 * g++ -Ofast -std=c++14 -o doorbell doorbell.cpp -lasound -lcurl -pthread
 */
#include <iomanip>
#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#ifndef DEBUG
#include <alsa/asoundlib.h>
#include <curl/curl.h>
#endif

#include "pianolizer.hpp"

#define SAMPLE_RATE     8000  // 8 kHz is enough for doorbells
#define BUFFER_SIZE     64    // 8 ms latency at 8 kHz
#define BANDWIDTH       5.    // Hz
#define AVERAGE_WINDOW  1.    // seconds - capture full melody pattern across bursts
#define SENSITIVITY     .1    // threshold for detection
#define COOLDOWN        10    // seconds
#define PUSHSAFER_KEY   "<YOUR_PRIVATE_KEY>"

using namespace std;

class DoorbellTuning : public Tuning {
public:
  DoorbellTuning(const unsigned sampleRate_) : Tuning{sampleRate_, 4} {}

  const vector<tuningValues> mapping() {
    return {
      frequencyAndBandwidthToKAndN(570., BANDWIDTH), // downstairs, low
      frequencyAndBandwidthToKAndN(732., BANDWIDTH), // downstairs, high
      frequencyAndBandwidthToKAndN(489., BANDWIDTH), // upstairs, low
      frequencyAndBandwidthToKAndN(978., BANDWIDTH), // upstairs, high
    };
  }
};

volatile sig_atomic_t alarmTriggered = 0;
volatile sig_atomic_t shouldExit = 0;

void alarmHandler(int sig) { alarmTriggered = 0; }

void alarmReset() {
  alarmTriggered = 1;
  alarm(0);
  alarm(COOLDOWN);
}

void signalHandler(int sig) { shouldExit = 1; }

class AudioSource {
public:
  virtual ~AudioSource() = default;
  virtual ssize_t read(float *buffer, size_t frames) = 0;
};

#ifndef DEBUG
class AlsaSource : public AudioSource {
private:
  snd_pcm_t *handle;

public:
  AlsaSource(const char *device) {
    int err;
    snd_pcm_hw_params_t *params;

    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_CAPTURE, 0)) < 0)
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
  }

  ~AlsaSource() override { snd_pcm_close(handle); }

  ssize_t read(float *buffer, size_t frames) override {
    int err = snd_pcm_readi(handle, buffer, frames);
    if (err < 0) {
      err = snd_pcm_recover(handle, err, 0);
      if (err < 0)
        throw runtime_error("Read error: " + string(snd_strerror(err)));
      return 0;
    }
    return err;
  }
};

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
    curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "k", CURLFORM_COPYCONTENTS, PUSHSAFER_KEY, CURLFORM_END);

    // Critical priority
    curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "pr", CURLFORM_COPYCONTENTS, "2", CURLFORM_END);

    // Message
    curl_formadd(&formpost, &lastptr, CURLFORM_COPYNAME, "m", CURLFORM_COPYCONTENTS, message.c_str(), CURLFORM_END);

    curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

    res = curl_easy_perform(curl);

    curl_formfree(formpost);
    curl_easy_cleanup(curl);
  }).detach();
}
#else
class FileSource : public AudioSource {
private:
  FILE *fp;

public:
  explicit FileSource(const char *path) {
    fp = fopen(path, "rb");
    if (!fp) throw runtime_error("Cannot open audio file: " + string(path));
  }

  ~FileSource() override { fclose(fp); }

  ssize_t read(float *buffer, size_t frames) override {
    return fread(buffer, sizeof(float), frames, fp);
  }
};

void pushNotification(const string message) {
  cerr << message << endl;
  shouldExit = 1;
}
#endif

void monitorDoorbell(AudioSource &source) {
  // pushNotification("TEST");

  // Sliding DFT setup
  auto sdft = SlidingDFT(make_shared<DoorbellTuning>(SAMPLE_RATE), -AVERAGE_WINDOW);

  ssize_t len;
  vector<float> input(BUFFER_SIZE);
  const float *output = nullptr;

  // Signal handling
  signal(SIGALRM, alarmHandler);
  signal(SIGINT, signalHandler);
  signal(SIGTERM, signalHandler);

  while (!shouldExit) {
    if ((len = source.read(input.data(), BUFFER_SIZE)) != BUFFER_SIZE) {
      if (len == 0) break;
      if (len < 0) throw runtime_error("Read returned error");
      continue;
    }

    if ((output = sdft.process(input.data(), BUFFER_SIZE, AVERAGE_WINDOW)) == nullptr)
      throw runtime_error("sdft.process() returned nothing");

    if (alarmTriggered) continue;

    float d_bins = output[0] + output[1];  // 568 + 730 Hz - downstairs melody
    float u_bins = output[2] + output[3];  // 489 + 978 Hz - upstairs melody

    if (d_bins >= SENSITIVITY && d_bins > u_bins) {
      alarmReset();
      pushNotification("DOWNSTAIRS DOORBELL");
    } else if (u_bins >= SENSITIVITY && u_bins > d_bins) {
      alarmReset();
      pushNotification("UPSTAIRS DOORBELL");
    }
  }

  alarm(0);
}

int main(int argc, char *argv[]) {
#ifndef DEBUG
  const char *device = (argc > 1) ? argv[1] : "plug:dsnoop";
  AlsaSource source(device);
#else
  const char *file = (argc > 1) ? argv[1] : "doorbell.raw";
  FileSource source(file);
#endif

  try {
    monitorDoorbell(source);
  } catch (const exception &e) {
    cerr << "ERROR: " << e.what() << endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
