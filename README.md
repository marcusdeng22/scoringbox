# Fencing Scoring Box

### Dev notes
Use multicore because signalling the LED strip via ws2812 is slow! We push all LED settings to the other core and let it handle everything; when we want to reset, we block until it completes.
Without multicore: approx 1000 microseconds
With multicore: approx 100 microseconds

The ADC we use is the MCP3008. We need to use an external ADC because the Pico ADC only ha 3 channels available. It's also very fast! We go through all 6 channels in approx 61-62 microseconds. We talk to it via SPI0.

ADC values: with 10 bit accuracy, the max value is 1024. At rest in foil, it will be around 512. Using a 1k resistor is enough to pull down/up the pins to be approximately accurate; using 220 ohm wasn't enough.

When we printf(), it takes about 125 microseconds.