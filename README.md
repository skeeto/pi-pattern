# Pi Pattern Searcher

This program allows for quickly searching for sequences of digits in
pi. The digits of pi are indexed in SQLite for quick searches.

You'll need to provide a source of digits in the form of UTF-8, such
as the one listed here (pi-billion.txt):

* https://stuff.mit.edu/afs/sipb/contrib/pi/

## Usage

    $ ./pipattern 123456

The initial run will build the index (multiple gigabytes for 1 billion
digits!) as needed before performing the search. Subsequent runs will
be nearly instantaneous.

This utility was inspired by the [Pi-Search
Page](http://www.angio.net/pi/piquery.html).
