# Fencing Scoring Box

### Intro
This is a DIY fencing scoring box compliant with FIE timings. I have included the circuit diagram and source code. The "box" to hold the circuits is just a wooden toolbox with some holes cut into it with some wooden boards attached to it. The LED strip is attached to the top board. However, I do not know much about circuit design, so if you have any questions, reach out to me in the contact section!

### Repo description
Main source code is under /src. Circuit design is under /circuit. The other folders are just remnants of test code.

### Dev notes
We use a Pico Pi board because it's cheaper than an Arduino, although you can also just use an Arduino. The external ADC we use is also faster than the Arduino's.

We use multicore because signalling the LED strip via ws2812 is slow! We push all the LED settings to the other core and let it handle everything; when we want to reset, we block until it completes.
- Without multicore: approx 1000 microseconds
- With multicore: approx 100 microseconds

The ADC we use is the MCP3008. We need to use an external ADC because the Pico ADC only ha 3 channels available. It's also very fast! We go through all 6 channels in approx 61-62 microseconds. We talk to it via SPI0. It must be 3V because the SPI pins only take 3V and I didn't want to buy a logic level converter. 5V should be more accurate though.

ADC values: with 10 bit accuracy, the max value is 1024. At rest in foil, it will be around 512. Using a 1k resistor is enough to pull down/up the pins to be approximately accurate; using 220 ohm wasn't enough.

When we printf(), it takes about 125 microseconds, which can be enabled by setting the DEBUG define to true.

Timings are set in microseconds according to the FIE regulations: https://ncaaorg.s3.amazonaws.com/championships/sports/fencing/rules/PRXFE_USAFencingRules.pdf

### Bill of Materials
For the circuit:
| Part                | Quant | Cost  | Source                                       | Notes                                                                      |
|---------------------|-------|-------|----------------------------------------------|----------------------------------------------------------------------------|
| Pico                | 1     | 4.99  | Microcenter                                  | Don't need Wifi/BT, if you want to solder headers yourself get it for 3.99 |
| Wires               | 1     | 12.99 | https://www.amazon.com/dp/B08B8XD5SB         | Solid core is easier to work with, assumes you have the tools              |
| Circuit board       | 1     | 11.99 | https://www.amazon.com/gp/product/B07LF7N5K9 | You can go the custom route, but more expensive                            |
| MCP3008             | 1     | 8.00  | https://www.amazon.com/gp/product/B00NAY3RB2 | Any ADC alternative can work                                               |
| Buzzer              | 1     | 6.98  | https://www.amazon.com/gp/product/B07VK1GJ9X | Used active buzzers since you don't need PWM, use 5V to be louder          |
| Transistor          | 1     | 1.50  | Leftover parts, Microcenter probably has it  | NPN to turn on the buzzer with 3V                                          |
| 5V power supply     | 1     | 6.99  | https://www.amazon.com/gp/product/B09W8X9VGK | Pick one with a receptor plug to put in the box                            |
| 220 Resistor        | 6     | 1.98  | Leftover parts, Microcenter probably has it  | For 7 segment display                                                      |
| 1k Resistor         | 6     | 1.98  | Leftover parts, Microcenter probably has it  | For weapon lines                                                           |
| 8 segment display   | 1     | 6.49  | Leftover parts, Microcenter probably has it  | For weapon mode, 7 segment also works                                      |
| Power switch        | 1     | 2.49  | Microcenter                                  | Anything that is on/off works                                              |
| Push button         | 1     | 2.79  | Microcenter                                  | Anything that will release on push                                         |
| Banana female plugs | 6     | -     | Taken from broken parts from the club        | For cord plugs                                                             |

For me, the total cost is about $60, since I had some parts already. You also get a lot of leftover parts when ordering from Amazon.

For the box, I used scrap wood and a leftover toolbox from Home Depot. I cut some holes in the box for plugs and wires, but feel free to do it however you want. You can find the exact toolbox here: https://www.homedepot.com/p/Houseworks-Tool-Box-Wood-Kit-94501/100658845

### Contact
Reach out to me at marcusd.9234@gmail.com if you have any questions.

### Credits
Inspiration for most of the code and circuitry are from: https://github.com/wnew/fencing_scoring_box/blob/master/hardware/circuit