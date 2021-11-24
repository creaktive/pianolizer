#include <iostream>
#include "pianolizer.hpp"

void testRingBuffer() {
  RingBuffer *rb = new RingBuffer(15);
  assert(rb != NULL);

  // is it initialized to zero?
  assert(rb->read(0) == 0); 

  // write one single element
  rb->write(1);
  assert(rb->read(0) == 1);
  assert(rb->read(1) == 0);

  // write 9 more elements
  for (unsigned i = 2; i < 10; i++)
    rb->write(i);

  // read in sequence
  for (unsigned i = 0; i < 10; i++)
    assert(rb->read(9 - i) == i);

  // overflow
  for (unsigned i = 10; i < 20; i++)
    rb->write(i);

  // both the head and the tail are expected values
  assert(rb->read(0) == 19);
  assert(rb->read(15) == 4);

  // reading beyond the capacity of ring buffer wraps it around
  assert(rb->read(16) == 19);
  assert(rb->read(17) == 18);
}

int main(int argc, char *argv[]) {
  testRingBuffer();

  return 0;
}