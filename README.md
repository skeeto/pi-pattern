# Pi Pattern Searcher

This program allows for quickly searching for sequences of digits in
pi. This is achieved by generating a custom index on the digits.

You'll need to provide a source of digits in the form of ASCII/UTF-8,
such as the one listed here (pi-billion.txt):

* https://stuff.mit.edu/afs/sipb/contrib/pi/

## Usage

    $ ./pipattern 12345678
    186557266: 1234567805029135
    295024271: 1234567855807249
    328286393: 1234567869790828
    501918952: 1234567807946746
    516776780: 1234567832726891
    523551502: 1234567892248644
    691782818: 1234567807127372
    773349079: 1234567895949720
    956753746: 1234567870553515

The initial run will build the index (4GB for 1 billion digits) as
needed before performing the search. Subsequent runs will be very
fast. For the index to be completed in any reasonable about of time,
**you'll need at least 6GB of RAM** for indexing 1 billion digits.
Otherwise it will thrash your computer and take a very long time.

This utility was inspired by the [Pi-Search
Page](http://www.angio.net/pi/piquery.html).
