# psmove-pair-win

A command-line tool for establishing a Bluetooth connection between a PlayStation Move Motion Controller and a Bluetooth radio on Windows.

To the best of my knowledge, the only tools that manage a painless connection process of the Move on Windows use some kind of custom drivers. A lot of people have gotten a Bluetooth connection to work on Windows without these tools, usually in exhausting trial and error sessions. But it always seems to be a matter of the correct timing, or the altitude of the sun, or who knows what.

This program manages to reliably establish the Bluetooth connection without using custom drivers or requiring special timing. It is, unfortunately, not a "one-click" solution though.


## Building the project

I wrote this using Visual Studio 2010 and only tested the 32-bit build. If you are using the same setup, this should work out of the box.

Builds using [MinGW-w64][4] 4.9.1 have also been tested successfully using the following:

```
g++ -std=c++11 -o psmove-pair-win.exe main.cpp -lbthprops -luuid -DUNICODE
```


## Prerequisites

To concentrate on the actual connection issue, the program is intentionally held simple. It assumes the following setup:

1. The Move Motion Controller is already paired to the Bluetooth radio, i.e. the radio's BDADDR is programmed into the controller. You can use [PS Move API][1]'s `psmovepair` utility if you do not know how to do this.
2. The controller is not already connected (neither via USB nor via Bluetooth).


## Usage

The program will first detect available Bluetooth radios. If you have more than one radio connected, you are prompted to choose one of them. It is probably less hassle to just disconnect the unwanted radios instead.

The program will then search for Bluetooth devices and output a list of its findings. For every Move Motion Controller, it will attempt to establish a connection. Press the PS button on the controller. This should make the Move's status LED blink. If it stops blinking, press again. Repeat this until the LED finally stays lit.

Do not pay too much attention to the text output. Concentrate on the Move's status LED instead. Near the end, the program will usually detect a successful connection even though the LED is still blinking or went off again. Just keep pressing that PS button, you are almost there!

Note that sometimes the LED stays lit for some seconds (hooray!) and then goes off again (booh!). You guessed it: press the PS button again. The LED will usually stay lit the second or third try after that and the program will keep reporting a successful connection.

Also note that the connection procedure is running in an endless loop, i.e. it will not stop even if the connection has successfully been established. Press Ctrl+C to exit.


## Supported platforms

The program has been successfully tested on the following platforms:

- Windows XP SP3
- Windows 7 SP1
- Windows 8.1 (x86 and x64)


## Contact

Please let me know about your experience with the program! Either by reporting an issue on this project's [GitHub page][2] or by sending an email to the [psmove mailing list][3].


[1]: https://github.com/thp/psmoveapi
[2]: https://github.com/nitsch/psmove-pair-win
[3]: https://groups.google.com/forum/#!aboutgroup/psmove
[4]: http://sourceforge.net/projects/mingw-w64/


